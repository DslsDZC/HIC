/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC域管理实现
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#include "domain.h"
#include "capability.h"
#include "pmm.h"
#include "pagetable.h"
#include "formal_verification.h"
#include "audit.h"
#include "monitor.h"
#include "console.h"
#include "lib/mem.h"
#include "hal.h"

/* 外部引用：内核代码段地址范围 */
extern u8 _text_start[];
extern u8 _text_end[];
extern u8 _rodata_start[];
extern u8 _rodata_end[];
extern u8 _data_start[];
extern u8 _data_end[];

/* 全局域表 */
domain_t g_domains[MAX_DOMAINS];
static u32 g_domain_count = 0;

/* 域系统初始化 */
void domain_system_init(void)
{
    console_puts("[Domain] Starting domain system initialization...\n");
    
    console_puts("[Domain] Step 1: Initializing domain table...\n");
    memzero(g_domains, sizeof(g_domains));
    console_puts("[Domain] Domain table cleared (");
    console_putu32(MAX_DOMAINS);
    console_puts(" slots)\n");
    
    g_domain_count = 0;
    
    console_puts("[Domain] Step 2: Marking all domains as INIT state...\n");
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        g_domains[i].domain_id = i;
        g_domains[i].state = DOMAIN_STATE_INIT;
        g_domains[i].flags = 0;
        g_domains[i].parent_domain = HIC_INVALID_DOMAIN;
    }
    console_puts("[Domain] All ");
    console_putu32(MAX_DOMAINS);
    console_puts(" domains marked as INIT\n");
    
    console_puts("[Domain] Step 3: Creating Core-0 domain...\n");
    domain_quota_t core_quota = {
        .max_memory = 0x100000,      /* 1MB */
        .max_threads = 16,
        .max_caps = 1024,
        .cpu_quota_percent = 100
    };
    
    console_puts("[Domain] Core-0 quota: 1MB memory, 16 threads, 1024 caps, 100% CPU\n");
    
    domain_id_t core_domain;
    hic_status_t status = domain_create(DOMAIN_TYPE_CORE, HIC_INVALID_DOMAIN, &core_quota, &core_domain);
    
    if (status == HIC_SUCCESS) {
        console_puts("[Domain] >>> Core-0 domain created successfully! <<<\n");
        console_puts("[Domain] Core-0 domain_id: ");
        console_putu32(core_domain);
        console_puts("\n");
        g_domain_count = 1;
    } else {
        console_puts("[Domain] WARNING: Failed to create Core-0 domain\n");
        console_puts("[Domain] Status: ");
        console_putu32(status);
        console_puts("\n");
        g_domain_count = 1;  // 仍然计数，即使创建失败
    }
    
    console_puts("[Domain] Step 4: Finalizing domain system...\n");
    console_puts("[Domain] Domain system initialized\n");
    console_puts("[Domain] Total domains: ");
    console_putu32(g_domain_count);
    console_puts("\n");
    console_puts("[Domain] >>> Domain system is now READY <<<\n");
}

/**
 * 创建域
 */
