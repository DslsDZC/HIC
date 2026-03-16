/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC逻辑内核系统实现
 * 将物理CPU核心抽象为可编程、可控制、可观察的能力对象
 * 
 * 核心特性：
 * 1. 解耦：软件需求与物理核心细节分离
 * 2. 可控：开发者真正"拥有"计算单元
 * 3. 高效：轻量级抽象，接近裸金属性能
 * 4. 安全：核心纳入能力系统，所有操作必须授权
 * 5. 可扩展：支持超配和透明迁移
 */

#include "logical_core.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "hardware_probe.h"
#include "thread.h"
#include "domain.h"
#include "monitor.h"

/* ==================== 全局数据结构定义 ==================== */

/* 逻辑核心表（全局） */
logical_core_t g_logical_cores[MAX_LOGICAL_CORES];

/* 逻辑核心到物理核心映射表 */
physical_core_id_t g_logical_to_physical_map[MAX_LOGICAL_CORES];

/* 物理核心到逻辑核心反向映射（每个物理核心上的逻辑核心列表） */
logical_core_id_t g_physical_to_logical_map[MAX_PHYSICAL_CORES][16];
u32 g_physical_core_load[MAX_PHYSICAL_CORES];

/* 空闲逻辑核心列表 */
logical_core_t *g_free_logical_cores = NULL;

/* 逻辑核心ID分配器 */
static u32 g_next_logical_core_id = 0;

/* 全局借用信息表 */
static logical_core_borrow_info_t g_borrow_info[MAX_LOGICAL_CORES];

/* 迁移配置 */
static migration_config_t g_migration_config = DEFAULT_MIGRATION_CONFIG;


/* ==================== 性能优化索引 ==================== */

/* 缓存的最小负载物理核心（O(1)分配） */
static physical_core_id_t g_cached_min_load_core = 0;

/* 逻辑核心在物理核心列表中的索引（O(1)迁移） */
static u8 g_lcore_index_in_pcore[MAX_LOGICAL_CORES];
/* ==================== 辅助函数 ==================== */

/**
 * 初始化逻辑核心控制块
 */
static void logical_core_init_block(logical_core_t *core, logical_core_id_t id) {
    memzero(core, sizeof(logical_core_t));
    
    core->logical_core_id = id;
    core->state = LOGICAL_CORE_STATE_FREE;
    core->flags = 0;
    core->owner_domain = HIC_INVALID_DOMAIN;
    core->capability_handle = CAP_HANDLE_INVALID;
    core->running_thread = INVALID_THREAD;
    
    /* 初始化映射信息 */
    core->mapping.physical_core_id = INVALID_PHYSICAL_CORE;
    core->mapping.migration_count = 0;
    core->mapping.last_migration_time = 0;
    
    /* 初始化亲和性掩码（默认所有物理核心） */
    memzero(&core->affinity, sizeof(logical_core_affinity_t));
    for (int i = 0; i < 4; i++) {
        core->affinity.mask[i] = 0xFFFFFFFFFFFFFFFFULL;
    }
    
    /* 初始化配额信息 */
    core->quota.guaranteed_quota = LOGICAL_CORE_DEFAULT_QUOTA;
    core->quota.max_quota = 100;
    core->quota.used_time = 0;
    core->quota.allocated_time = 0;
    
    /* 初始化性能计数器 */
    memzero(&core->perf, sizeof(logical_core_perf_t));
    
    /* 添加到空闲列表 */
    core->next = g_free_logical_cores;
    core->prev = NULL;
    if (g_free_logical_cores) {
        g_free_logical_cores->prev = core;
    }
    g_free_logical_cores = core;
}

/**
 * 从空闲列表移除逻辑核心
 */
static void logical_core_remove_from_free_list(logical_core_t *core) {
    if (core->prev) {
        core->prev->next = core->next;
    } else {
        /* 这是列表头 */
        g_free_logical_cores = core->next;
    }
    
    if (core->next) {
        core->next->prev = core->prev;
    }
    
    core->next = NULL;
    core->prev = NULL;
}

/**
 * 验证逻辑核心能力句柄
 */
logical_core_id_t logical_core_validate_handle(domain_id_t domain_id,
                                              cap_handle_t handle) {
    if (handle == CAP_HANDLE_INVALID) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 使用能力系统验证句柄 */
    hic_status_t status = cap_check_access(domain_id, handle, CAP_LCORE_USE);
    if (status != HIC_SUCCESS) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 从能力条目获取逻辑核心ID */
    cap_id_t cap_id = cap_get_cap_id(handle);
    logical_core_id_t logical_core_id;
    logical_core_flags_t flags;
    logical_core_quota_t quota;
    
    status = cap_get_logical_core_info(cap_id, &logical_core_id, &flags, &quota);
    if (status != HIC_SUCCESS) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 验证逻辑核心ID范围 */
    if (logical_core_id >= MAX_LOGICAL_CORES) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 验证逻辑核心状态 */
    logical_core_t *core = &g_logical_cores[logical_core_id];
    if (core->state != LOGICAL_CORE_STATE_ALLOCATED &&
        core->state != LOGICAL_CORE_STATE_ACTIVE) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 验证所有权 */
    if (core->owner_domain != domain_id) {
        return INVALID_LOGICAL_CORE;
    }
    
    return logical_core_id;
}

/**
 * 查找空闲逻辑核心
 */
logical_core_id_t logical_core_find_free(logical_core_flags_t flags,
                                        logical_core_quota_t quota,
                                        const logical_core_affinity_t *affinity) {
    /* 遍历空闲列表 */
    logical_core_t *current = g_free_logical_cores;
    while (current) {
        /* 检查配额是否满足 */
        if (quota > current->quota.guaranteed_quota) {
            current = current->next;
            continue;
        }
        
        /* 检查亲和性是否匹配 */
        if (affinity) {
            bool affinity_match = false;
            for (int i = 0; i < 4; i++) {
                if (affinity->mask[i] & current->affinity.mask[i]) {
                    affinity_match = true;
                    break;
                }
            }
            if (!affinity_match) {
                current = current->next;
                continue;
            }
        }
        
        /* 检查标志是否兼容 */
        /* 独占核心只能分配给一个域 */
        if ((flags & LOGICAL_CORE_FLAG_EXCLUSIVE) && 
            current->state != LOGICAL_CORE_STATE_FREE) {
            current = current->next;
            continue;
        }
        
        /* 找到合适的逻辑核心 */
        return current->logical_core_id;
    }
    
    return INVALID_LOGICAL_CORE;
}

