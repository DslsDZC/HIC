/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 调度器服务策略层实现
 * 
 * 调用机制层原语实现策略决策：
 * - 迁移策略配置
 * - 负载均衡策略选择
 * - 配额策略管理
 */

#include "service.h"

/* 外部机制层接口（来自 Core-0） */
extern void logical_core_system_init(void);
extern u32 logical_core_get_count(void);
extern u32 logical_core_get_free_count(void);
extern u32 logical_core_get_system_utilization(void);
extern u32 logical_core_get_domain_utilization(u32 domain);

/* 机制层迁移接口 */
extern u32 logical_core_enhanced_migration_decision(const void *config, u32 *migrations_executed);
extern u32 logical_core_calculate_load(u32 physical_core_id, void *load_info);
extern u32 logical_core_evaluate_migration(u32 logical_core_id, u32 target_physical,
                                           const void *config, void *candidate);

/* 机制层配置类型 */
typedef struct {
    u32 load_balance_threshold;
    u32 migration_cooldown_ms;
    u32 max_migrations_per_cycle;
    u32 cache_affinity_weight;
    u32 memory_locality_weight;
    bool enable_predictive;
} migration_config_t;

/* 日志输出 */
static void log_info(const char *msg) { 
    extern void serial_print(const char *); 
    serial_print("[SCHED_SVC] "); 
    serial_print(msg); 
    serial_print("\n"); 
}

static void log_warn(const char *msg) { 
    extern void serial_print(const char *); 
    serial_print("[SCHED_SVC WARN] "); 
    serial_print(msg); 
    serial_print("\n"); 
}

/* ========== 全局策略配置 ========== */

static migration_policy_t g_migration_policy = {
    .load_balance_threshold = 20,
    .migration_cooldown_ms = 100,
    .max_migrations_per_cycle = 2,
    .cache_affinity_weight = 30,
    .memory_locality_weight = 20,
    .enable_predictive = false,
    .enable_auto_balance = true,
};

static load_balance_config_t g_balance_config = {
    .strategy = BALANCE_STRATEGY_ADAPTIVE,
    .check_interval_ms = 1000,
    .imbalance_threshold = 15,
    .prefer_cache_locality = true,
    .consider_memory_bandwidth = true,
};

static quota_policy_t g_quota_policy = {
    .default_quota_percent = 10,
    .max_quota_percent = 100,
    .allow_borrowing = true,
    .max_borrow_duration_ms = 5000,
    .enforce_quotas = true,
};

/* ========== 初始化 ========== */

void scheduler_service_init(void)
{
    log_info("Scheduler service initializing...");
    
    /* 策略层不直接管理数据，调用机制层获取 */
    log_info("Using mechanism layer for load information");
    
    log_info("Scheduler service ready");
}

/* ========== 迁移策略 ========== */

void scheduler_set_migration_policy(const migration_policy_t *policy)
{
    if (policy) {
        g_migration_policy = *policy;
        log_info("Migration policy updated");
    }
}

const migration_policy_t* scheduler_get_migration_policy(void)
{
    return &g_migration_policy;
}

u32 scheduler_run_migration_cycle(void)
{
    if (!g_migration_policy.enable_auto_balance) {
        return 0;
    }
    
    /* 构造机制层配置 */
    migration_config_t config = {
        .load_balance_threshold = g_migration_policy.load_balance_threshold,
        .migration_cooldown_ms = g_migration_policy.migration_cooldown_ms,
        .max_migrations_per_cycle = g_migration_policy.max_migrations_per_cycle,
        .cache_affinity_weight = g_migration_policy.cache_affinity_weight,
        .memory_locality_weight = g_migration_policy.memory_locality_weight,
        .enable_predictive = g_migration_policy.enable_predictive,
    };
    
    /* 调用机制层执行迁移决策 */
    u32 migrations_executed = 0;
    u32 status = logical_core_enhanced_migration_decision(&config, &migrations_executed);
    
    if (status == 0 && migrations_executed > 0) {
        log_info("Migration cycle completed");
    }
    
    return migrations_executed;
}

/* ========== 负载均衡 ========== */

void scheduler_set_balance_config(const load_balance_config_t *config)
{
    if (config) {
        g_balance_config = *config;
    }
}

void scheduler_get_system_load(system_load_info_t *info)
{
    if (!info) return;
    
    /* 调用机制层获取系统利用率 */
    info->system_utilization = logical_core_get_system_utilization();
    
    /* 其他信息需要从机制层查询 */
    info->total_cores = logical_core_get_count();
    info->active_cores = info->total_cores - logical_core_get_free_count();
    info->max_core_load = 0;  /* 需要遍历物理核心 */
    info->min_core_load = 100;
}