hic_status_t domain_create(domain_type_t type, domain_id_t parent,
                           const domain_quota_t *quota, domain_id_t *out)
{
    /* 查找空闲域ID */
    domain_id_t domain_id = HIC_INVALID_DOMAIN;
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        if (g_domains[i].state == DOMAIN_STATE_INIT) {
            domain_id = i;
            break;
        }
    }
    
    if (domain_id == HIC_INVALID_DOMAIN) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    /* 分配能力空间 */
    domain->cap_capacity = quota->max_caps;
    phys_addr_t cap_space_phys;
    u32 cap_pages = (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    if (pmm_alloc_frames(HIC_DOMAIN_CORE, cap_pages, PAGE_FRAME_CORE, &cap_space_phys) != HIC_SUCCESS) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 将物理地址映射到虚拟地址 */
    /* 使用直接映射（虚拟地址 = 物理地址） */
    domain->cap_space = (cap_handle_t *)cap_space_phys;
    if (!domain->cap_space) {
        pmm_free_frames(cap_space_phys, cap_pages);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 分配物理内存 */
    phys_addr_t mem_base;
    size_t mem_size = quota->max_memory;
    if (pmm_alloc_frames(domain_id, (u32)((mem_size + PAGE_SIZE - 1) / PAGE_SIZE),
                         PAGE_FRAME_PRIVILEGED, &mem_base) != HIC_SUCCESS) {
        pmm_free_frames((phys_addr_t)domain->cap_space,
                        (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE));
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 初始化域 */
    domain->domain_id = domain_id;
    domain->type = type;
    domain->state = DOMAIN_STATE_READY;
    domain->phys_base = mem_base;
    domain->phys_size = mem_size;
    domain->cap_count = 0;
    domain->thread_list = 0;
    domain->thread_count = 0;
    domain->quota = *quota;
    domain->usage.memory_used = 0;
    domain->usage.thread_used = 0;
    domain->cpu_time_total = 0;
    domain->syscalls_total = 0;
    domain->flags = 0;
    domain->parent_domain = parent;
    
    /* 创建独立页表 */
    if (type == DOMAIN_TYPE_CORE) {
        /* Core-0 使用内核页表（共享地址空间） */
        domain->page_table = 0;  /* 使用当前CR3（内核页表） */
        domain->flags |= DOMAIN_FLAG_TRUSTED;
        console_puts("[Domain] Core-0 domain uses kernel page table\n");
    } else {
        /* 其他域创建独立页表 */
        page_table_t *domain_pagetable = pagetable_create();
        if (domain_pagetable == NULL) {
            console_puts("[Domain] ERROR: Failed to create page table for domain\n");
            pmm_free_frames(mem_base, (u32)((mem_size + PAGE_SIZE - 1) / PAGE_SIZE));
            pmm_free_frames((phys_addr_t)domain->cap_space,
                            (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE));
            return HIC_ERROR_NO_MEMORY;
        }
        
        domain->page_table = (virt_addr_t)domain_pagetable;
        
        /* 映射域的物理内存到其虚拟地址空间 */
        /* 使用恒等映射（虚拟地址 = 物理地址） */
        hic_status_t map_status = pagetable_map(domain_pagetable, 
                                                (virt_addr_t)mem_base, 
                                                mem_base, 
                                                mem_size,
                                                PERM_RW, 
                                                MAP_TYPE_USER);
        if (map_status != HIC_SUCCESS) {
            console_puts("[Domain] ERROR: Failed to map domain memory\n");
            pagetable_destroy(domain_pagetable);
            pmm_free_frames(mem_base, (u32)((mem_size + PAGE_SIZE - 1) / PAGE_SIZE));
            pmm_free_frames((phys_addr_t)domain->cap_space,
                            (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE));
            return HIC_ERROR_NO_MEMORY;
        }
        
        console_puts("[Domain] Created independent page table for domain ");
        console_putu32(domain_id);
        console_puts(" at 0x");
        console_puthex64((u64)domain_pagetable);
        console_puts("\n");
        
        console_puts("[Domain] Mapped domain memory: 0x");
        console_puthex64(mem_base);
        console_puts(" - 0x");
        console_puthex64(mem_base + mem_size);
        console_puts("\n");
    }
    
    /* 特权域标记（Privileged-1 服务默认为特权域） */
    if (type == DOMAIN_TYPE_PRIVILEGED) {
        domain->flags |= DOMAIN_FLAG_PRIVILEGED;
        
        /* 设置运行时特权位图（增强安全） */
        extern void cap_set_privileged_domain(domain_id_t, bool);
        cap_set_privileged_domain(domain_id, true);
    }
    
    g_domain_count++;
    
    *out = domain_id;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[Domain] Invariant violation detected after domain_create!\n");
    }
    
    return HIC_SUCCESS;
}

/**
 * 销毁域
 */
hic_status_t domain_destroy(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state == DOMAIN_STATE_INIT ||
        domain->state == DOMAIN_STATE_TERMINATED) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* 回收所有能力 */
    for (u32 i = 0; i < domain->cap_count; i++) {
        cap_id_t cap_id = (cap_id_t)domain->cap_space[i];
        if (cap_id != HIC_CAP_INVALID) {
            cap_revoke(cap_id);
        }
    }
    
    /* 释放能力空间 */
    if (domain->cap_space) {
        pmm_free_frames((phys_addr_t)domain->cap_space,
                        (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE));
    }

    /* 释放物理内存 */
    if (domain->phys_base != 0) {
        pmm_free_frames(domain->phys_base, (u32)((domain->phys_size + PAGE_SIZE - 1) / PAGE_SIZE));
    }
    
    /* 销毁独立页表（非Core-0域） */
    if (domain->page_table != 0 && domain->type != DOMAIN_TYPE_CORE) {
        console_puts("[Domain] Destroying page table for domain ");
        console_putu32(domain_id);
        console_puts("\n");
        pagetable_destroy((page_table_t*)domain->page_table);
        domain->page_table = 0;
    }
    
    domain->state = DOMAIN_STATE_TERMINATED;
    g_domain_count--;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[Domain] Invariant violation detected after domain_destroy!\n");
    }
    
    return HIC_SUCCESS;
}

