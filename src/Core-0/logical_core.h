/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC逻辑核心（Logical Core）系统
 * 
 * 逻辑核心是对物理CPU核心的虚拟化抽象，表现为可编程、可控制、可观察的能力对象。
 * 它将物理CPU核心转化为可以被软件显式申请、持有、使用、释放和传递的能力句柄。
 * 
 * 核心概念：
 * 1. 解耦：软件对计算资源的需求与底层物理核心细节分离
 * 2. 可控：开发者真正"拥有"计算单元，而不是临时借用
 * 3. 高效：轻量级抽象，接近裸金属性能
 * 4. 安全：核心本身纳入能力系统，所有操作必须经过授权
 * 5. 可扩展：支持超配（逻辑核心数 > 物理核心数）和透明迁移
 */

#ifndef HIC_KERNEL_LOGICAL_CORE_H
#define HIC_KERNEL_LOGICAL_CORE_H

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "thread.h"

/* ==================== 类型定义 ==================== */

/* 逻辑核心状态 */
typedef enum {
    LOGICAL_CORE_STATE_FREE,        /* 空闲，未分配 */
    LOGICAL_CORE_STATE_ALLOCATED,   /* 已分配，持有者域拥有能力 */
    LOGICAL_CORE_STATE_ACTIVE,      /* 活跃，有线程正在运行 */
    LOGICAL_CORE_STATE_MIGRATING,   /* 迁移中 */
    LOGICAL_CORE_STATE_SUSPENDED,   /* 暂停 */
} logical_core_state_t;

/* 逻辑核心亲和性掩码（位图，支持最多256个物理核心） */
typedef struct {
    u64 mask[4];  /* 256位，每个位对应一个物理核心 */
} logical_core_affinity_t;

/* 逻辑核心映射信息 */
typedef struct {
    physical_core_id_t physical_core_id;  /* 当前映射的物理核心ID */
    u64 migration_count;                   /* 迁移次数 */
    u64 last_migration_time;               /* 最后迁移时间戳 */
} logical_core_mapping_t;

/* 逻辑核心配额信息 */
typedef struct {
    logical_core_quota_t guaranteed_quota;  /* 保证的CPU时间百分比（0-100） */
    logical_core_quota_t max_quota;         /* 最大CPU时间百分比 */
    u64 used_time;                          /* 已使用的CPU时间（纳秒） */
    u64 allocated_time;                     /* 分配的CPU时间（纳秒） */
} logical_core_quota_info_t;

/* 逻辑核心性能计数器 */
typedef struct {
    u64 instructions_retired;      /* 退休指令数 */
    u64 cycles_executed;           /* 执行周期数 */
    u64 cache_hits;                /* 缓存命中数 */
    u64 cache_misses;              /* 缓存未命中数 */
    u64 branch_hits;               /* 分支命中数 */
    u64 branch_misses;             /* 分支未命中数 */
    u64 migration_latency_sum;     /* 迁移延迟总和 */
    u64 migration_count;           /* 迁移次数 */
} logical_core_perf_t;

/* 逻辑核心控制块 */
typedef struct logical_core {
    logical_core_id_t logical_core_id;      /* 逻辑核心ID */
    logical_core_state_t state;             /* 当前状态 */
    logical_core_flags_t flags;             /* 属性标志 */
    
    /* 持有者信息 */
    domain_id_t owner_domain;               /* 拥有者域ID */
    cap_handle_t capability_handle;         /* 对应的能力句柄 */
    
    /* 映射信息 */
    logical_core_mapping_t mapping;         /* 物理核心映射 */
    logical_core_affinity_t affinity;       /* 亲和性掩码 */
    
    /* 配额信息 */
    logical_core_quota_info_t quota;        /* CPU时间配额 */
    
    /* 调度信息 */
    thread_id_t running_thread;             /* 当前运行的线程ID */
    u64 last_schedule_time;                 /* 上次调度时间戳 */
    
    /* 性能计数器 */
    logical_core_perf_t perf;               /* 性能统计 */
    
    /* 链表指针（用于空闲列表和域拥有的核心列表） */
    struct logical_core *next;
    struct logical_core *prev;
} logical_core_t;