/**
 * 分配逻辑核心给域
 */
cap_handle_t logical_core_allocate_to_domain(logical_core_id_t logical_core_id,
                                            domain_id_t domain_id,
                                            logical_core_flags_t flags,
                                            logical_core_quota_t quota,
                                            const logical_core_affinity_t *affinity) {
    if (logical_core_id >= MAX_LOGICAL_CORES || domain_id >= HIC_DOMAIN_MAX) {
        return CAP_HANDLE_INVALID;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    /* 从空闲列表移除 */
    logical_core_remove_from_free_list(core);
    
    /* 更新核心信息 */
    core->state = LOGICAL_CORE_STATE_ALLOCATED;
    core->flags = flags;
    core->owner_domain = domain_id;
    core->quota.guaranteed_quota = quota;
    
    /* 设置亲和性 */
    if (affinity) {
        core->affinity = *affinity;
    }
    
    /* 选择物理核心映射 */
    physical_core_id_t physical_core_id = INVALID_PHYSICAL_CORE;
    
    /* 如果有亲和性要求，选择匹配的物理核心 */
    if (affinity) {
        for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
            u64 mask_index = i / 64;
            u64 mask_bit = 1ULL << (i % 64);
            
            if (affinity->mask[mask_index] & mask_bit) {
                /* 检查物理核心负载 */
                if (g_physical_core_load[i] < 16) {  /* 每个物理核心最多16个逻辑核心 */
                    physical_core_id = i;
                    break;
                }
            }
        }
    }
    
    /* 如果没有亲和性要求或没有匹配的，使用缓存的最小负载物理核心 */
    if (physical_core_id == INVALID_PHYSICAL_CORE) {
        /* O(1) 快速路径：使用缓存的最小负载核心 */
        physical_core_id = g_cached_min_load_core;
        
        /* 验证缓存是否有效（每16次分配更新一次缓存） */
        static u32 cache_check_counter = 0;
        if (++cache_check_counter >= 16) {
            cache_check_counter = 0;
            u32 min_load = g_physical_core_load[physical_core_id];
            for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
                if (g_physical_core_load[i] < min_load) {
                    min_load = g_physical_core_load[i];
                    g_cached_min_load_core = i;
                }
            }
            physical_core_id = g_cached_min_load_core;
        }
    }
    
    /* 更新映射表 */
    core->mapping.physical_core_id = physical_core_id;
    g_logical_to_physical_map[logical_core_id] = physical_core_id;
    
    /* 添加到物理核心的负载列表（O(1)，同时更新索引） */
    if (physical_core_id != INVALID_PHYSICAL_CORE) {
        u32 load = g_physical_core_load[physical_core_id];
        if (load < 16) {
            g_physical_to_logical_map[physical_core_id][load] = logical_core_id;
            g_lcore_index_in_pcore[logical_core_id] = (u8)load;  /* 记录索引 */
            g_physical_core_load[physical_core_id]++;
        }
    }
    
    /* 创建能力对象 */
    cap_id_t cap_id;
    hic_status_t status = cap_create_logical_core(domain_id, logical_core_id,
                                                 flags, quota,
                                                 CAP_LCORE_USE | CAP_LCORE_QUERY,
                                                 &cap_id);
    if (status != HIC_SUCCESS) {
        /* 回滚分配 */
        core->state = LOGICAL_CORE_STATE_FREE;
        core->owner_domain = HIC_INVALID_DOMAIN;
        /* 重新添加到空闲列表 */
        logical_core_init_block(core, logical_core_id);
        return CAP_HANDLE_INVALID;
    }
    
    /* 生成能力句柄 */
    cap_handle_t handle;
    status = cap_grant(domain_id, cap_id, &handle);
    if (status != HIC_SUCCESS) {
        cap_revoke(cap_id);
        core->state = LOGICAL_CORE_STATE_FREE;
        core->owner_domain = HIC_INVALID_DOMAIN;
        logical_core_init_block(core, logical_core_id);
        return CAP_HANDLE_INVALID;
    }
    
    core->capability_handle = handle;
    return handle;
}

/**
 * 执行逻辑核心迁移
 */