/**
 * 查询域信息
 */
hic_status_t domain_get_info(domain_id_t domain_id, domain_t *info)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state == DOMAIN_STATE_INIT) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    *info = *domain;
    return HIC_SUCCESS;
}

/**
 * 暂停域
 */
hic_status_t domain_suspend(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state != DOMAIN_STATE_RUNNING) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    domain->state = DOMAIN_STATE_SUSPENDED;
    return HIC_SUCCESS;
}

/**
 * 恢复域
 */
hic_status_t domain_resume(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    /* 允许从 READY 或 SUSPENDED 状态恢复 */
    if (domain->state != DOMAIN_STATE_SUSPENDED &&
        domain->state != DOMAIN_STATE_READY) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    domain->state = DOMAIN_STATE_RUNNING;
    return HIC_SUCCESS;
}

/**
 * 检查内存配额
 */
hic_status_t domain_check_memory_quota(domain_id_t domain_id, size_t size)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    console_puts("[Domain] Quota check: used=");
    console_putu64(domain->usage.memory_used);
    console_puts(", size=");
    console_putu64(size);
    console_puts(", max=");
    console_putu64(domain->quota.max_memory);
    console_puts(", total=");
    console_putu64(domain->usage.memory_used + size);
    console_puts("\n");
    
    if (domain->usage.memory_used + size > domain->quota.max_memory) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    return HIC_SUCCESS;
}

/**
 * 检查线程配额
 */
hic_status_t domain_check_thread_quota(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->usage.thread_used >= domain->quota.max_threads) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    return HIC_SUCCESS;
}

/* ============================================ */
/* 形式化验证接口实现 */
/* ============================================ */

/**
 * 检查域是否活跃
 */
bool domain_is_active(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return false;
    }
    
    domain_t *d = &g_domains[domain];
    return d->state == DOMAIN_STATE_RUNNING || d->state == DOMAIN_STATE_SUSPENDED;
}

/**
 * 统计域的能力数量
 */
u64 count_domain_capabilities(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *d = &g_domains[domain];
    return d->cap_count;
}

/**
 * 获取域的初始能力配额
 */
u64 get_domain_initial_cap_quota(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *d = &g_domains[domain];
    return d->quota.max_caps;
}

/**
 * 获取域被授予的能力数量
 */
u64 get_domain_granted_caps(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    /* 统计从其他域接收的能力 */
    domain_t *d = &g_domains[domain];
    u64 granted_count = 0;
    
    for (u32 i = 0; i < d->cap_count; i++) {
        cap_id_t cap_id = (cap_id_t)d->cap_space[i];
        if (cap_id != HIC_CAP_INVALID && cap_id < CAP_TABLE_SIZE) {
            /* 检查能力是否存在 */
            if (g_global_cap_table[cap_id].cap_id == cap_id) {
                granted_count++;
            }
        }
    }
    
    return granted_count;
}

/**
 * 获取域被撤销的能力数量
 */
u64 get_domain_revoked_caps(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    /* 统计已撤销的能力 */
    u64 revoked_count = 0;
    
    for (cap_id_t i = 0; i < CAP_TABLE_SIZE; i++) {
        if (g_global_cap_table[i].cap_id == i) {
            if (g_global_cap_table[i].owner == domain && (g_global_cap_table[i].flags & CAP_FLAG_REVOKED)) {
                revoked_count++;
            }
        }
    }
    
    return revoked_count;
}

/**
 * 获取域的内存区域
 */