/* ==================== 常量定义 ==================== */

#define MAX_LOGICAL_CORES         1024      /* 最大逻辑核心数 */
#define MAX_PHYSICAL_CORES        256       /* 最大物理核心数 */
#define LOGICAL_CORE_QUOTA_MIN    1         /* 最小配额（1%） */
#define LOGICAL_CORE_QUOTA_MAX    100       /* 最大配额（100%） */
#define LOGICAL_CORE_DEFAULT_QUOTA 10       /* 默认配额（10%） */

/* 逻辑核心能力类型 */
#define CAP_TYPE_LOGICAL_CORE     0x4C434F52  /* "LCOR" - Logical Core */

/* ==================== 全局数据结构 ==================== */

/* 逻辑核心表（全局） */
extern logical_core_t g_logical_cores[MAX_LOGICAL_CORES];

/* 逻辑核心到物理核心映射表 */
extern physical_core_id_t g_logical_to_physical_map[MAX_LOGICAL_CORES];

/* 物理核心到逻辑核心反向映射（每个物理核心上的逻辑核心列表） */
extern logical_core_id_t g_physical_to_logical_map[MAX_PHYSICAL_CORES][16]; /* 每个物理核心最多16个逻辑核心 */
extern u32 g_physical_core_load[MAX_PHYSICAL_CORES]; /* 物理核心负载（逻辑核心数） */

/* 空闲逻辑核心列表 */
extern logical_core_t *g_free_logical_cores;

/* ==================== API函数声明 ==================== */

/**
 * @brief 初始化逻辑核心系统
 * 
 * 在系统启动时调用，初始化逻辑核心表、映射表和相关数据结构。
 * 基于硬件探测的物理核心数创建初始的逻辑核心池。
 */
void logical_core_system_init(void);

/**
 * @brief 分配逻辑核心
 * 
 * 为指定域分配一个或多个逻辑核心。分配的逻辑核心作为能力对象返回。
 * 
 * @param domain_id 请求域的ID
 * @param count 请求的逻辑核心数量（1-16）
 * @param flags 逻辑核心属性标志（独占、实时等）
 * @param quota 每个逻辑核心的CPU时间配额（百分比，1-100）
 * @param affinity 亲和性掩码（NULL表示无限制）
 * @param out_handles 输出的能力句柄数组（必须至少能容纳count个元素）
 * @return 状态码
 */
hic_status_t hic_logical_core_allocate(domain_id_t domain_id, u32 count,
                                      logical_core_flags_t flags,
                                      logical_core_quota_t quota,
                                      const logical_core_affinity_t *affinity,
                                      cap_handle_t out_handles[]);

/**
 * @brief 释放逻辑核心
 * 
 * 释放域拥有的逻辑核心，将其返回给系统空闲池。
 * 
 * @param domain_id 域ID
 * @param handles 要释放的能力句柄数组
 * @param count 句柄数量
 * @return 状态码
 */
hic_status_t hic_logical_core_release(domain_id_t domain_id,
                                     const cap_handle_t handles[],
                                     u32 count);

/**
 * @brief 在逻辑核心上创建线程
 * 
 * 在指定的逻辑核心上创建线程。线程将绑定到该逻辑核心执行。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param entry_point 线程入口点
 * @param priority 线程优先级
 * @param arg 传递给线程的参数
 * @param out_thread_id 输出的线程ID
 * @return 状态码
 */
hic_status_t hic_thread_create_on_core(cap_handle_t logical_core_handle,
                                      virt_addr_t entry_point,
                                      priority_t priority,
                                      void *arg,
                                      thread_id_t *out_thread_id);

