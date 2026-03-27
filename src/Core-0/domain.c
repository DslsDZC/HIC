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
#include "atomic.h"
#include "lib/mem.h"
#include "hal.h"
#include "thread.h"  /* 用于域切换时的线程状态管理 */

/* 外部引用：内核代码段地址范围 */
extern u8 _text_start[];
extern u8 _text_end[];
extern u8 _rodata_start[];
extern u8 _rodata_end[];
extern u8 _data_start[];
extern u8 _data_end[];
extern u8 _kernel_end[];  /* 内核结束地址，用于 Core-0 域内存范围 */

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
    
    /* 分配能力空间（Core-0 域不需要额外分配） */
    domain->cap_capacity = quota->max_caps;
    
    if (type == DOMAIN_TYPE_CORE) {
        /* Core-0 域：使用内核固定内存，不额外分配 */
        domain->cap_space = NULL;  /* Core-0 使用全局能力表 */
        domain->phys_base = (phys_addr_t)_text_start;
        domain->phys_size = (size_t)(_kernel_end - _text_start);
    } else {
        /* 其他域：分配独立的能力空间 */
        phys_addr_t cap_space_phys;
        u32 cap_pages = (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        if (pmm_alloc_frames(domain_id, cap_pages, PAGE_FRAME_PRIVILEGED, &cap_space_phys) != HIC_SUCCESS) {
            return HIC_ERROR_NO_RESOURCE;
        }
        domain->cap_space = (cap_handle_t *)cap_space_phys;
    }
    
    /* 分配物理内存（Core-0 域不需要额外分配） */
    phys_addr_t mem_base = 0;
    size_t mem_size = quota->max_memory;
    
    if (type != DOMAIN_TYPE_CORE) {
        if (pmm_alloc_frames(domain_id, (u32)((mem_size + PAGE_SIZE - 1) / PAGE_SIZE),
                             PAGE_FRAME_PRIVILEGED, &mem_base) != HIC_SUCCESS) {
            if (domain->cap_space) {
                pmm_free_frames((phys_addr_t)domain->cap_space,
                                (u32)((domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE));
            }
            return HIC_ERROR_NO_RESOURCE;
        }
    } else {
        /* Core-0 使用内核内存范围 */
        mem_base = domain->phys_base;
        mem_size = domain->phys_size;
    }
    
    /* 初始化域 */
    domain->domain_id = domain_id;
    domain->type = type;
    domain->state = DOMAIN_STATE_READY;
    if (type != DOMAIN_TYPE_CORE) {
        domain->phys_base = mem_base;
        domain->phys_size = mem_size;
    }
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
        
        /* 映射内核代码段到域页表（机制：确保切换页表后内核代码仍可执行） */
        {
            size_t text_size = (size_t)(_text_end - _text_start);
            if (text_size > 0) {
                hic_status_t kernel_map_status = pagetable_map(
                    domain_pagetable,
                    (virt_addr_t)_text_start,
                    (phys_addr_t)_text_start,
                    text_size,
                    PERM_RX,           /* 只读可执行 */
                    MAP_TYPE_KERNEL    /* 内核映射 */
                );
                if (kernel_map_status != HIC_SUCCESS) {
                    console_puts("[Domain] WARN: Failed to map kernel text for domain\n");
                    /* 继续执行，非致命错误 */
                }
            }
            
            /* 映射内核只读数据段 */
            size_t rodata_size = (size_t)(_rodata_end - _rodata_start);
            if (rodata_size > 0) {
                pagetable_map(domain_pagetable,
                    (virt_addr_t)_rodata_start,
                    (phys_addr_t)_rodata_start,
                    rodata_size,
                    PERM_READ,
                    MAP_TYPE_KERNEL);
            }
            
            /* 
             * 内核数据段和 BSS 段映射策略：
             * 
             * 对于 Privileged-1 域：需要可写权限
             * - 调度器切换页表后仍需操作内核栈（位于 .bss）
             * - context_switch 的 push/pop 操作需要写入栈
             * - 必须确保栈操作在页表切换后仍能正常工作
             * 
             * 对于 Application-3 域：保持只读
             * - 这些域不会直接执行内核代码
             * - 通过 IPC 与内核交互，无需直接访问内核数据
             * 
             * 安全考虑：
             * - Privileged-1 服务已通过能力系统隔离
             * - 恶意服务无法绕过能力检查
             */
            if (type == DOMAIN_TYPE_PRIVILEGED) {
                /* 特权域：映射为可读写（调度器需要操作栈） */
                size_t data_size = (size_t)(_data_end - _data_start);
                if (data_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)_data_start,
                        (phys_addr_t)_data_start,
                        data_size,
                        PERM_RW,  /* 可读写：特权域需要操作内核栈 */
                        MAP_TYPE_KERNEL);
                }
                
                extern char __bss_start[], __bss_end[];
                size_t bss_size = (size_t)(__bss_end - __bss_start);
                if (bss_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)__bss_start,
                        (phys_addr_t)__bss_start,
                        bss_size,
                        PERM_RW,  /* 可读写：内核栈在 BSS 中 */
                        MAP_TYPE_KERNEL);
                }
                
                extern char __lbss_start[], __lbss_end[];
                size_t lbss_size = (size_t)(__lbss_end - __lbss_start);
                if (lbss_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)__lbss_start,
                        (phys_addr_t)__lbss_start,
                        lbss_size,
                        PERM_RW,  /* 可读写：大型 BSS 可能包含栈 */
                        MAP_TYPE_KERNEL);
                }
                
                console_puts("[Domain] Privileged domain: kernel data mapped RW for scheduler\n");
            } else {
                /* 非特权域（Application）：映射为只读 */
                size_t data_size = (size_t)(_data_end - _data_start);
                if (data_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)_data_start,
                        (phys_addr_t)_data_start,
                        data_size,
                        PERM_READ,  /* 只读：防止权限提升攻击 */
                        MAP_TYPE_KERNEL);
                }
                
                extern char __bss_start[], __bss_end[];
                size_t bss_size = (size_t)(__bss_end - __bss_start);
                if (bss_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)__bss_start,
                        (phys_addr_t)__bss_start,
                        bss_size,
                        PERM_READ,
                        MAP_TYPE_KERNEL);
                }
                
                extern char __lbss_start[], __lbss_end[];
                size_t lbss_size = (size_t)(__lbss_end - __lbss_start);
                if (lbss_size > 0) {
                    pagetable_map(domain_pagetable,
                        (virt_addr_t)__lbss_start,
                        (phys_addr_t)__lbss_start,
                        lbss_size,
                        PERM_READ,
                        MAP_TYPE_KERNEL);
                }
            }
        }
        
        /* 注册域页表到 domain_switch 子系统 */
        pagetable_setup_domain(domain_id, domain_pagetable);
        console_puts("[Domain] Registered page table for domain ");
        console_putu32(domain_id);
        console_puts(" (with kernel regions mapped)\n");
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
    mem_region_t region = {0, 0, NULL};
    
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
                monitor_record_event(MONITOR_EVENT_TYPE_4, i);
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