mem_region_t get_domain_memory_region(domain_id_t domain)
{
    mem_region_t region = {0, 0};
    
    if (domain >= MAX_DOMAINS) {
        return region;
    }
    
    domain_t *d = &g_domains[domain];
    region.base = d->phys_base;
    region.size = d->phys_size;
    
    return region;
}

/**
 * 获取域已分配的内存
 */
u64 get_domain_allocated_memory(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *d = &g_domains[domain];
    return d->usage.memory_used;
}

/**
 * 获取域的CPU配额
 */
u64 get_domain_cpu_quota(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *d = &g_domains[domain];
    return d->quota.cpu_quota_percent;
}

/* ==================== 机制层：配额强制检查原语实现 ==================== */

/**
 * 检查配额是否充足
 */
quota_check_result_t domain_quota_check(domain_id_t domain_id, 
                                         quota_type_t type, 
                                         size_t amount,
                                         size_t *actual_available)
{
    if (domain_id >= MAX_DOMAINS) {
        return QUOTA_CHECK_EXCEEDED;
    }
    
    domain_t *domain = &g_domains[domain_id];
    size_t available = 0;
    size_t used = 0;
    size_t max = 0;
    
    switch (type) {
        case QUOTA_TYPE_MEMORY:
            used = domain->usage.memory_used;
            max = domain->quota.max_memory;
            break;
        case QUOTA_TYPE_THREADS:
            used = domain->usage.thread_used;
            max = domain->quota.max_threads;
            break;
        case QUOTA_TYPE_CAPABILITIES:
            used = domain->usage.cap_used;
            max = domain->quota.max_caps;
            break;
        case QUOTA_TYPE_CPU:
            used = (size_t)(domain->usage.cpu_time_used / 1000000); /* 微秒转毫秒 */
            max = (size_t)domain->quota.cpu_quota_percent * 10; /* 简化计算 */
            break;
    }
    
    available = (max > used) ? (max - used) : 0;
    
    if (actual_available) {
        *actual_available = available;
    }
    
    /* 检查是否超出 */
    if (amount > available) {
        return QUOTA_CHECK_EXCEEDED;
    }
    
    /* 计算使用率 */
    u32 percent = (u32)((used * 100) / (max > 0 ? max : 1));
    
    if (percent >= 95) {
        return QUOTA_CHECK_CRITICAL;
    } else if (percent >= 80) {
        return QUOTA_CHECK_WARNING;
    }
    
    return QUOTA_CHECK_OK;
}

/**
 * 强制消耗配额
 */
hic_status_t domain_quota_consume(domain_id_t domain_id, 
                                   quota_type_t type, 
                                   size_t amount)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    /* 先检查配额 */
    quota_check_result_t result = domain_quota_check(domain_id, type, amount, NULL);
    if (result == QUOTA_CHECK_EXCEEDED) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    /* 消耗配额 */
    switch (type) {
        case QUOTA_TYPE_MEMORY:
            domain->usage.memory_used += amount;
            /* 更新分配速率 */
            {
                u64 now = hal_get_timestamp();
                u64 elapsed = now - domain->usage.last_check_time;
                if (elapsed > 0 && elapsed < 1000000000ULL) { /* 1秒内 */
                    domain->usage.memory_alloc_rate = 
                        (amount * 1000000000ULL) / elapsed;
                }
                domain->usage.last_check_time = now;
            }
            break;
        case QUOTA_TYPE_THREADS:
            domain->usage.thread_used++;
            break;
        case QUOTA_TYPE_CAPABILITIES:
            domain->usage.cap_used++;
            break;
        case QUOTA_TYPE_CPU:
            domain->usage.cpu_time_used += amount;
            break;
    }
    
    return HIC_SUCCESS;
}

/**
 * 释放配额
 */