/**
 * @brief 获取逻辑核心信息
 * 
 * 查询逻辑核心的详细信息，包括状态、映射、配额等。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param info 输出的逻辑核心信息结构
 * @return 状态码
 */
hic_status_t hic_logical_core_get_info(cap_handle_t logical_core_handle,
                                      logical_core_t *info);

/**
 * @brief 设置逻辑核心亲和性
 * 
 * 更新逻辑核心的亲和性掩码，影响后续的调度决策。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param affinity 新的亲和性掩码
 * @return 状态码
 */
hic_status_t hic_logical_core_set_affinity(cap_handle_t logical_core_handle,
                                          const logical_core_affinity_t *affinity);

/**
 * @brief 迁移逻辑核心
 * 
 * 将逻辑核心迁移到指定的物理核心。需要逻辑核心允许迁移。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param physical_core_id 目标物理核心ID
 * @return 状态码
 */
hic_status_t hic_logical_core_migrate(cap_handle_t logical_core_handle,
                                     physical_core_id_t physical_core_id);

/**
 * @brief 获取逻辑核心性能统计
 * 
 * 读取逻辑核心的性能计数器值。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param perf 输出的性能统计结构
 * @return 状态码
 */
hic_status_t hic_logical_core_get_perf(cap_handle_t logical_core_handle,
                                      logical_core_perf_t *perf);

/**
 * @brief 重置逻辑核心性能计数器
 * 
 * 清零逻辑核心的性能计数器。
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @return 状态码
 */
hic_status_t hic_logical_core_reset_perf(cap_handle_t logical_core_handle);

/**
 * @brief 逻辑核心调度器集成
 * 
 * 调度器调用此函数选择在哪个逻辑核心上运行线程。
 * 内部使用，不对外暴露。
 * 
 * @param thread 要调度的线程
 * @return 选择的逻辑核心ID，或INVALID_LOGICAL_CORE
 */
logical_core_id_t logical_core_schedule_select(thread_t *thread);

/**
 * @brief 逻辑核心调度器通知
 * 
 * 调度器在线程开始/停止在逻辑核心上运行时调用此函数。
 * 内部使用，不对外暴露。
 * 
 * @param logical_core_id 逻辑核心ID
 * @param thread_id 线程ID
 * @param starting true=线程开始运行，false=线程停止运行
 */
void logical_core_schedule_notify(logical_core_id_t logical_core_id,
                                 thread_id_t thread_id,
                                 bool starting);

/**
 * @brief 更新逻辑核心配额使用
 * 
 * 定时器中断中调用，更新逻辑核心的CPU时间使用统计。
 * 内部使用，不对外暴露。
 */
void logical_core_update_quotas(void);

/**
 * @brief 逻辑核心迁移决策
 * 
 * 监控服务定期调用，决定是否需要迁移逻辑核心以平衡负载。
 * 内部使用，不对外暴露。
 */
void logical_core_migration_decision(void);

/* ==================== 内部辅助函数 ==================== */

/* 以下函数仅供逻辑核心系统内部使用 */

/**
 * @brief 验证逻辑核心能力句柄
 * 
 * @param domain_id 域ID
 * @param handle 能力句柄
 * @return 验证通过返回逻辑核心ID，否则返回INVALID_LOGICAL_CORE
 */
logical_core_id_t logical_core_validate_handle(domain_id_t domain_id,
                                              cap_handle_t handle);

/**
 * @brief 查找空闲逻辑核心
 * 
 * @param flags 要求的属性标志
 * @param quota 要求的配额
 * @param affinity 要求的亲和性
 * @return 找到的逻辑核心ID，或INVALID_LOGICAL_CORE
 */
logical_core_id_t logical_core_find_free(logical_core_flags_t flags,
                                        logical_core_quota_t quota,
                                        const logical_core_affinity_t *affinity);

