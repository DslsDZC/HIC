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
#include "console.h"
#include "lib/mem.h"

/* 全局域表 */
static domain_t g_domains[MAX_DOMAINS];
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
    domain->page_table = 0;
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
    
    /* 特权域标记（Privileged-1 服务默认为特权域） */
    if (type == DOMAIN_TYPE_PRIVILEGED) {
        domain->flags |= DOMAIN_FLAG_PRIVILEGED;
        
        /* 设置运行时特权位图（增强安全） */
        extern void cap_set_privileged_domain(domain_id_t, bool);
        cap_set_privileged_domain(domain_id, true);
    }
    
    /* Core-0 为可信域 */
    if (type == DOMAIN_TYPE_CORE) {
        domain->flags |= DOMAIN_FLAG_TRUSTED;
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
    
    if (domain->state != DOMAIN_STATE_SUSPENDED) {
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