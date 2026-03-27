/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 逻辑核心机制层 (Logical Core Mechanism Layer)
 * 
 * Core-0 提供逻辑核心操作的原语接口：
 * - 逻辑核心分配/释放
 * - 物理核心映射
 * - 亲和性设置
 * - 迁移执行
 * - 性能计数器
 * - 借用/归还操作
 * 
 * 策略层（scheduler_service）负责：
 * - 迁移决策算法
 * - 负载均衡策略
 * - 借用策略配置
 * - 调度策略选择
 */

#ifndef HIC_KERNEL_LOGICAL_CORE_H
#define HIC_KERNEL_LOGICAL_CORE_H

#include "types.h"
#include "capability.h"
#include "domain.h"

/* ==================== 类型定义 ==================== */

typedef u32 logical_core_flags_t;
typedef u8 logical_core_quota_t;
typedef u32 physical_core_id_t;
#define INVALID_PHYSICAL_CORE ((physical_core_id_t)-1)

/* 逻辑核心状态 */
typedef enum {
    LOGICAL_CORE_STATE_FREE,
    LOGICAL_CORE_STATE_ALLOCATED,
    LOGICAL_CORE_STATE_ACTIVE,
    LOGICAL_CORE_STATE_MIGRATING,
    LOGICAL_CORE_STATE_SUSPENDED,
} logical_core_state_t;

/* 借用状态 */
typedef enum {
    BORROW_STATE_NONE,
    BORROW_STATE_BORROWED,
    BORROW_STATE_RETURNING,
} borrow_state_t;

/* 亲和性掩码 */
typedef struct {
    u64 mask[4];
} logical_core_affinity_t;

/* 映射信息 */
typedef struct {
    physical_core_id_t physical_core_id;
    u64 migration_count;
    u64 last_migration_time;
} logical_core_mapping_t;

/* 配额信息 */
typedef struct {
    logical_core_quota_t guaranteed_quota;
    logical_core_quota_t max_quota;
    u64 used_time;
    u64 allocated_time;
} logical_core_quota_info_t;

/* 性能计数器 */
typedef struct {
    u64 instructions_retired;
    u64 cycles_executed;
    u64 cache_hits;
    u64 cache_misses;
    u64 branch_hits;
    u64 branch_misses;
    u64 migration_latency_sum;
    u64 migration_count;
} logical_core_perf_t;

/* 逻辑核心控制块（机制层内部） */
typedef struct logical_core {
    logical_core_id_t logical_core_id;
    logical_core_state_t state;
    logical_core_flags_t flags;
    
    domain_id_t owner_domain;
    cap_handle_t capability_handle;
    
    logical_core_mapping_t mapping;
    logical_core_affinity_t affinity;
    logical_core_quota_info_t quota;
    
    thread_id_t running_thread;
    u64 last_schedule_time;
    
    logical_core_perf_t perf;
    
    struct logical_core *next;
    struct logical_core *prev;
} logical_core_t;

/* 借用信息 */
typedef struct {
    borrow_state_t state;
    domain_id_t original_owner;
    domain_id_t borrower_domain;
    cap_handle_t original_cap_handle;
    cap_handle_t derived_cap_handle;
    u64 borrow_start_time;
    u64 borrow_duration;
    u64 borrow_deadline;
    u32 borrow_quota_used;
} logical_core_borrow_info_t;

/* 物理核心负载信息 */
typedef struct {
    u32 logical_core_count;
    u32 active_thread_count;
    u64 total_cpu_time;
    u64 idle_time;
    u32 load_percentage;
    u32 cache_pressure;
    u32 memory_bandwidth_usage;
} physical_core_load_info_t;

/* 逻辑核心信息（供策略层查询） */
typedef struct {
    logical_core_id_t id;
    logical_core_state_t state;
    domain_id_t owner_domain;
    physical_core_id_t physical_core;
    logical_core_quota_t quota;
    u64 used_time;
} logical_core_info_t;

/* ==================== 常量定义 ==================== */

#define MAX_LOGICAL_CORES         1024
#define MAX_PHYSICAL_CORES        256
#define INVALID_LOGICAL_CORE      ((logical_core_id_t)-1)
#define LOGICAL_CORE_QUOTA_MIN    1
#define LOGICAL_CORE_QUOTA_MAX    100
#define LOGICAL_CORE_DEFAULT_QUOTA 10

/* 逻辑核心标志已在 types.h 中定义 */

/* 迁移决策配置 */
typedef struct {
    u32 load_balance_threshold;
    u32 migration_cooldown_ms;
    u32 max_migrations_per_cycle;
    u32 cache_affinity_weight;
    u32 memory_locality_weight;
    bool enable_predictive;
} migration_config_t;

/* 迁移候选评估结果 */
typedef struct {
    logical_core_id_t logical_core_id;
    physical_core_id_t source_core;
    physical_core_id_t target_core;
    u32 migration_benefit;
    u32 migration_cost;
    bool recommended;
} migration_candidate_t;