/**
 * @brief 分配逻辑核心给域
 * 
 * @param logical_core_id 逻辑核心ID
 * @param domain_id 域ID
 * @param flags 属性标志
 * @param quota CPU时间配额
 * @param affinity 亲和性掩码
 * @return 分配的能力句柄
 */
cap_handle_t logical_core_allocate_to_domain(logical_core_id_t logical_core_id,
                                            domain_id_t domain_id,
                                            logical_core_flags_t flags,
                                            logical_core_quota_t quota,
                                            const logical_core_affinity_t *affinity);

/**
 * @brief 执行逻辑核心迁移
 * 
 * @param logical_core_id 逻辑核心ID
 * @param target_physical_core_id 目标物理核心ID
 * @return 是否成功
 */
bool logical_core_perform_migration(logical_core_id_t logical_core_id,
                                   physical_core_id_t target_physical_core_id);

/* ==================== 借用机制相关定义 ==================== */

/* 借用状态 */
typedef enum {
    BORROW_STATE_NONE,           /* 未被借用 */
    BORROW_STATE_BORROWED,       /* 已被借用 */
    BORROW_STATE_RETURNING,      /* 正在归还 */
} borrow_state_t;

/* 借用信息结构 */
typedef struct logical_core_borrow_info {
    borrow_state_t state;               /* 借用状态 */
    domain_id_t original_owner;          /* 原始拥有者域ID */
    domain_id_t borrower_domain;         /* 借用者域ID */
    cap_handle_t original_cap_handle;    /* 原始能力句柄 */
    cap_handle_t derived_cap_handle;     /* 派生能力句柄 */
    u64 borrow_start_time;               /* 借用开始时间 */
    u64 borrow_duration;                 /* 借用时长（纳秒） */
    u64 borrow_deadline;                 /* 借用截止时间 */
    u32 borrow_quota_used;               /* 借用期间使用的配额 */
} logical_core_borrow_info_t;

/* 物理核心负载详细信息 */
typedef struct {
    u32 logical_core_count;        /* 逻辑核心数量 */
    u32 active_thread_count;       /* 活跃线程数 */
    u64 total_cpu_time;            /* 累计CPU时间 */
    u64 idle_time;                 /* 空闲时间 */
    u32 load_percentage;           /* 负载百分比 (0-100) */
    u32 cache_pressure;            /* 缓存压力指标 */
    u32 memory_bandwidth_usage;    /* 内存带宽使用率 */
} physical_core_load_info_t;

/* 迁移候选评估结果 */
typedef struct {
    logical_core_id_t logical_core_id;
    physical_core_id_t source_core;
    physical_core_id_t target_core;
    u32 migration_benefit;         /* 迁移收益评分 */
    u32 migration_cost;            /* 迁移成本评分 */
    bool recommended;              /* 是否推荐迁移 */
} migration_candidate_t;

/* 迁移决策配置 */
typedef struct {
    u32 load_balance_threshold;     /* 负载均衡阈值（百分比差异） */
    u32 migration_cooldown_ms;      /* 迁移冷却时间（毫秒） */
    u32 max_migrations_per_cycle;   /* 每周期最大迁移数 */
    u32 cache_affinity_weight;      /* 缓存亲和性权重 */
    u32 memory_locality_weight;     /* 内存局部性权重 */
    bool enable_predictive;         /* 启用预测性迁移 */
} migration_config_t;

/* 默认迁移配置 */
#define DEFAULT_MIGRATION_CONFIG { \
    .load_balance_threshold = 20, \
    .migration_cooldown_ms = 100, \
    .max_migrations_per_cycle = 2, \
    .cache_affinity_weight = 30, \
    .memory_locality_weight = 20, \
    .enable_predictive = false \
}

/* ==================== 借用机制API ==================== */

