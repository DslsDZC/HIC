/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理服务 (策略层)
 * 
 * 实现电源管理策略，调用 Core-0 机制原语：
 * - DVFS 模式选择策略
 * - 空闲预测算法
 * - 系统睡眠决策
 */

#ifndef PM_SERVICE_H
#define PM_SERVICE_H

#include "../../include/common.h"

/* ========== DVFS 策略模式 ========== */

typedef enum pm_policy_mode {
    PM_POLICY_PERFORMANCE = 0,  /* 性能优先：最高频率 */
    PM_POLICY_BALANCED,         /* 平衡模式：动态调节 */
    PM_POLICY_POWERSAVE,        /* 节能优先：低频率 */
    PM_POLICY_CUSTOM,           /* 自定义配置 */
} pm_policy_mode_t;

/* ========== 空闲预测策略 ========== */

typedef enum idle_predict_strategy {
    IDLE_PREDICT_DISABLED = 0,  /* 禁用 */
    IDLE_PREDICT_SIMPLE,        /* 简单：历史平均 */
    IDLE_PREDICT_ADAPTIVE,      /* 自适应：机器学习 */
} idle_predict_strategy_t;

/* ========== 策略配置 ========== */

typedef struct pm_policy_config {
    pm_policy_mode_t mode;
    
    /* DVFS 配置 */
    u32 min_freq_khz;
    u32 max_freq_khz;
    bool turbo_enabled;
    
    /* C-state 配置 */
    u32 max_cstate;             /* 最大允许 C-state */
    u32 cstate_latency_limit;   /* 延迟限制 (us) */
    
    /* 空闲预测配置 */
    idle_predict_strategy_t predict_strategy;
    
    /* 睡眠策略 */
    u32 idle_timeout_ms;        /* 空闲多久进入睡眠 */
    bool suspend_enabled;
    bool hibernate_enabled;
} pm_policy_config_t;

/* ========== 服务接口 ========== */

void pm_service_init(void);

hic_status_t pm_service_set_policy(pm_policy_mode_t mode);
pm_policy_mode_t pm_service_get_policy(void);
hic_status_t pm_service_configure(const pm_policy_config_t *config);

/* DVFS 策略 */
u32 pm_service_select_frequency(u32 domain_id, u32 load_percent);
void pm_service_adjust_frequency(u32 domain_id, u32 target_freq);

/* 空闲预测 */
u32 pm_service_predict_cstate(void);
void pm_service_record_idle(u32 duration_us);

/* 睡眠策略 */
bool pm_service_should_suspend(void);
bool pm_service_should_hibernate(void);
void pm_service_enter_sleep(void);

/* 主循环 */
void pm_service_run(void);

#endif /* PM_SERVICE_H */