/* 默认迁移配置 */
#define DEFAULT_MIGRATION_CONFIG { \
    .load_balance_threshold = 20, \
    .migration_cooldown_ms = 100, \
    .max_migrations_per_cycle = 2, \
    .cache_affinity_weight = 30, \
    .memory_locality_weight = 20, \
    .enable_predictive = false \
}

/* ==================== 全局数据结构（机制层内部） ==================== */

extern logical_core_t g_logical_cores[MAX_LOGICAL_CORES];
extern physical_core_id_t g_logical_to_physical_map[MAX_LOGICAL_CORES];
extern logical_core_id_t g_physical_to_logical_map[MAX_PHYSICAL_CORES][16];
extern u32 g_physical_core_load[MAX_PHYSICAL_CORES];
extern logical_core_t *g_free_logical_cores;
extern u64 g_free_lcore_bitmap[16];
extern u32 g_free_lcore_count;

/* ==================== 机制层接口 ==================== */

/* 初始化 */
void logical_core_system_init(void);

/* 分配/释放 */
hic_status_t hic_logical_core_allocate(domain_id_t domain, u32 count,
                                        logical_core_flags_t flags,
                                        logical_core_quota_t quota,
                                        const logical_core_affinity_t *affinity,
                                        cap_handle_t out_handles[]);

hic_status_t hic_logical_core_release(domain_id_t domain,
                                       const cap_handle_t handles[], u32 count);

/* 查询 */
hic_status_t hic_logical_core_get_info(cap_handle_t handle, logical_core_t *info);
u32 logical_core_get_count(void);
u32 logical_core_get_free_count(void);

/* 映射与亲和性 */
hic_status_t hic_logical_core_set_affinity(cap_handle_t handle,
                                            const logical_core_affinity_t *affinity);
physical_core_id_t hic_logical_core_get_physical(cap_handle_t handle);

/* 迁移（机制层执行，策略层决策） */
hic_status_t hic_logical_core_migrate(cap_handle_t handle, physical_core_id_t target);
u64 hic_logical_core_get_migration_count(cap_handle_t handle);

/* 性能计数器 */
hic_status_t hic_logical_core_get_perf(cap_handle_t handle, logical_core_perf_t *perf);
hic_status_t hic_logical_core_reset_perf(cap_handle_t handle);

/* 借用机制 */
hic_status_t hic_logical_core_borrow(domain_id_t borrower, domain_id_t owner,
                                      u64 duration, u32 quota,
                                      const logical_core_affinity_t *affinity,
                                      cap_handle_t *out_handle);

hic_status_t hic_logical_core_return(domain_id_t borrower, cap_handle_t handle);
hic_status_t hic_logical_core_get_borrow_info(cap_handle_t handle,
                                               logical_core_borrow_info_t *info);
void logical_core_check_borrow_expiry(void);
hic_status_t hic_logical_core_get_borrowable(domain_id_t source_domain,
                                             logical_core_id_t out_ids[],
                                             u32 max_count, u32 *out_actual_count);

/* 调度器集成（内部使用） */
struct thread;  /* 前向声明 */
logical_core_id_t logical_core_schedule_select(struct thread *thread);
void logical_core_schedule_notify(logical_core_id_t id, thread_id_t thread_id, bool starting);
void logical_core_update_quotas(void);
void logical_core_migration_decision(void);

/* 负载计算（供策略层使用） */
hic_status_t logical_core_calculate_load(physical_core_id_t physical_core_id,
                                         physical_core_load_info_t *load_info);
u32 logical_core_get_system_utilization(void);
u32 logical_core_get_domain_utilization(domain_id_t domain_id);

/* 迁移决策（策略层调用） */
hic_status_t logical_core_enhanced_migration_decision(const migration_config_t *config,
                                                       u32 *migrations_executed);
hic_status_t logical_core_evaluate_migration(logical_core_id_t logical_core_id,
                                             physical_core_id_t target_physical_core,
                                             const migration_config_t *config,
                                             migration_candidate_t *candidate);

/* 句柄验证（内部使用） */
logical_core_id_t logical_core_validate_handle(domain_id_t domain, cap_handle_t handle);
logical_core_id_t logical_core_find_free(logical_core_flags_t flags,
                                         logical_core_quota_t quota,
                                         const logical_core_affinity_t *affinity);
cap_handle_t logical_core_allocate_to_domain(logical_core_id_t logical_core_id,
                                             domain_id_t domain_id,
                                             logical_core_flags_t flags,
                                             logical_core_quota_t quota,
                                             const logical_core_affinity_t *affinity);
bool logical_core_perform_migration(logical_core_id_t logical_core_id,
                                    physical_core_id_t target_physical_core_id);

#endif /* HIC_KERNEL_LOGICAL_CORE_H */