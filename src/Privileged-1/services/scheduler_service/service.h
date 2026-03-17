/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 调度器服务策略层 (Scheduler Service Policy Layer)
 * 
 * 实现调度策略决策：
 * - 迁移决策算法
 * - 负载均衡策略
 * - 借用策略配置
 * - 配额管理策略
 */

#ifndef SCHEDULER_SERVICE_H
#define SCHEDULER_SERVICE_H

#include "../../include/common.h"

/* ==================== 迁移策略 ==================== */

typedef struct migration_policy {
    u32 load_balance_threshold;     /* 负载差异阈值（%） */
    u32 migration_cooldown_ms;      /* 迁移冷却时间 */
    u32 max_migrations_per_cycle;   /* 每周期最大迁移数 */
    u32 cache_affinity_weight;      /* 缓存亲和性权重 */
    u32 memory_locality_weight;     /* 内存局部性权重 */
    bool enable_predictive;         /* 预测性迁移 */
    bool enable_auto_balance;       /* 自动负载均衡 */
} migration_policy_t;

typedef struct migration_candidate {
    u32 logical_core_id;
    u32 source_physical;
    u32 target_physical;
    u32 benefit_score;
    u32 cost_score;
    bool recommended;
} migration_candidate_t;

/* ==================== 负载均衡策略 ==================== */

typedef enum balance_strategy {
    BALANCE_STRATEGY_NONE = 0,
    BALANCE_STRATEGY_ROUND_ROBIN,
    BALANCE_STRATEGY_LEAST_LOADED,
    BALANCE_STRATEGY_CACHE_AWARE,
    BALANCE_STRATEGY_ADAPTIVE,
} balance_strategy_t;

typedef struct load_balance_config {
    balance_strategy_t strategy;
    u32 check_interval_ms;
    u32 imbalance_threshold;
    bool prefer_cache_locality;
    bool consider_memory_bandwidth;
} load_balance_config_t;

/* ==================== 配额策略 ==================== */

typedef struct quota_policy {
    u32 default_quota_percent;
    u32 max_quota_percent;
    bool allow_borrowing;
    u32 max_borrow_duration_ms;
    bool enforce_quotas;
} quota_policy_t;

/* ==================== 负载信息 ==================== */

typedef struct physical_core_load {
    u32 core_id;
    u32 logical_core_count;
    u32 active_thread_count;
    u32 load_percentage;
    u32 cache_pressure;            /* 缓存压力 */
    u32 memory_bandwidth_usage;    /* 内存带宽使用 */
    u64 total_cpu_time;
} physical_core_load_t;

typedef struct system_load_info {
    u32 total_cores;
    u32 active_cores;
    u32 system_utilization;
    u32 max_core_load;
    u32 min_core_load;
} system_load_info_t;

/* ==================== 策略层接口 ==================== */

/* 初始化 */
void scheduler_service_init(void);

/* 迁移策略 */
void scheduler_set_migration_policy(const migration_policy_t *policy);
const migration_policy_t* scheduler_get_migration_policy(void);
u32 scheduler_run_migration_cycle(void);  /* 返回执行的迁移数 */

/* 负载均衡 */
void scheduler_set_balance_config(const load_balance_config_t *config);
void scheduler_get_system_load(system_load_info_t *info);
void scheduler_get_core_loads(physical_core_load_t *loads, u32 *count);
u32 scheduler_select_target_core(u32 source_core);

/* 配额策略 */
void scheduler_set_quota_policy(const quota_policy_t *policy);
bool scheduler_check_quota_available(u32 domain, u32 requested);

/* 迁移候选评估 */
void scheduler_evaluate_migrations(migration_candidate_t *candidates, u32 max_count, u32 *out_count);
bool scheduler_should_migrate(u32 logical_core_id, u32 target_physical);

/* 服务入口 */
int scheduler_service_start(void);

#endif /* SCHEDULER_SERVICE_H */