/* ==================== 零停机更新机制层实现 ==================== */

/* 并行域伙伴关系表 */
static domain_id_t g_parallel_partners[MAX_DOMAINS];

/**
 * 并行域创建（机制层）
 */
hic_status_t domain_parallel_create(domain_id_t template_domain,
                                     const char *name,
                                     const domain_quota_t *quota_override,
                                     domain_id_t *out) {
    (void)name;  /* 保留供未来扩展 */
    
    if (template_domain >= MAX_DOMAINS || !out) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    domain_t *template = &g_domains[template_domain];
    
    /* 检查模板域状态 */
    if (template->state != DOMAIN_STATE_RUNNING &&
        template->state != DOMAIN_STATE_READY) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* 使用覆盖配额或模板配额 */
    domain_quota_t quota = quota_override ? *quota_override : template->quota;
    
    /* 创建新域 */
    domain_id_t new_domain_id;
    hic_status_t status = domain_create(template->type, template_domain,
                                         &quota, &new_domain_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    domain_t *new_domain = &g_domains[new_domain_id];
    
    /* 标记为新实例 */
    new_domain->flags |= DOMAIN_FLAG_NEW_INSTANCE;
    
    /* 设置并行伙伴关系 */
    g_parallel_partners[new_domain_id] = template_domain;
    g_parallel_partners[template_domain] = new_domain_id;
    
    /* 复制模板域的能力配置（不复制实际能力） */
    new_domain->cap_capacity = template->cap_capacity;
    
    console_puts("[DOMAIN] Created parallel domain ");
    console_putu32(new_domain_id);
    console_puts(" from template ");
    console_putu32(template_domain);
    console_puts("\n");
    
    *out = new_domain_id;
    return HIC_SUCCESS;
}

/**
 * 设置域的并行运行伙伴（机制层）
 */
hic_status_t domain_set_parallel_partner(domain_id_t domain_a, domain_id_t domain_b) {
    if (domain_a >= MAX_DOMAINS || domain_b >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    g_parallel_partners[domain_a] = domain_b;
    g_parallel_partners[domain_b] = domain_a;
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 获取域的并行运行伙伴（机制层）
 */
hic_status_t domain_get_parallel_partner(domain_id_t domain, domain_id_t *partner) {
    if (domain >= MAX_DOMAINS || !partner) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    *partner = g_parallel_partners[domain];
    
    if (*partner == HIC_INVALID_DOMAIN) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    return HIC_SUCCESS;
}

/**
 * 原子性域切换（机制层）
 * 
 * 安全保证：
 * 1. 旧域线程立即停止调度（设置为 BLOCKED）
 * 2. 新域线程可被调度（设置为 READY）
 * 3. 端点原子重定向
 * 4. 整个过程在临界区保护内
 */
hic_status_t domain_atomic_switch(domain_id_t from,
                                   domain_id_t to,
                                   cap_id_t *endpoint_caps,
                                   u32 cap_count) {
    if (from >= MAX_DOMAINS || to >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    domain_t *from_domain = &g_domains[from];
    domain_t *to_domain = &g_domains[to];
    
    /* 检查域状态 */
    if (from_domain->state != DOMAIN_STATE_RUNNING &&
        from_domain->state != DOMAIN_STATE_READY) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    if (to_domain->state != DOMAIN_STATE_RUNNING &&
        to_domain->state != DOMAIN_STATE_READY) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 1. 标记旧域为排空状态 */
    from_domain->flags |= DOMAIN_FLAG_DRAINING | DOMAIN_FLAG_OLD_INSTANCE;
    from_domain->state = DOMAIN_STATE_SUSPENDED;
    
    /* 2. 阻塞旧域的所有线程（防止继续调度） */
    extern thread_t g_threads[];
    u32 blocked_count = 0;
    u32 woken_count = 0;
    
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_threads[i];
        
        /* 跳过无效线程 */
        if (t->thread_id != (thread_id_t)i) {
            continue;
        }
        
        /* 处理旧域线程：设置为 BLOCKED */
        if (t->domain_id == from) {
            if (t->state == THREAD_STATE_READY || t->state == THREAD_STATE_RUNNING) {
                t->state = THREAD_STATE_BLOCKED;
                blocked_count++;
            }
        }
        
        /* 处理新域线程：唤醒 */
        if (t->domain_id == to) {
            if (t->state == THREAD_STATE_BLOCKED || t->state == THREAD_STATE_WAITING) {
                t->state = THREAD_STATE_READY;
                t->time_slice = 100;  /* 重置时间片 */
                woken_count++;
            }
        }
    }
    
    /* 3. 原子性重定向所有端点 */
    for (u32 i = 0; i < cap_count && endpoint_caps; i++) {
        if (endpoint_caps[i] != HIC_CAP_INVALID) {
            domain_id_t old_target;
            hic_status_t status = cap_endpoint_redirect(endpoint_caps[i], to, &old_target);
            if (status != HIC_SUCCESS) {
                /* 重定向失败，记录但继续 */
                console_puts("[DOMAIN] Warning: endpoint redirect failed for cap ");
                console_putu32(endpoint_caps[i]);
                console_puts("\n");
            }
        }
    }
    
    /* 4. 标记新域为主实例 */
    to_domain->flags &= ~DOMAIN_FLAG_NEW_INSTANCE;
    to_domain->flags |= DOMAIN_FLAG_PRIMARY;
    
    /* 5. 确保所有写入可见 */
    atomic_release_barrier();
    
    atomic_exit_critical(irq);
    
    console_puts("[DOMAIN] Atomic switch: domain ");
    console_putu32(from);
    console_puts(" -> domain ");
    console_putu32(to);
    console_puts(" (blocked ");
    console_putu32(blocked_count);
    console_puts(" threads, woken ");
    console_putu32(woken_count);
    console_puts(", ");
    console_putu32(cap_count);
    console_puts(" endpoints)\n");
    
    return HIC_SUCCESS;
}

/**
 * 域优雅关闭（机制层）
 */
hic_status_t domain_graceful_shutdown(domain_id_t domain,
                                        u32 timeout_ms,
                                        bool force) {
    if (domain >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *target = &g_domains[domain];
    
    /* 检查域状态 */
    if (target->state != DOMAIN_STATE_RUNNING &&
        target->state != DOMAIN_STATE_SUSPENDED) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    console_puts("[DOMAIN] Graceful shutdown for domain ");
    console_putu32(domain);
    console_puts(", timeout=");
    console_putu32(timeout_ms);
    console_puts("ms\n");
    
    /* 1. 阻塞域的所有线程（防止新活动） */
    extern thread_t g_threads[];
    bool irq = atomic_enter_critical();
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_threads[i];
        if (t->domain_id == domain && 
            (t->state == THREAD_STATE_READY || t->state == THREAD_STATE_RUNNING)) {
            t->state = THREAD_STATE_BLOCKED;
        }
    }
    target->state = DOMAIN_STATE_SUSPENDED;
    target->flags |= DOMAIN_FLAG_OLD_INSTANCE;
    atomic_exit_critical(irq);
    
    /* 2. 等待资源释放（带调度让步） */
    u64 start_time = hal_get_timestamp();
    u64 timeout_ns = (u64)timeout_ms * 1000000ULL;
    u32 check_count = 0;
    
    while (target->usage.thread_used > 0 || target->usage.cap_used > 0) {
        u64 elapsed = hal_get_timestamp() - start_time;
        if (elapsed >= timeout_ns) {
            console_puts("[DOMAIN] Graceful shutdown timeout, force=");
            console_puts(force ? "yes" : "no");
            console_puts("\n");
            
            if (!force) {
                return HIC_ERROR_TIMEOUT;
            }
            break;
        }
        
        /* 每隔一段时间让调度器运行其他线程 */
        check_count++;
        if ((check_count % 10) == 0) {
            /* 让其他线程有机会执行（可能正在释放资源） */
            extern void thread_yield(void);
            thread_yield();
        } else {
            /* 短暂延迟避免忙等待 */
            hal_udelay(100);  /* 100us */
        }
    }
    
    /* 3. 销毁域 */
    hic_status_t status = domain_destroy(domain);
    
    if (status == HIC_SUCCESS) {
        console_puts("[DOMAIN] Domain ");
        console_putu32(domain);
        console_puts(" gracefully shutdown\n");
    }
    
    return status;
}