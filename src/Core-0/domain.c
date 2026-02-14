/**
 * HIK域管理实现
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#include "domain.h"
#include "capability.h"
#include "pmm.h"
#include "formal_verification.h"
#include "console.h"

/* 全局域表 */
static domain_t g_domains[MAX_DOMAINS];
static u32 g_domain_count = 0;

/* 域系统初始化 */
void domain_system_init(void)
{
    console_puts("[Domain] Initializing domain system...\n");
    
    /* 初始化所有域为无效状态 */
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        g_domains[i].domain_id = i;
        g_domains[i].state = DOMAIN_STATE_INIT;
        g_domains[i].flags = 0;
        g_domains[i].parent_domain = HIK_INVALID_DOMAIN;
    }
    
    /* 创建Core-0域 */
    domain_quota_t core_quota = {
        .max_memory = 0x100000,      /* 1MB */
        .max_threads = 16,
        .max_caps = 1024,
        .cpu_quota_percent = 100
    };
    
    domain_id_t core_domain;
    if (domain_create(DOMAIN_TYPE_CORE, HIK_INVALID_DOMAIN, &core_quota, &core_domain) == HIK_SUCCESS) {
        console_puts("[Domain] Core-0 domain created\n");
    }
    
    g_domain_count = 1;
}

/**
 * 创建域
 */
hik_status_t domain_create(domain_type_t type, domain_id_t parent,
                           const domain_quota_t *quota, domain_id_t *out)
{
    /* 查找空闲域ID */
    domain_id_t domain_id = HIK_INVALID_DOMAIN;
    for (u32 i = 0; i < MAX_DOMAINS; i++) {
        if (g_domains[i].state == DOMAIN_STATE_INIT) {
            domain_id = i;
            break;
        }
    }
    
    if (domain_id == HIK_INVALID_DOMAIN) {
        return HIK_ERROR_NO_RESOURCE;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    /* 分配能力空间 */
    domain->cap_capacity = quota->max_caps;
    domain->cap_space = (cap_handle_t *)pmm_alloc_frames(HIK_DOMAIN_CORE,
                                                       (domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE,
                                                       PAGE_FRAME_CORE, NULL);
    if (!domain->cap_space) {
        return HIK_ERROR_NO_RESOURCE;
    }
    
    /* 分配物理内存 */
    phys_addr_t mem_base;
    size_t mem_size = quota->max_memory;
    if (pmm_alloc_frames(domain_id, (mem_size + PAGE_SIZE - 1) / PAGE_SIZE,
                         PAGE_FRAME_PRIVILEGED, &mem_base) != HIK_SUCCESS) {
        pmm_free_frames((phys_addr_t)domain->cap_space, 
                        (domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE);
        return HIK_ERROR_NO_RESOURCE;
    }
    
    /* 初始化域 */
    domain->domain_id = domain_id;
    domain->type = type;
    domain->state = DOMAIN_STATE_READY;
    domain->phys_base = mem_base;
    domain->phys_size = mem_size;
    domain->page_table = 0;
    domain->cap_count = 0;
    domain->thread_list = NULL;
    domain->thread_count = 0;
    domain->quota = *quota;
    domain->usage.memory_used = 0;
    domain->usage.thread_used = 0;
    domain->cpu_time_total = 0;
    domain->syscalls_total = 0;
    domain->flags = 0;
    domain->parent_domain = parent;
    
    g_domain_count++;
    
    *out = domain_id;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[Domain] Invariant violation detected after domain_create!\n");
    }
    
    return HIK_SUCCESS;
}

/**
 * 销毁域
 */
hik_status_t domain_destroy(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state == DOMAIN_STATE_INIT ||
        domain->state == DOMAIN_STATE_TERMINATED) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    /* 回收所有能力 */
    for (u32 i = 0; i < domain->cap_count; i++) {
        cap_revoke(domain_id, domain->cap_space[i]);
    }
    
    /* 释放能力空间 */
    if (domain->cap_space) {
        pmm_free_frames((phys_addr_t)domain->cap_space,
                        (domain->cap_capacity * sizeof(cap_handle_t) + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    /* 释放物理内存 */
    if (domain->phys_base != 0) {
        pmm_free_frames(domain->phys_base, (domain->phys_size + PAGE_SIZE - 1) / PAGE_SIZE);
    }
    
    domain->state = DOMAIN_STATE_TERMINATED;
    g_domain_count--;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[Domain] Invariant violation detected after domain_destroy!\n");
    }
    
    return HIK_SUCCESS;
}

/**
 * 查询域信息
 */
hik_status_t domain_get_info(domain_id_t domain_id, domain_t *info)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state == DOMAIN_STATE_INIT) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    *info = *domain;
    return HIK_SUCCESS;
}

/**
 * 暂停域
 */
hik_status_t domain_suspend(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state != DOMAIN_STATE_RUNNING) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    domain->state = DOMAIN_STATE_SUSPENDED;
    return HIK_SUCCESS;
}

/**
 * 恢复域
 */
hik_status_t domain_resume(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->state != DOMAIN_STATE_SUSPENDED) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    domain->state = DOMAIN_STATE_RUNNING;
    return HIK_SUCCESS;
}

/**
 * 检查内存配额
 */
hik_status_t domain_check_memory_quota(domain_id_t domain_id, size_t size)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->usage.memory_used + size > domain->quota.max_memory) {
        return HIK_ERROR_QUOTA_EXCEEDED;
    }
    
    return HIK_SUCCESS;
}

/**
 * 检查线程配额
 */
hik_status_t domain_check_thread_quota(domain_id_t domain_id)
{
    if (domain_id >= MAX_DOMAINS) {
        return HIK_ERROR_INVALID_DOMAIN;
    }
    
    domain_t *domain = &g_domains[domain_id];
    
    if (domain->usage.thread_used >= domain->quota.max_threads) {
        return HIK_ERROR_QUOTA_EXCEEDED;
    }
    
    return HIK_SUCCESS;
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
        cap_entry_t cap;
        if (cap_get_info(d->cap_space[i], &cap) == HIK_SUCCESS) {
            if (cap.type == CAP_TYPE_CAP_DERIVE) {
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
    
    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        cap_entry_t cap;
        if (cap_get_info(i, &cap) == HIK_SUCCESS) {
            if (cap.owner == domain && (cap.flags & CAP_FLAG_REVOKED)) {
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