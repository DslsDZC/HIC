/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理机制层 (Power Management Mechanism Layer)
 * 
 * Core-0 提供电源状态转换的原语接口：
 * - CPU 电源状态 (C-states) 转换
 * - 系统电源状态 (S-states) 转换
 * - 频率/电压调节
 * - 功耗域开关
 * 
 * 策略层（pm_service）负责：
 * - DVFS 模式选择策略
 * - 空闲预测算法
 * - 设备电源管理策略
 * - 系统睡眠决策
 */

#ifndef HIC_KERNEL_PM_H
#define HIC_KERNEL_PM_H

#include "types.h"

/* ========== CPU 电源状态 (C-states) ========== */

typedef enum pm_cstate {
    PM_CSTATE_C0 = 0,       /* 活跃状态 */
    PM_CSTATE_C1,           /* HLT/Wait */
    PM_CSTATE_C2,           /* Stop-Clock */
    PM_CSTATE_C3,           /* Sleep */
    PM_CSTATE_C4,           /* Deeper Sleep */
    PM_CSTATE_C6,           /* Deep Power Down */
    PM_CSTATE_MAX
} pm_cstate_t;

typedef struct pm_cstate_info {
    pm_cstate_t state;
    const char *name;
    u32 latency_us;         /* 进入/退出延迟 */
    u32 power_mw;           /* 功耗 */
    u32 flags;
} pm_cstate_info_t;

#define PM_CSTATE_FLAG_CACHE_FLUSH   (1 << 0)
#define PM_CSTATE_FLAG_TLB_FLUSH     (1 << 1)
#define PM_CSTATE_FLAG_CONTEXT_SAVE  (1 << 2)
#define PM_CSTATE_FLAG_HALT_ONLY     (1 << 3)

/* ========== 系统电源状态 (S-states) ========== */

typedef enum pm_sstate {
    PM_SSTATE_S0 = 0,       /* 工作 */
    PM_SSTATE_S1,           /* 待机 */
    PM_SSTATE_S3,           /* 挂起到内存 */
    PM_SSTATE_S4,           /* 挂起到磁盘 */
    PM_SSTATE_S5,           /* 软关机 */
    PM_SSTATE_MAX
} pm_sstate_t;

typedef struct pm_sstate_info {
    pm_sstate_t state;
    const char *name;
    u32 latency_ms;
    u32 power_mw;
    bool supported;
} pm_sstate_info_t;

/* ========== 功耗域 ========== */

typedef enum pm_domain_type {
    PM_DOMAIN_CPU = 0,
    PM_DOMAIN_GPU,
    PM_DOMAIN_MEMORY,
    PM_DOMAIN_DEVICE,
    PM_DOMAIN_MAX
} pm_domain_type_t;

/* ========== 频率/电压调节 ========== */

typedef struct pm_freq_level {
    u32 freq_khz;
    u32 voltage_mv;
    u32 power_mw;
} pm_freq_level_t;

/* ========== 架构操作回调（HAL实现） ========== */

typedef struct pm_arch_ops {
    /* C-state 操作 */
    hic_status_t (*enter_cstate)(pm_cstate_t state);
    hic_status_t (*exit_cstate)(void);
    
    /* S-state 操作 */
    hic_status_t (*enter_sstate)(pm_sstate_t state);
    void (*reboot)(void);
    void (*shutdown)(void);
    
    /* 频率/电压操作 */
    hic_status_t (*set_frequency)(u32 domain_id, u32 freq_khz);
    u32 (*get_frequency)(u32 domain_id);
    
    /* 功耗域操作 */
    hic_status_t (*domain_power_on)(u32 domain_id);
    hic_status_t (*domain_power_off)(u32 domain_id);
} pm_arch_ops_t;

/* ========== 统计数据（供策略层查询） ========== */

typedef struct pm_stats {
    pm_cstate_t current_cstate;
    pm_cstate_t max_cstate;
    pm_sstate_t current_sstate;
    u64 total_idle_time_us;
    u64 total_wakeups;
    u32 last_idle_duration_us;
    u32 domain_count;
} pm_stats_t;

/* ========== 机制层接口 ========== */

/* 初始化 */
void pm_init(void);
void pm_shutdown(void);
void pm_register_arch_ops(const pm_arch_ops_t *ops);

/* C-state 操作 */
pm_cstate_t pm_get_cstate(void);
hic_status_t pm_set_cstate(pm_cstate_t state);
pm_cstate_info_t* pm_get_cstate_info(pm_cstate_t state);
u32 pm_get_supported_cstates(void);

/* S-state 操作 */
pm_sstate_t pm_get_sstate(void);
hic_status_t pm_set_sstate(pm_sstate_t state);
bool pm_sstate_supported(pm_sstate_t state);
hic_status_t pm_reboot(void);
hic_status_t pm_shutdown_system(void);

/* 频率操作 */
u32 pm_get_frequency(u32 domain_id);
hic_status_t pm_set_frequency(u32 domain_id, u32 freq_khz);

/* 功耗域操作 */
hic_status_t pm_domain_power_on(u32 domain_id);
hic_status_t pm_domain_power_off(u32 domain_id);
bool pm_domain_is_on(u32 domain_id);
u32 pm_get_domain_count(void);

/* 统计查询 */
void pm_get_stats(pm_stats_t *stats);

/* 空闲入口（由调度器调用，策略层决定进入哪个C-state） */
void pm_enter_cstate(pm_cstate_t state);
void pm_exit_cstate(void);

#endif /* HIC_KERNEL_PM_H */