/**
 * @brief 借用逻辑核心
 * 
 * 域A可以通过能力派生临时借用域B空闲的逻辑核心。
 * 借用期间，核心的控制权转移给借用者，但所有权仍归原持有者。
 * 
 * @param borrower_domain 借用者域ID
 * @param source_domain 出借者域ID
 * @param duration 借用时长（纳秒）
 * @param quota 借用期间的配额限制
 * @param affinity 要求的亲和性掩码（可选）
 * @param out_handle 输出的派生能力句柄
 * @return 状态码
 */
hic_status_t hic_logical_core_borrow(domain_id_t borrower_domain,
                                    domain_id_t source_domain,
                                    u64 duration,
                                    u32 quota,
                                    const logical_core_affinity_t *affinity,
                                    cap_handle_t *out_handle);

/**
 * @brief 归还借用的逻辑核心
 * 
 * 借用到期或主动归还逻辑核心。
 * 
 * @param borrower_domain 借用者域ID
 * @param borrowed_handle 借用期间获得的能力句柄
 * @return 状态码
 */
hic_status_t hic_logical_core_return(domain_id_t borrower_domain,
                                     cap_handle_t borrowed_handle);

/**
 * @brief 查询逻辑核心借用状态
 * 
 * @param logical_core_handle 逻辑核心能力句柄
 * @param borrow_info 输出的借用信息
 * @return 状态码
 */
hic_status_t hic_logical_core_get_borrow_info(cap_handle_t logical_core_handle,
                                              logical_core_borrow_info_t *borrow_info);

/**
 * @brief 检查借用是否到期
 * 
 * 检查所有借用的逻辑核心，自动归还已到期的借用。
 * 由监控服务定期调用。
 */
void logical_core_check_borrow_expiry(void);

/**
 * @brief 获取可用于借用的逻辑核心列表
 * 
 * @param source_domain 出借者域ID
 * @param out_ids 输出的逻辑核心ID数组
 * @param max_count 数组最大容量
 * @param out_actual_count 实际返回的数量
 * @return 状态码
 */
hic_status_t hic_logical_core_get_borrowable(domain_id_t source_domain,
                                             logical_core_id_t out_ids[],
                                             u32 max_count,
                                             u32 *out_actual_count);

/* ==================== 增强的迁移决策API ==================== */

/**
 * @brief 增强的迁移决策
 * 
 * 基于实时负载、缓存亲和性、内存局部性等因素，
 * 计算最优迁移方案并执行。
 * 
 * @param config 迁移配置参数
 * @param migrations_executed 输出实际执行的迁移数
 * @return 状态码
 */
hic_status_t logical_core_enhanced_migration_decision(
    const migration_config_t *config,
    u32 *migrations_executed);

/**
 * @brief 计算物理核心负载详情
 * 
 * @param physical_core_id 物理核心ID
 * @param load_info 输出的负载信息
 * @return 状态码
 */
hic_status_t logical_core_calculate_load(physical_core_id_t physical_core_id,
                                         physical_core_load_info_t *load_info);

/**
 * @brief 评估迁移候选
 * 
 * 综合考虑负载均衡、缓存亲和性、迁移成本等因素，
 * 评估是否应该迁移某个逻辑核心。
 * 
 * @param logical_core_id 待评估的逻辑核心ID
 * @param target_physical_core 目标物理核心ID
 * @param config 迁移配置
 * @param candidate 输出的评估结果
 * @return 状态码
 */
hic_status_t logical_core_evaluate_migration(logical_core_id_t logical_core_id,
                                             physical_core_id_t target_physical_core,
                                             const migration_config_t *config,
                                             migration_candidate_t *candidate);

/**
 * @brief 获取系统整体利用率
 * 
 * @return 系统利用率百分比 (0-100)
 */
u32 logical_core_get_system_utilization(void);

/**
 * @brief 获取指定域的核心利用率
 * 
 * @param domain_id 域ID
 * @return 域的核心利用率百分比 (0-100)
 */
u32 logical_core_get_domain_utilization(domain_id_t domain_id);

#endif /* HIC_KERNEL_LOGICAL_CORE_H */