void domain_quota_release(domain_id_t domain_id, 
                          quota_type_t type, 
                          size_t amount)
{
    if (domain_id >= MAX_DOMAINS) {
        return;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    switch (type) {
        case QUOTA_TYPE_MEMORY:
            if (domain->usage.memory_used >= amount) {
                domain->usage.memory_used -= amount;
            } else {
                domain->usage.memory_used = 0;
            }
            break;
        case QUOTA_TYPE_THREADS:
            if (domain->usage.thread_used > 0) {
                domain->usage.thread_used--;
            }
            break;
        case QUOTA_TYPE_CAPABILITIES:
            if (domain->usage.cap_used > 0) {
                domain->usage.cap_used--;
            }
            break;
        case QUOTA_TYPE_CPU:
            /* CPU时间不释放 */
            break;
    }
}

/**
 * 创建子配额委托
 */
hic_status_t domain_quota_delegate(domain_id_t parent, 
                                    domain_id_t child,
                                    size_t memory_quota,
                                    u32 thread_quota)
{
    if (parent >= MAX_DOMAINS || child >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *parent_domain = &g_domains[parent];
    domain_t *child_domain = &g_domains[child];
    
    /* 检查父域是否有足够配额 */
    size_t available_memory = parent_domain->quota.max_memory - 
                              parent_domain->quota.memory_delegated -
                              parent_domain->usage.memory_used;
    
    u32 available_threads = parent_domain->quota.max_threads - 
                            parent_domain->quota.threads_delegated -
                            parent_domain->usage.thread_used;
    
    if (memory_quota > available_memory || thread_quota > available_threads) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    /* 记录委托 */
    parent_domain->quota.memory_delegated += memory_quota;
    parent_domain->quota.threads_delegated += thread_quota;
    
    /* 设置子域配额 */
    child_domain->quota.max_memory = memory_quota;
    child_domain->quota.max_threads = thread_quota;
    child_domain->parent_domain = parent;
    
    return HIC_SUCCESS;
}

/**
 * 获取资源使用率
 */
u32 domain_get_usage_percent(domain_id_t domain_id, quota_type_t type)
{
    if (domain_id >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *domain = &g_domains[domain_id];
    size_t used = 0, max = 1;
    
    switch (type) {
        case QUOTA_TYPE_MEMORY:
            used = domain->usage.memory_used;
            max = domain->quota.max_memory > 0 ? domain->quota.max_memory : 1;
            break;
        case QUOTA_TYPE_THREADS:
            used = domain->usage.thread_used;
            max = domain->quota.max_threads > 0 ? domain->quota.max_threads : 1;
            break;
        case QUOTA_TYPE_CAPABILITIES:
            used = domain->usage.cap_used;
            max = domain->quota.max_caps > 0 ? domain->quota.max_caps : 1;
            break;
        case QUOTA_TYPE_CPU:
            return (u32)domain->quota.cpu_quota_percent;
    }
    
    return (u32)((used * 100) / max);
}

/**
 * 获取资源分配速率
 */
u64 domain_get_allocation_rate(domain_id_t domain_id, quota_type_t type)
{
    if (domain_id >= MAX_DOMAINS) {
        return 0;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    switch (type) {
        case QUOTA_TYPE_MEMORY:
            return domain->usage.memory_alloc_rate;
        case QUOTA_TYPE_THREADS:
            return domain->usage.thread_create_rate;
        default:
            return 0;
    }
}

/* ==================== 机制层：紧急状态检测原语实现 ==================== */

/**
 * 获取系统资源状态
 */
void domain_get_system_status(system_resource_status_t *status)
{
    if (!status) return;
    
    memzero(status, sizeof(system_resource_status_t));
    
    /* 获取内存状态 */
    u64 total = total_memory();
    u64 used = used_memory();
    status->total_memory = total;
    status->used_memory = used;
    status->free_memory = (total > used) ? (total - used) : 0;
    status->free_percent = (u32)((status->free_memory * 100) / 
                                  (total > 0 ? total : 1));
    
    /* 统计域和线程 */
    status->domain_count = g_domain_count;
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        if (g_domains[i].state != DOMAIN_STATE_INIT) {
            status->thread_count += g_domains[i].thread_count;
            status->cap_count += g_domains[i].cap_count;
        }
    }
    
    /* 检测紧急状态 */
    status->level = domain_detect_emergency();
}

/**
 * 检测紧急状态
 */
emergency_level_t domain_detect_emergency(void)
{
    u64 total = total_memory();
    u64 used = used_memory();
    
    if (total == 0) {
        return EMERGENCY_LEVEL_NONE;
    }
    
    u64 free_mem = (total > used) ? (total - used) : 0;
    u32 free_percent = (u32)((free_mem * 100) / total);
    
    if (free_percent < 5) {
        return EMERGENCY_LEVEL_EMERGENCY;
    } else if (free_percent < 10) {
        return EMERGENCY_LEVEL_CRITICAL;
    } else if (free_percent < 20) {
        return EMERGENCY_LEVEL_WARNING;
    }
    
    return EMERGENCY_LEVEL_NONE;
}

/**
 * 触发紧急状态动作
 */
u32 domain_trigger_emergency_action(emergency_level_t level, bool exclude_critical)
{
    u32 affected = 0;
    
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        domain_t *domain = &g_domains[i];
        
        /* 跳过非活跃域 */
        if (domain->state != DOMAIN_STATE_RUNNING) {
            continue;
        }
        
        /* 跳过Core-0 */
        if (domain->type == DOMAIN_TYPE_CORE) {
            continue;
        }
        
        /* 跳过关键服务（如果要求） */
        if (exclude_critical && (domain->flags & DOMAIN_FLAG_CRITICAL)) {
            continue;
        }
        
        /* 根据紧急级别执行动作 */
        switch (level) {
            case EMERGENCY_LEVEL_WARNING:
                /* 警告：只通知，不暂停 */
                console_puts("[EMERGENCY] Warning for domain ");
                console_putu32(i);
                console_puts("\n");
                monitor_record_event(MONITOR_EVENT_RESOURCE_EXHAUSTED, i);
                break;
                
            case EMERGENCY_LEVEL_CRITICAL:
                /* 严重：暂停非关键应用 */
                if (domain->type == DOMAIN_TYPE_APPLICATION) {
                    console_puts("[EMERGENCY] Suspending application domain ");
                    console_putu32(i);
                    console_puts("\n");
                    domain_suspend(i);
                    affected++;
                }
                break;
                
            case EMERGENCY_LEVEL_EMERGENCY:
                /* 紧急：暂停所有非关键域 */
                if (!(domain->flags & DOMAIN_FLAG_CRITICAL)) {
                    console_puts("[EMERGENCY] Suspending domain ");
                    console_putu32(i);
                    console_puts("\n");
                    domain_suspend(i);
                    affected++;
                }
                break;
                
            default:
                break;
        }
    }
    
    return affected;
}

/* ==================== 机制层：原子性资源回收 ==================== */

/**
 * 原子性回收域的所有资源
 * 当域崩溃时调用，确保所有资源被完整回收
 */
hic_status_t domain_atomic_resource_reclaim(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    console_puts("[DOMAIN] Atomic resource reclaim for domain ");
    console_putu32(domain_id);
    console_puts("\n");
    
    /* 1. 暂停所有线程 */
    domain->state = DOMAIN_STATE_SUSPENDED;
    
    /* 2. 原子性撤销所有能力 */
    u32 caps_revoked = 0;
    for (u32 i = 0; i < domain->cap_count; i++) {
        cap_id_t cap_id = (cap_id_t)domain->cap_space[i];
        if (cap_id != HIC_CAP_INVALID) {
            cap_revoke(cap_id);
            caps_revoked++;
        }
    }
    console_puts("[DOMAIN] Revoked ");
    console_putu32(caps_revoked);
    console_puts(" capabilities\n");
    
    /* 3. 回收内存 */
    size_t memory_reclaimed = domain->usage.memory_used;
    console_puts("[DOMAIN] Reclaimed ");
    console_putu64(memory_reclaimed);
    console_puts(" bytes memory\n");
    
    /* 4. 清理线程 */
    u32 threads_terminated = domain->usage.thread_used;
    console_puts("[DOMAIN] Terminated ");
    console_putu32(threads_terminated);
    console_puts(" threads\n");
    
    /* 5. 重置使用统计 */
    domain->usage.memory_used = 0;
    domain->usage.thread_used = 0;
    domain->usage.cap_used = 0;
    
    /* 6. 记录审计事件 */
    u64 data[4] = { domain_id, caps_revoked, memory_reclaimed, threads_terminated };
    audit_log_event(AUDIT_EVENT_DOMAIN_DESTROY, domain_id, 0, 0, data, 4, 1);
    
    console_puts("[DOMAIN] Atomic resource reclaim complete\n");
    
    return HIC_SUCCESS;
}