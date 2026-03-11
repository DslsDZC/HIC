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
    
    /* 如果没有亲和性要求或没有匹配的，选择负载最轻的物理核心 */
    if (physical_core_id == INVALID_PHYSICAL_CORE) {
        u32 min_load = 0xFFFFFFFF;
        for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
            if (g_physical_core_load[i] < min_load) {
                min_load = g_physical_core_load[i];
                physical_core_id = i;
            }
        }
    }
    
    /* 更新映射表 */
    core->mapping.physical_core_id = physical_core_id;
    g_logical_to_physical_map[logical_core_id] = physical_core_id;
    
    /* 添加到物理核心的负载列表 */
    if (physical_core_id != INVALID_PHYSICAL_CORE) {
        u32 load = g_physical_core_load[physical_core_id];
        if (load < 16) {
            g_physical_to_logical_map[physical_core_id][load] = logical_core_id;
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
    
    /* 从旧物理核心移除 */
    if (old_physical_core_id != INVALID_PHYSICAL_CORE) {
        for (u32 i = 0; i < g_physical_core_load[old_physical_core_id]; i++) {
            if (g_physical_to_logical_map[old_physical_core_id][i] == logical_core_id) {
                /* 移动最后一个元素到当前位置 */
                g_physical_to_logical_map[old_physical_core_id][i] = 
                    g_physical_to_logical_map[old_physical_core_id][g_physical_core_load[old_physical_core_id] - 1];
                g_physical_core_load[old_physical_core_id]--;
                break;
            }
        }
    }
    
    /* 添加到新物理核心 */
    u32 new_load = g_physical_core_load[target_physical_core_id];
    g_physical_to_logical_map[target_physical_core_id][new_load] = logical_core_id;
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
        
        /* 从物理核心负载列表中移除 */
        physical_core_id_t physical_core_id = core->mapping.physical_core_id;
        if (physical_core_id != INVALID_PHYSICAL_CORE) {
            for (u32 j = 0; j < g_physical_core_load[physical_core_id]; j++) {
                if (g_physical_to_logical_map[physical_core_id][j] == logical_core_id) {
                    /* 移动最后一个元素到当前位置 */
                    g_physical_to_logical_map[physical_core_id][j] = 
                        g_physical_to_logical_map[physical_core_id][g_physical_core_load[physical_core_id] - 1];
                    g_physical_core_load[physical_core_id]--;
                    break;
                }
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
    if (logical_core_handle == CAP_HANDLE_INVALID || out_thread_id == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 需要知道调用者的域ID，这里简化处理 */
    extern thread_t *g_current_thread;
    domain_id_t domain_id = g_current_thread ? g_current_thread->domain_id : HIC_DOMAIN_CORE;
    
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
    
    /* 创建线程 */
    thread_id_t thread_id;
    hic_status_t status = thread_create(domain_id, entry_point, priority, &thread_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 绑定线程到逻辑核心 */
    core->running_thread = thread_id;
    core->state = LOGICAL_CORE_STATE_ACTIVE;
    
    /* 设置线程的亲和性（通过线程扩展字段） */
    thread_t *thread = &g_threads[thread_id];
    /* 这里可以添加线程到逻辑核心的绑定信息 */
    
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
    memcpy(info, core, sizeof(logical_core_t));
    
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
    memcpy(perf, &core->perf, sizeof(logical_core_perf_t));
    
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
    
    /* 简化实现：选择负载最轻的物理核心上的空闲逻辑核心 */
    physical_core_id_t best_physical_core = INVALID_PHYSICAL_CORE;
    u32 min_load = 0xFFFFFFFF;
    
    for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
        if (g_physical_core_load[i] < min_load) {
            min_load = g_physical_core_load[i];
            best_physical_core = i;
        }
    }
    
    if (best_physical_core == INVALID_PHYSICAL_CORE) {
        return INVALID_LOGICAL_CORE;
    }
    
    /* 在该物理核心上查找空闲逻辑核心 */
    for (u32 i = 0; i < g_physical_core_load[best_physical_core]; i++) {
        logical_core_id_t logical_core_id = g_physical_to_logical_map[best_physical_core][i];
        logical_core_t *core = &g_logical_cores[logical_core_id];
        
        if (core->state == LOGICAL_CORE_STATE_ALLOCATED && 
            core->running_thread == INVALID_THREAD &&
            core->owner_domain == thread->domain_id) {
            return logical_core_id;
        }
    }
    
    return INVALID_LOGICAL_CORE;
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
            
            /* 检查是否超过配额 */
            if (core->quota.used_time > core->quota.allocated_time) {
                /* 配额超限，可以触发调度或通知监控服务 */
                /* 简化实现：只记录日志 */
                console_puts("[LCORE] Warning: Logical core ");
                console_putu32(i);
                console_puts(" exceeded quota\n");
            }
        }
    }
}

/**
 * 逻辑核心迁移决策
 * 监控服务定期调用，决定是否需要迁移逻辑核心以平衡负载
 */
void logical_core_migration_decision(void) {
    /* 简化实现：基于负载均衡的迁移决策 */
    
    /* 计算每个物理核心的平均负载 */
    u32 total_logical_cores = 0;
    for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
        total_logical_cores += g_physical_core_load[i];
    }
    
    if (total_logical_cores == 0) {
        return;
    }
    
    u32 avg_load = total_logical_cores / MAX_PHYSICAL_CORES;
    
    /* 查找负载过高和过低的物理核心 */
    physical_core_id_t overloaded_core = INVALID_PHYSICAL_CORE;
    physical_core_id_t underloaded_core = INVALID_PHYSICAL_CORE;
    u32 max_load = 0;
    u32 min_load = 0xFFFFFFFF;
    
    for (physical_core_id_t i = 0; i < MAX_PHYSICAL_CORES; i++) {
        u32 load = g_physical_core_load[i];
        if (load > max_load) {
            max_load = load;
            overloaded_core = i;
        }
        if (load < min_load) {
            min_load = load;
            underloaded_core = i;
        }
    }
    
    /* 如果负载差异过大，执行迁移 */
    if (overloaded_core != INVALID_PHYSICAL_CORE && 
        underloaded_core != INVALID_PHYSICAL_CORE &&
        max_load - min_load > 2) {  /* 负载差异阈值 */
        
        /* 从过载核心迁移一个逻辑核心到低负载核心 */
        for (u32 i = 0; i < g_physical_core_load[overloaded_core]; i++) {
            logical_core_id_t logical_core_id = g_physical_to_logical_map[overloaded_core][i];
            logical_core_t *core = &g_logical_cores[logical_core_id];
            
            /* 检查是否允许迁移 */
            if ((core->flags & LOGICAL_CORE_FLAG_MIGRATABLE) &&
                !(core->flags & LOGICAL_CORE_FLAG_PINNED)) {
                
                /* 检查目标核心是否有空间 */
                if (g_physical_core_load[underloaded_core] < 16) {
                    /* 执行迁移 */
                    logical_core_perform_migration(logical_core_id, underloaded_core);
                    break;
                }
            }
        }
    }
}