bool logical_core_perform_migration(logical_core_id_t logical_core_id,
                                   physical_core_id_t target_physical_core_id) {
    if (logical_core_id >= MAX_LOGICAL_CORES || 
        target_physical_core_id >= MAX_PHYSICAL_CORES) {
        return false;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    physical_core_id_t old_physical_core_id = core->mapping.physical_core_id;
    
    /* 检查目标物理核心是否有空间 */
    if (g_physical_core_load[target_physical_core_id] >= 16) {
        return false;
    }
    
    /* 检查亲和性是否允许 */
    u64 mask_index = target_physical_core_id / 64;
    u64 mask_bit = 1ULL << (target_physical_core_id % 64);
    if (!(core->affinity.mask[mask_index] & mask_bit)) {
        return false;
    }
    
    /* 检查是否允许迁移 */
    if (!(core->flags & LOGICAL_CORE_FLAG_MIGRATABLE) &&
        (core->flags & LOGICAL_CORE_FLAG_PINNED)) {
        return false;
    }
    
    /* 更新状态为迁移中 */
    core->state = LOGICAL_CORE_STATE_MIGRATING;
    
    /* 从旧物理核心移除（O(1)：使用索引直接定位） */
    if (old_physical_core_id != INVALID_PHYSICAL_CORE) {
        u8 idx = g_lcore_index_in_pcore[logical_core_id];
        u32 load = g_physical_core_load[old_physical_core_id];
        
        if (idx < load) {
            /* 移动最后一个元素到当前位置 */
            logical_core_id_t last_lcore = g_physical_to_logical_map[old_physical_core_id][load - 1];
            g_physical_to_logical_map[old_physical_core_id][idx] = last_lcore;
            g_lcore_index_in_pcore[last_lcore] = idx;  /* 更新被移动元素的索引 */
            g_physical_core_load[old_physical_core_id]--;
        }
    }
    
    /* 添加到新物理核心（O(1)） */
    u32 new_load = g_physical_core_load[target_physical_core_id];
    g_physical_to_logical_map[target_physical_core_id][new_load] = logical_core_id;
    g_lcore_index_in_pcore[logical_core_id] = (u8)new_load;  /* 记录新索引 */
    g_physical_core_load[target_physical_core_id]++;
    
    /* 更新映射表 */
    core->mapping.physical_core_id = target_physical_core_id;
    g_logical_to_physical_map[logical_core_id] = target_physical_core_id;
    
    /* 更新迁移统计 */
    core->mapping.migration_count++;
    extern u64 hal_get_timestamp(void);
    core->mapping.last_migration_time = hal_get_timestamp();
    
    /* 恢复状态 */
    if (core->running_thread != INVALID_THREAD) {
        core->state = LOGICAL_CORE_STATE_ACTIVE;
    } else {
        core->state = LOGICAL_CORE_STATE_ALLOCATED;
    }
    
    return true;
}

/* ==================== 公共API实现 ==================== */

/**
 * 初始化逻辑内核系统
 */
void logical_core_system_init(void) {
    console_puts("[LCORE] Initializing logical core system...\n");
    
    /* 获取物理核心数量 */
    extern cpu_info_t g_cpu_info;
    u32 physical_core_count = g_cpu_info.physical_cores;
    if (physical_core_count == 0) {
        physical_core_count = 1;  /* 至少一个物理核心 */
    }
    
    console_puts("[LCORE] Physical cores detected: ");
    console_putu32(physical_core_count);
    console_puts("\n");
    
    /* 初始化全局数据结构 */
    memzero(g_logical_cores, sizeof(g_logical_cores));
    memzero(g_logical_to_physical_map, sizeof(g_logical_to_physical_map));
    memzero(g_physical_to_logical_map, sizeof(g_physical_to_logical_map));
    memzero(g_physical_core_load, sizeof(g_physical_core_load));
    
    /* 创建逻辑核心池 */
    u32 logical_core_count = MAX_LOGICAL_CORES;
    if (logical_core_count > 1024) {
        logical_core_count = 1024;  /* 限制最大数量 */
    }
    
    console_puts("[LCORE] Creating logical core pool: ");
    console_putu32(logical_core_count);
    console_puts(" logical cores\n");
    
    for (u32 i = 0; i < logical_core_count; i++) {
        logical_core_init_block(&g_logical_cores[i], i);
    }
    
    g_next_logical_core_id = logical_core_count;
    
    console_puts("[LCORE] Logical core system initialized\n");
    console_puts("[LCORE] Ready for allocation (supports overcommitment)\n");
}

/**
 * 分配逻辑核心
 */
hic_status_t hic_logical_core_allocate(domain_id_t domain_id, u32 count,
                                      logical_core_flags_t flags,
                                      logical_core_quota_t quota,
                                      const logical_core_affinity_t *affinity,
                                      cap_handle_t out_handles[]) {
    if (domain_id >= HIC_DOMAIN_MAX || count == 0 || count > 16 || out_handles == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查配额范围 */
    if (quota < LOGICAL_CORE_QUOTA_MIN || quota > LOGICAL_CORE_QUOTA_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查域是否存在 */
    extern domain_t g_domains[HIC_DOMAIN_MAX];
    if (domain_id != HIC_DOMAIN_CORE && 
        (g_domains[domain_id].state != DOMAIN_STATE_READY &&
         g_domains[domain_id].state != DOMAIN_STATE_RUNNING)) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 分配请求数量的逻辑核心 */
    u32 allocated = 0;
    for (u32 i = 0; i < count; i++) {
        logical_core_id_t logical_core_id = logical_core_find_free(flags, quota, affinity);
        if (logical_core_id == INVALID_LOGICAL_CORE) {
            /* 分配失败，回滚已分配的核心 */
            for (u32 j = 0; j < allocated; j++) {
                /* 释放已分配的核心 */
                cap_handle_t handle = out_handles[j];
                cap_id_t cap_id = cap_get_cap_id(handle);
                cap_revoke(cap_id);
                
                /* 恢复逻辑核心状态 */
                logical_core_id_t lcore_id;
                logical_core_flags_t lcore_flags;
                logical_core_quota_t lcore_quota;
                cap_get_logical_core_info(cap_id, &lcore_id, &lcore_flags, &lcore_quota);
                
                logical_core_t *core = &g_logical_cores[lcore_id];
                core->state = LOGICAL_CORE_STATE_FREE;
                core->owner_domain = HIC_INVALID_DOMAIN;
                logical_core_init_block(core, lcore_id);
            }
            atomic_exit_critical(irq);
            return HIC_ERROR_NO_RESOURCE;
        }
        
        /* 分配逻辑核心给域 */
        cap_handle_t handle = logical_core_allocate_to_domain(logical_core_id, domain_id,
                                                             flags, quota, affinity);
        if (handle == CAP_HANDLE_INVALID) {
            /* 分配失败，回滚已分配的核心 */
            for (u32 j = 0; j < allocated; j++) {
                cap_handle_t h = out_handles[j];
                cap_id_t cap_id = cap_get_cap_id(h);
                cap_revoke(cap_id);
                
                logical_core_id_t lcore_id;
                logical_core_flags_t lcore_flags;
                logical_core_quota_t lcore_quota;
                cap_get_logical_core_info(cap_id, &lcore_id, &lcore_flags, &lcore_quota);
                
                logical_core_t *core = &g_logical_cores[lcore_id];
                core->state = LOGICAL_CORE_STATE_FREE;
                core->owner_domain = HIC_INVALID_DOMAIN;
                logical_core_init_block(core, lcore_id);
            }
            atomic_exit_critical(irq);
            return HIC_ERROR_NO_RESOURCE;
        }
        
        out_handles[allocated] = handle;
        allocated++;
    }
    
    atomic_exit_critical(irq);
    return HIC_SUCCESS;
}

/**
 * 释放逻辑核心
 */
hic_status_t hic_logical_core_release(domain_id_t domain_id,
                                     const cap_handle_t handles[],
                                     u32 count) {
    if (domain_id >= HIC_DOMAIN_MAX || handles == NULL || count == 0 || count > 16) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    for (u32 i = 0; i < count; i++) {
        cap_handle_t handle = handles[i];
        
        /* 验证句柄 */
        logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, handle);
        if (logical_core_id == INVALID_LOGICAL_CORE) {
            atomic_exit_critical(irq);
            return HIC_ERROR_CAP_INVALID;
        }
        
        logical_core_t *core = &g_logical_cores[logical_core_id];
        
        /* 检查是否有线程正在运行 */
        if (core->running_thread != INVALID_THREAD) {
            atomic_exit_critical(irq);
            return HIC_ERROR_BUSY;
        }
        
        /* 从物理核心负载列表中移除（O(1)：使用索引直接定位） */
        physical_core_id_t physical_core_id = core->mapping.physical_core_id;
        if (physical_core_id != INVALID_PHYSICAL_CORE) {
            u8 idx = g_lcore_index_in_pcore[logical_core_id];
            u32 load = g_physical_core_load[physical_core_id];
            
            if (idx < load) {
                /* 移动最后一个元素到当前位置 */
                logical_core_id_t last_lcore = g_physical_to_logical_map[physical_core_id][load - 1];
                g_physical_to_logical_map[physical_core_id][idx] = last_lcore;
                g_lcore_index_in_pcore[last_lcore] = idx;  /* 更新被移动元素的索引 */
                g_physical_core_load[physical_core_id]--;
            }
        }
        
        /* 撤销能力 */
        cap_id_t cap_id = cap_get_cap_id(handle);
        cap_revoke(cap_id);
        
        /* 重置逻辑核心状态 */
        core->state = LOGICAL_CORE_STATE_FREE;
        core->owner_domain = HIC_INVALID_DOMAIN;
        core->capability_handle = CAP_HANDLE_INVALID;
        core->running_thread = INVALID_THREAD;
        
        /* 重新添加到空闲列表 */
        logical_core_init_block(core, logical_core_id);
    }
    
    atomic_exit_critical(irq);
    return HIC_SUCCESS;
}

/**
 * 在逻辑核心上创建线程
 */
hic_status_t hic_thread_create_on_core(cap_handle_t logical_core_handle,
                                      virt_addr_t entry_point,
                                      priority_t priority,
                                      void *arg,
                                      thread_id_t *out_thread_id) {
    (void)arg;  /* 参数暂未使用，保留API兼容性 */
    
    if (logical_core_handle == CAP_HANDLE_INVALID || out_thread_id == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 获取调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id;
    
    if (g_current_thread != NULL) {
        domain_id = g_current_thread->domain_id;
    } else {
        /* 从域切换模块获取当前域 */
        extern domain_id_t domain_switch_get_current(void);
        domain_id_t current_domain = domain_switch_get_current();
        domain_id = (current_domain != HIC_INVALID_DOMAIN) ? current_domain : HIC_DOMAIN_CORE;
    }
    
    /* 验证逻辑核心句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    /* 检查核心是否已被使用 */
    if (core->running_thread != INVALID_THREAD) {
        return HIC_ERROR_BUSY;
    }
    
    /* 创建绑定到逻辑核心的线程 */
    thread_id_t thread_id;
    hic_status_t status = thread_create_bound(domain_id, logical_core_id, 
                                              entry_point, priority, &thread_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 更新逻辑核心状态（thread_create_bound 已设置 thread.logical_core_id） */
    core->running_thread = thread_id;
    core->state = LOGICAL_CORE_STATE_ACTIVE;
    
    *out_thread_id = thread_id;
    return HIC_SUCCESS;
}

/**
 * 获取逻辑核心信息
 */
hic_status_t hic_logical_core_get_info(cap_handle_t logical_core_handle,
                                      logical_core_t *info) {
    if (logical_core_handle == CAP_HANDLE_INVALID || info == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
    /* 验证句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查权限 */
    hic_status_t status = cap_check_access(domain_id, logical_core_handle, CAP_LCORE_QUERY);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 复制信息 */
    logical_core_t *core = &g_logical_cores[logical_core_id];
    memcopy(info, core, sizeof(logical_core_t));
    
    return HIC_SUCCESS;
}

/**
 * 设置逻辑核心亲和性
 */
hic_status_t hic_logical_core_set_affinity(cap_handle_t logical_core_handle,
                                          const logical_core_affinity_t *affinity) {
    if (logical_core_handle == CAP_HANDLE_INVALID || affinity == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
    /* 验证句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查权限 */
    hic_status_t status = cap_check_access(domain_id, logical_core_handle, CAP_LCORE_SET_AFFINITY);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    /* 检查核心是否正在运行线程 */
    if (core->running_thread != INVALID_THREAD) {
        return HIC_ERROR_BUSY;
    }
    
    /* 更新亲和性 */
    core->affinity = *affinity;
    
    return HIC_SUCCESS;
}

/**
 * 迁移逻辑核心
 */
hic_status_t hic_logical_core_migrate(cap_handle_t logical_core_handle,
                                     physical_core_id_t physical_core_id) {
    if (logical_core_handle == CAP_HANDLE_INVALID || physical_core_id >= MAX_PHYSICAL_CORES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
    /* 验证句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查权限 */
    hic_status_t status = cap_check_access(domain_id, logical_core_handle, CAP_LCORE_MIGRATE);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 执行迁移 */
    bool success = logical_core_perform_migration(logical_core_id, physical_core_id);
    if (!success) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    return HIC_SUCCESS;
}

/**
 * 获取逻辑核心性能统计
 */
hic_status_t hic_logical_core_get_perf(cap_handle_t logical_core_handle,
                                      logical_core_perf_t *perf) {
    if (logical_core_handle == CAP_HANDLE_INVALID || perf == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
    /* 验证句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查权限 */
    hic_status_t status = cap_check_access(domain_id, logical_core_handle, CAP_LCORE_MONITOR);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    memcopy(perf, &core->perf, sizeof(logical_core_perf_t));
    
    return HIC_SUCCESS;
}

/**
 * 重置逻辑核心性能计数器
 */
hic_status_t hic_logical_core_reset_perf(cap_handle_t logical_core_handle) {
    if (logical_core_handle == CAP_HANDLE_INVALID) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
    /* 验证句柄 */
    logical_core_id_t logical_core_id = logical_core_validate_handle(domain_id, logical_core_handle);
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    /* 检查权限 */
    hic_status_t status = cap_check_access(domain_id, logical_core_handle, CAP_LCORE_MONITOR);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    memzero(&core->perf, sizeof(logical_core_perf_t));
    
    return HIC_SUCCESS;
}

/* ==================== 调度器集成函数 ==================== */

/**
 * 逻辑核心调度器选择
 * 调度器调用此函数选择在哪个逻辑核心上运行线程
 */
logical_core_id_t logical_core_schedule_select(thread_t *thread) {
    if (thread == NULL) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 获取物理核心数量 */
    extern cpu_info_t g_cpu_info;
    u32 physical_core_count = g_cpu_info.physical_cores;
    if (physical_core_count == 0) {
        physical_core_count = 1;
    }
    
    logical_core_id_t best_core = INVALID_LOGICAL_CORE;
    u32 best_score = 0;
    
    /* 遍历所有物理核心，计算每个的调度评分 */
    for (physical_core_id_t phys_id = 0; phys_id < physical_core_count; phys_id++) {
        /* 该物理核心上属于同域的最优逻辑核心 */
        logical_core_id_t domain_best_core = INVALID_LOGICAL_CORE;
        u32 domain_best_score = 0;
        
        /* 遍历该物理核心上的逻辑核心 */
        for (u32 i = 0; i < g_physical_core_load[phys_id]; i++) {
            logical_core_id_t lcore_id = g_physical_to_logical_map[phys_id][i];
            logical_core_t *core = &g_logical_cores[lcore_id];
            
            /* 必须满足基本条件 */
            if (core->state != LOGICAL_CORE_STATE_ALLOCATED ||
                core->running_thread != INVALID_THREAD) {
                continue;
            }
            
            /* 优先选择同域的逻辑核心 */
            u32 score = 0;
            
            if (core->owner_domain == thread->domain_id) {
                score += 100;  /* 同域高优先级 */
            } else if (g_borrow_info[lcore_id].state == BORROW_STATE_BORROWED &&
                      g_borrow_info[lcore_id].borrower_domain == thread->domain_id) {
                score += 80;  /* 借用的核心 */
            } else {
                continue;  /* 不能使用其他域的核心 */
            }
            
            /* 考虑负载：负载越低越好 */
            physical_core_load_info_t load_info;
            logical_core_calculate_load(phys_id, &load_info);
            score += (100 - load_info.load_percentage) / 2;
            
            /* 考虑配额剩余 */
            if (core->quota.allocated_time > core->quota.used_time) {
                score += 10;  /* 还有剩余配额 */
            }
            
            /* 考虑缓存亲和性：如果之前在此核心运行过 */
            if (core->perf.cache_hits > core->perf.cache_misses) {
                score += 15;
            }
            
            if (score > domain_best_score) {
                domain_best_score = score;
                domain_best_core = lcore_id;
            }
        }
        
        /* 选择全局最优 */
        if (domain_best_score > best_score) {
            best_score = domain_best_score;
            best_core = domain_best_core;
        }
    }
    
    return best_core;
}

/**
 * 逻辑核心调度器通知
 * 调度器在线程开始/停止在逻辑核心上运行时调用此函数
 */
void logical_core_schedule_notify(logical_core_id_t logical_core_id,
                                 thread_id_t thread_id,
                                 bool starting) {
    if (logical_core_id >= MAX_LOGICAL_CORES) {
        return;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    if (starting) {
        core->running_thread = thread_id;
        core->state = LOGICAL_CORE_STATE_ACTIVE;
        core->last_schedule_time = hal_get_timestamp();
    } else {
        core->running_thread = INVALID_THREAD;
        if (core->state == LOGICAL_CORE_STATE_ACTIVE) {
            core->state = LOGICAL_CORE_STATE_ALLOCATED;
        }
        
        /* 更新CPU时间使用统计 */
        u64 current_time = hal_get_timestamp();
        u64 time_used = current_time - core->last_schedule_time;
        core->quota.used_time += time_used;
    }
}

/**
 * 更新逻辑核心配额使用
 * 定时器中断中调用，更新逻辑核心的CPU时间使用统计
 */
void logical_core_update_quotas(void) {
    u64 current_time = hal_get_timestamp();
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        logical_core_t *core = &g_logical_cores[i];
        
        if (core->state == LOGICAL_CORE_STATE_ACTIVE) {
            /* 更新活跃核心的使用时间 */
            u64 time_used = current_time - core->last_schedule_time;
            core->quota.used_time += time_used;
            core->last_schedule_time = current_time;
            
            /* 更新借用核心的配额使用 */
            if (g_borrow_info[i].state == BORROW_STATE_BORROWED) {
                g_borrow_info[i].borrow_quota_used += (u32)time_used;
            }
            
            /* 检查是否超过配额 */
            if (core->quota.used_time > core->quota.allocated_time) {
                u64 overage = core->quota.used_time - core->quota.allocated_time;
                u32 overage_percent = (u32)((overage * 100) / core->quota.allocated_time);
                
                /* 记录警告 */
                console_puts("[LCORE] Warning: Logical core ");
                console_putu32(i);
                console_puts(" quota exceeded by ");
                console_putu32(overage_percent);
                console_puts("%\n");
                
                /* 根据超限程度采取不同措施 */
                if (overage_percent < 10) {
                    /* 轻微超限：仅警告，继续运行 */
                    core->flags |= LOGICAL_CORE_FLAG_QUOTA_WARNING;
                } else if (overage_percent < 25) {
                    /* 中度超限：暂停线程调度，等待配额补充 */
                    core->flags |= LOGICAL_CORE_FLAG_QUOTA_THROTTLED;
                    
                    /* 让出当前线程 */
                    core->running_thread = INVALID_THREAD;
                    core->state = LOGICAL_CORE_STATE_ALLOCATED;
                } else {
                    /* 严重超限：暂停核心 */
                    console_puts("[LCORE] Critical: Suspending logical core ");
                    console_putu32(i);
                    console_puts(" due to severe quota violation\n");
                    
                    core->state = LOGICAL_CORE_STATE_SUSPENDED;
                    core->flags |= LOGICAL_CORE_FLAG_QUOTA_EXCEEDED;
                    core->running_thread = INVALID_THREAD;
                    
                    /* 通知监控服务 */
                    monitor_event_t event;
                    event.type = MONITOR_EVENT_RESOURCE_EXHAUSTED;
                    event.domain = core->owner_domain;
                    event.timestamp = current_time;
                    event.data[0] = i;  /* 逻辑核心ID */
                    event.data[1] = core->quota.used_time;
                    event.data[2] = core->quota.allocated_time;
                    event.data[3] = overage_percent;
                    monitor_report_event(&event);
                }
            }
            
            /* 检查借用核心是否超时 */
            if (g_borrow_info[i].state == BORROW_STATE_BORROWED &&
                current_time >= g_borrow_info[i].borrow_deadline) {
                
                console_puts("[LCORE] Borrow expired for core ");
                console_putu32(i);
                console_puts(", scheduling return\n");
                
                /* 标记需要归还 */
                g_borrow_info[i].state = BORROW_STATE_RETURNING;
            }
        }
    }
}

/**
 * 逻辑核心迁移决策
 * 监控服务定期调用，决定是否需要迁移逻辑核心以平衡负载
 * 
 * 使用默认配置调用增强版迁移决策
 */
void logical_core_migration_decision(void) {
    u32 migrations_executed = 0;
    migration_config_t config = DEFAULT_MIGRATION_CONFIG;
    logical_core_enhanced_migration_decision(&config, &migrations_executed);
}

/* ==================== 借用机制实现 ==================== */

/**
 * 借用逻辑核心
 */
hic_status_t hic_logical_core_borrow(domain_id_t borrower_domain,
                                    domain_id_t source_domain,
                                    u64 duration,
                                    u32 quota,
                                    const logical_core_affinity_t *affinity,
                                    cap_handle_t *out_handle) {
    if (out_handle == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (borrower_domain == source_domain) {
        return HIC_ERROR_INVALID_PARAM;  /* 不能向自己借用 */
    }
    
    /* 验证借用者域是否存在 */
    extern bool domain_is_active(domain_id_t domain_id);
    if (!domain_is_active(borrower_domain) || !domain_is_active(source_domain)) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    
    /* 查找出借者域中可借用的逻辑核心 */
    logical_core_id_t borrowable_ids[16];
    u32 borrowable_count = 0;
    
    hic_status_t status = hic_logical_core_get_borrowable(source_domain, borrowable_ids, 
                                                          16, &borrowable_count);
    if (status != HIC_SUCCESS || borrowable_count == 0) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 选择最合适的可借用核心 */
    logical_core_id_t selected_core = INVALID_LOGICAL_CORE;
    
    for (u32 i = 0; i < borrowable_count; i++) {
        logical_core_t *core = &g_logical_cores[borrowable_ids[i]];
        
        /* 检查亲和性约束 */
        if (affinity != NULL) {
            bool affinity_match = false;
            for (int j = 0; j < 4; j++) {
                if (core->affinity.mask[j] & affinity->mask[j]) {
                    affinity_match = true;
                    break;
                }
            }
            if (!affinity_match) {
                continue;
            }
        }
        
        /* 选择第一个满足条件的核心 */
        selected_core = borrowable_ids[i];
        break;
    }
    
    if (selected_core == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_NO_RESOURCE;
    }
    
    logical_core_t *core = &g_logical_cores[selected_core];
    
    /* 创建派生能力 */
    /* cap_derive(owner, parent, sub_rights, out) */
    cap_id_t derived_cap_id;
    status = cap_derive(borrower_domain, (cap_id_t)core->capability_handle,
                        CAP_LCORE_BORROW | CAP_LCORE_USE, &derived_cap_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 获取派生能力的句柄 */
    cap_handle_t derived_handle = (cap_handle_t)derived_cap_id;
    
    /* 更新借用信息 */
    logical_core_borrow_info_t *borrow_info = &g_borrow_info[selected_core];
    borrow_info->state = BORROW_STATE_BORROWED;
    borrow_info->original_owner = core->owner_domain;
    borrow_info->borrower_domain = borrower_domain;
    borrow_info->original_cap_handle = core->capability_handle;
    borrow_info->derived_cap_handle = derived_handle;
    borrow_info->borrow_start_time = hal_get_timestamp();
    borrow_info->borrow_duration = duration;
    borrow_info->borrow_deadline = borrow_info->borrow_start_time + duration;
    borrow_info->borrow_quota_used = 0;
    
    /* 设置借用期间的配额限制 */
    core->quota.allocated_time = (u64)quota * 10000000ULL;  /* 转换为纳秒 */
    
    /* 临时转移控制权 */
    core->owner_domain = borrower_domain;
    core->capability_handle = derived_handle;
    
    console_puts("[LCORE] Core ");
    console_putu32(selected_core);
    console_puts(" borrowed by domain ");
    console_putu32(borrower_domain);
    console_puts(" for ");
    console_putu64(duration / 1000000ULL);  /* 转换为毫秒显示 */
    console_puts(" ms\n");
    
    *out_handle = derived_handle;
    return HIC_SUCCESS;
}

/**
 * 归还借用的逻辑核心
 */
hic_status_t hic_logical_core_return(domain_id_t borrower_domain,
                                     cap_handle_t borrowed_handle) {
    /* 查找对应的逻辑核心 */
    logical_core_id_t logical_core_id = INVALID_LOGICAL_CORE;
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        if (g_borrow_info[i].state == BORROW_STATE_BORROWED &&
            g_borrow_info[i].borrower_domain == borrower_domain &&
            g_borrow_info[i].derived_cap_handle == borrowed_handle) {
            logical_core_id = i;
            break;
        }
    }
    
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    logical_core_borrow_info_t *borrow_info = &g_borrow_info[logical_core_id];
    
    /* 更新状态为正在归还 */
    borrow_info->state = BORROW_STATE_RETURNING;
    
    /* 撤销派生能力 */
    extern hic_status_t cap_revoke(cap_id_t cap_id);
    cap_revoke((cap_id_t)borrowed_handle);
    
    /* 恢复原始所有权 */
    core->owner_domain = borrow_info->original_owner;
    core->capability_handle = borrow_info->original_cap_handle;
    
    /* 清除借用信息 */
    u64 actual_duration = hal_get_timestamp() - borrow_info->borrow_start_time;
    
    console_puts("[LCORE] Core ");
    console_putu32(logical_core_id);
    console_puts(" returned to domain ");
    console_putu32(borrow_info->original_owner);
    console_puts(" after ");
    console_putu64(actual_duration / 1000000ULL);
    console_puts(" ms\n");
    
    memzero(borrow_info, sizeof(logical_core_borrow_info_t));
    
    return HIC_SUCCESS;
}

/**
 * 查询逻辑核心借用状态
 */
hic_status_t hic_logical_core_get_borrow_info(cap_handle_t logical_core_handle,
                                              logical_core_borrow_info_t *borrow_info) {
    if (borrow_info == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 查找逻辑核心ID */
    logical_core_id_t logical_core_id = INVALID_LOGICAL_CORE;
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        if (g_logical_cores[i].capability_handle == logical_core_handle) {
            logical_core_id = i;
            break;
        }
    }
    
    if (logical_core_id == INVALID_LOGICAL_CORE) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    *borrow_info = g_borrow_info[logical_core_id];
    return HIC_SUCCESS;
}

/**
 * 检查借用是否到期
 */
void logical_core_check_borrow_expiry(void) {
    u64 current_time = hal_get_timestamp();
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        logical_core_borrow_info_t *borrow_info = &g_borrow_info[i];
        
        if (borrow_info->state == BORROW_STATE_BORROWED &&
            current_time >= borrow_info->borrow_deadline) {
            
            /* 借用已到期，自动归还 */
            console_puts("[LCORE] Borrow expired for core ");
            console_putu32(i);
            console_puts(", auto-returning to domain ");
            console_putu32(borrow_info->original_owner);
            console_puts("\n");
            
            hic_logical_core_return(borrow_info->borrower_domain,
                                   borrow_info->derived_cap_handle);
        }
    }
}

/**
 * 获取可用于借用的逻辑核心列表
 */
hic_status_t hic_logical_core_get_borrowable(domain_id_t source_domain,
                                             logical_core_id_t out_ids[],
                                             u32 max_count,
                                             u32 *out_actual_count) {
    if (out_ids == NULL || out_actual_count == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 count = 0;
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES && count < max_count; i++) {
        logical_core_t *core = &g_logical_cores[i];
        
        /* 检查条件：
         * 1. 属于源域
         * 2. 状态为已分配但未活跃
         * 3. 未被借用
         * 4. 允许迁移（借用需要迁移权限）
         */
        if (core->owner_domain == source_domain &&
            core->state == LOGICAL_CORE_STATE_ALLOCATED &&
            core->running_thread == INVALID_THREAD &&
            g_borrow_info[i].state == BORROW_STATE_NONE &&
            (core->flags & LOGICAL_CORE_FLAG_MIGRATABLE)) {
            
            out_ids[count++] = i;
        }
    }
    
    *out_actual_count = count;
    return HIC_SUCCESS;
}

/* ==================== 增强的迁移决策实现 ==================== */

/**
 * 计算物理核心负载详情
 */
hic_status_t logical_core_calculate_load(physical_core_id_t physical_core_id,
                                         physical_core_load_info_t *load_info) {
    if (physical_core_id >= MAX_PHYSICAL_CORES || load_info == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memzero(load_info, sizeof(physical_core_load_info_t));
    
    u64 current_time = hal_get_timestamp();
    u64 total_used_time = 0;
    u32 active_threads = 0;
    
    /* 遍历该物理核心上的所有逻辑核心 */
    for (u32 i = 0; i < g_physical_core_load[physical_core_id]; i++) {
        logical_core_id_t lcore_id = g_physical_to_logical_map[physical_core_id][i];
        logical_core_t *core = &g_logical_cores[lcore_id];
        
        load_info->logical_core_count++;
        total_used_time += core->quota.used_time;
        
        if (core->state == LOGICAL_CORE_STATE_ACTIVE) {
            active_threads++;
            /* 计算当前运行线程的时间 */
            if (core->last_schedule_time > 0) {
                total_used_time += (current_time - core->last_schedule_time);
            }
        }
    }
    
    load_info->active_thread_count = active_threads;
    load_info->total_cpu_time = total_used_time;
    
    /* 计算负载百分比：基于实际CPU时间使用率 */
    if (load_info->logical_core_count > 0) {
        /* 使用实际CPU时间计算负载百分比 */
        /* 获取时间窗口内的总可用时间 */
        u64 time_window = 1000000000ULL; /* 1秒窗口（纳秒） */
        u64 available_time = time_window * load_info->logical_core_count;
        
        if (available_time > 0 && total_used_time > 0) {
            /* 负载 = 已用时间 / 可用时间 * 100 */
            u64 usage_ratio = (total_used_time * 100) / available_time;
            load_info->load_percentage = (u32)(usage_ratio > 100 ? 100 : usage_ratio);
        } else {
            load_info->load_percentage = 0;
        }
        
        /* 缓存压力估算：基于活跃核心数和迁移历史 */
        u32 migration_factor = 0;
        for (u32 i = 0; i < g_physical_core_load[physical_core_id]; i++) {
            logical_core_id_t lcore_id = g_physical_to_logical_map[physical_core_id][i];
            logical_core_t *core = &g_logical_cores[lcore_id];
            /* 近期有迁移的核心会增加缓存压力 */
            if (current_time - core->mapping.last_migration_time < time_window) {
                migration_factor += 10;
            }
        }
        
        load_info->cache_pressure = (load_info->logical_core_count * 50) / 16 + migration_factor;
        if (load_info->cache_pressure > 100) {
            load_info->cache_pressure = 100;
        }
        
        /* 内存带宽估算 */
        load_info->memory_bandwidth_usage = load_info->load_percentage;
    }
    
    return HIC_SUCCESS;
}

/**
 * 评估迁移候选
 */
hic_status_t logical_core_evaluate_migration(logical_core_id_t logical_core_id,
                                             physical_core_id_t target_physical_core,
                                             const migration_config_t *config,
                                             migration_candidate_t *candidate) {
    if (candidate == NULL || config == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memzero(candidate, sizeof(migration_candidate_t));
    candidate->logical_core_id = logical_core_id;
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    candidate->source_core = core->mapping.physical_core_id;
    candidate->target_core = target_physical_core;
    
    /* 检查是否允许迁移 */
    if (!(core->flags & LOGICAL_CORE_FLAG_MIGRATABLE) ||
        (core->flags & LOGICAL_CORE_FLAG_PINNED)) {
        candidate->recommended = false;
        return HIC_SUCCESS;
    }
    
    /* 检查亲和性 */
    u64 mask_index = target_physical_core / 64;
    u64 mask_bit = 1ULL << (target_physical_core % 64);
    if (!(core->affinity.mask[mask_index] & mask_bit)) {
        candidate->recommended = false;
        return HIC_SUCCESS;
    }
    
    /* 检查迁移冷却时间 */
    u64 current_time = hal_get_timestamp();
    u64 cooldown_ns = (u64)config->migration_cooldown_ms * 1000000ULL;
    if (current_time - core->mapping.last_migration_time < cooldown_ns) {
        candidate->recommended = false;
        return HIC_SUCCESS;
    }
    
    /* 获取源和目标核心的负载信息 */
    physical_core_load_info_t source_load, target_load;
    logical_core_calculate_load(candidate->source_core, &source_load);
    logical_core_calculate_load(target_physical_core, &target_load);
    
    /* 计算迁移收益：负载差异 */
    i32 load_diff = (i32)source_load.load_percentage - (i32)target_load.load_percentage;
    candidate->migration_benefit = (load_diff > 0) ? (u32)load_diff : 0;
    
    /* 计算迁移成本：
     * 1. 缓存亲和性损失
     * 2. 迁移历史（频繁迁移增加成本）
     * 3. 当前活跃状态
     */
    u32 cache_cost = (core->perf.cache_misses > 0) ? 
                     (u32)(core->perf.cache_misses * config->cache_affinity_weight / 100) : 0;
    u32 migration_history_cost = (u32)(core->mapping.migration_count * 5);
    u32 active_cost = (core->state == LOGICAL_CORE_STATE_ACTIVE) ? 20 : 0;
    
    candidate->migration_cost = cache_cost + migration_history_cost + active_cost;
    
    /* 综合评估：收益大于成本时推荐迁移 */
    candidate->recommended = (candidate->migration_benefit > candidate->migration_cost) &&
                            (candidate->migration_benefit > config->load_balance_threshold);
    
    return HIC_SUCCESS;
}

/**
 * 获取系统整体利用率
 */
u32 logical_core_get_system_utilization(void) {
    u32 total_cores = 0;
    u32 active_cores = 0;
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        logical_core_t *core = &g_logical_cores[i];
        
        if (core->state != LOGICAL_CORE_STATE_FREE) {
            total_cores++;
            if (core->state == LOGICAL_CORE_STATE_ACTIVE) {
                active_cores++;
            }
        }
    }
    
    if (total_cores == 0) {
        return 0;
    }
    
    return (active_cores * 100) / total_cores;
}

/**
 * 获取指定域的核心利用率
 */
u32 logical_core_get_domain_utilization(domain_id_t domain_id) {
    u32 domain_cores = 0;
    u32 domain_active = 0;
    
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        logical_core_t *core = &g_logical_cores[i];
        
        if (core->owner_domain == domain_id && 
            core->state != LOGICAL_CORE_STATE_FREE) {
            domain_cores++;
            if (core->state == LOGICAL_CORE_STATE_ACTIVE) {
                domain_active++;
            }
        }
    }
    
    if (domain_cores == 0) {
        return 0;
    }
    
    return (domain_active * 100) / domain_cores;
}

/**
 * 增强的迁移决策
 */
hic_status_t logical_core_enhanced_migration_decision(
    const migration_config_t *config,
    u32 *migrations_executed) {
    
    if (config == NULL || migrations_executed == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    *migrations_executed = 0;
    
    /* 首先检查借用到期 */
    logical_core_check_borrow_expiry();
    
    /* 获取物理核心数量 */
    extern cpu_info_t g_cpu_info;
    u32 physical_core_count = g_cpu_info.physical_cores;
    if (physical_core_count == 0) {
        physical_core_count = 1;
    }
    
    /* 收集所有物理核心的负载信息 */
    physical_core_load_info_t load_infos[MAX_PHYSICAL_CORES];
    for (u32 i = 0; i < physical_core_count; i++) {
        logical_core_calculate_load(i, &load_infos[i]);
    }
    
    /* 计算平均负载 */
    u32 total_load = 0;
    for (u32 i = 0; i < physical_core_count; i++) {
        total_load += load_infos[i].load_percentage;
    }
    u32 avg_load = total_load / physical_core_count;
    
    /* 识别过载和低载核心 */
    physical_core_id_t overloaded_cores[MAX_PHYSICAL_CORES];
    physical_core_id_t underloaded_cores[MAX_PHYSICAL_CORES];
    u32 overloaded_count = 0;
    u32 underloaded_count = 0;
    
    for (u32 i = 0; i < physical_core_count; i++) {
        /* 过载：负载超过平均值+阈值 */
        if (load_infos[i].load_percentage > avg_load + config->load_balance_threshold) {
            overloaded_cores[overloaded_count++] = i;
        }
        /* 低载：负载低于平均值-阈值 */
        else if (load_infos[i].load_percentage < avg_load - config->load_balance_threshold ||
                 load_infos[i].load_percentage < 20) {  /* 负载低于20%视为低载 */
            underloaded_cores[underloaded_count++] = i;
        }
    }
    
    /* 如果没有需要平衡的核心，返回 */
    if (overloaded_count == 0 || underloaded_count == 0) {
        return HIC_SUCCESS;
    }
    
    /* 评估迁移候选 */
    migration_candidate_t candidates[MAX_LOGICAL_CORES];
    u32 candidate_count = 0;
    
    for (u32 i = 0; i < overloaded_count; i++) {
        physical_core_id_t src_core = overloaded_cores[i];
        
        /* 遍历该物理核心上的逻辑核心 */
        for (u32 j = 0; j < g_physical_core_load[src_core]; j++) {
            logical_core_id_t lcore_id = g_physical_to_logical_map[src_core][j];
            
            /* 尝试找到最佳目标核心 */
            for (u32 k = 0; k < underloaded_count; k++) {
                physical_core_id_t target_core = underloaded_cores[k];
                
                migration_candidate_t candidate;
                if (logical_core_evaluate_migration(lcore_id, target_core, 
                                                   config, &candidate) == HIC_SUCCESS &&
                    candidate.recommended) {
                    
                    candidates[candidate_count++] = candidate;
                    
                    if (candidate_count >= MAX_LOGICAL_CORES) {
                        break;
                    }
                }
            }
        }
    }
    
    /* 按迁移收益排序（简单的选择排序） */
    for (u32 i = 0; i < candidate_count - 1; i++) {
        u32 max_idx = i;
        for (u32 j = i + 1; j < candidate_count; j++) {
            i32 benefit_i = (i32)candidates[j].migration_benefit - (i32)candidates[j].migration_cost;
            i32 benefit_max = (i32)candidates[max_idx].migration_benefit - (i32)candidates[max_idx].migration_cost;
            if (benefit_i > benefit_max) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            migration_candidate_t temp = candidates[i];
            candidates[i] = candidates[max_idx];
            candidates[max_idx] = temp;
        }
    }
    
    /* 执行迁移 */
    u32 max_migrations = (config->max_migrations_per_cycle < candidate_count) ?
                         config->max_migrations_per_cycle : candidate_count;
    
    for (u32 i = 0; i < max_migrations; i++) {
        migration_candidate_t *c = &candidates[i];
        
        /* 再次检查目标核心是否有空间 */
        if (g_physical_core_load[c->target_core] >= 16) {
            continue;
        }
        
        /* 执行迁移 */
        if (logical_core_perform_migration(c->logical_core_id, c->target_core)) {
            (*migrations_executed)++;
            
            console_puts("[LCORE] Migrated core ");
            console_putu32(c->logical_core_id);
            console_puts(" from physical ");
            console_putu32(c->source_core);
            console_puts(" to ");
            console_putu32(c->target_core);
            console_puts(" (benefit: ");
            console_putu32(c->migration_benefit);
            console_puts(", cost: ");
            console_putu32(c->migration_cost);
            console_puts(")\n");
        }
    }
    
    /* 输出系统利用率统计 */
    if (*migrations_executed > 0) {
        console_puts("[LCORE] System utilization: ");
        console_putu32(logical_core_get_system_utilization());
        console_puts("%\n");
    }
    
    return HIC_SUCCESS;
}