void scheduler_get_core_loads(physical_core_load_t *loads, u32 *count)
{
    if (!loads || !count) return;
    
    /* 调用机制层获取每个物理核心的负载 */
    for (u32 i = 0; i < *count; i++) {
        logical_core_calculate_load(i, &loads[i]);
    }
}

u32 scheduler_select_target_core(u32 source_core)
{
    /* 策略层决策：选择目标核心 */
    u32 target = source_core;
    
    switch (g_balance_config.strategy) {
        case BALANCE_STRATEGY_LEAST_LOADED:
            /* 遍历所有核心找最小负载 */
            {
                u32 min_load = 100;
                physical_core_load_t load;
                for (u32 i = 0; i < 256; i++) {
                    if (logical_core_calculate_load(i, &load) == 0) {
                        if (load.load_percentage < min_load) {
                            min_load = load.load_percentage;
                            target = i;
                        }
                    }
                }
            }
            break;
            
        case BALANCE_STRATEGY_ROUND_ROBIN:
            target = (source_core + 1) % 256;
            break;
            
        case BALANCE_STRATEGY_ADAPTIVE:
        default:
            /* 自适应策略：考虑负载和缓存 */
            {
                u32 min_score = 1000;
                physical_core_load_t load;
                for (u32 i = 0; i < 256; i++) {
                    if (i != source_core && logical_core_calculate_load(i, &load) == 0) {
                        u32 score = load.load_percentage;
                        if (g_balance_config.prefer_cache_locality) {
                            score += load.cache_pressure / 10;
                        }
                        if (score < min_score) {
                            min_score = score;
                            target = i;
                        }
                    }
                }
            }
            break;
    }
    
    return target;
}

/* ========== 配额策略 ========== */

void scheduler_set_quota_policy(const quota_policy_t *policy)
{
    if (policy) {
        g_quota_policy = *policy;
    }
}

bool scheduler_check_quota_available(u32 domain, u32 requested)
{
    if (!g_quota_policy.enforce_quotas) return true;
    
    /* 调用机制层获取域利用率 */
    u32 domain_util = logical_core_get_domain_utilization(domain);
    
    return (domain_util + requested) <= g_quota_policy.max_quota_percent;
}

/* ========== 迁移候选评估 ========== */

void scheduler_evaluate_migrations(migration_candidate_t *candidates, u32 max_count, u32 *out_count)
{
    if (!candidates || !out_count) return;
    
    *out_count = 0;
    
    /* 构造机制层配置 */
    migration_config_t config = {
        .load_balance_threshold = g_migration_policy.load_balance_threshold,
        .migration_cooldown_ms = g_migration_policy.migration_cooldown_ms,
        .max_migrations_per_cycle = max_count,
        .cache_affinity_weight = g_migration_policy.cache_affinity_weight,
        .memory_locality_weight = g_migration_policy.memory_locality_weight,
        .enable_predictive = g_migration_policy.enable_predictive,
    };
    
    /* 遍历逻辑核心，调用机制层评估 */
    u32 total_cores = logical_core_get_count();
    for (u32 i = 0; i < total_cores && *out_count < max_count; i++) {
        /* 选择目标核心 */
        u32 target = scheduler_select_target_core(i);
        
        if (target != i) {
            /* 调用机制层评估迁移 */
            logical_core_evaluate_migration(i, target, &config, &candidates[*out_count]);
            
            if (candidates[*out_count].recommended) {
                (*out_count)++;
            }
        }
    }
}

bool scheduler_should_migrate(u32 logical_core_id, u32 target_physical)
{
    migration_config_t config = {
        .load_balance_threshold = g_migration_policy.load_balance_threshold,
        .migration_cooldown_ms = g_migration_policy.migration_cooldown_ms,
        .max_migrations_per_cycle = 1,
        .cache_affinity_weight = g_migration_policy.cache_affinity_weight,
        .memory_locality_weight = g_migration_policy.memory_locality_weight,
        .enable_predictive = false,
    };
    
    migration_candidate_t candidate;
    logical_core_evaluate_migration(logical_core_id, target_physical, &config, &candidate);
    
    return candidate.recommended;
}

/* ========== 服务入口 ========== */

int scheduler_service_start(void)
{
    scheduler_service_init();
    
    log_info("Scheduler service started");
    log_info("Strategy: Adaptive load balancing");
    log_info("Auto-balance: enabled");
    
    return 0;
}