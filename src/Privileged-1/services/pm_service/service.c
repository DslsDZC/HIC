/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理服务实现 (策略层)
 */

#include "service.h"

/* ========== 外部接口 ========== */

extern void console_puts(const char *s);
extern void console_putu32(u32 v);

/* ========== 策略状态 ========== */

static struct {
    pm_policy_config_t config;
    bool initialized;
    
    /* 空闲历史 */
    u32 idle_history[16];
    u32 idle_history_idx;
    u32 idle_history_count;
    
    /* 统计 */
    u64 total_idle_time;
} g_pm_svc = {0};

/* ========== 预设策略 ========== */

static const pm_policy_config_t g_presets[] = {
    [PM_POLICY_PERFORMANCE] = {
        .mode = PM_POLICY_PERFORMANCE,
        .min_freq_khz = 2000000,
        .max_freq_khz = 4000000,
        .turbo_enabled = true,
        .max_cstate = 2,
        .idle_timeout_ms = 300000,
        .suspend_enabled = true,
    },
    [PM_POLICY_BALANCED] = {
        .mode = PM_POLICY_BALANCED,
        .min_freq_khz = 800000,
        .max_freq_khz = 3000000,
        .turbo_enabled = true,
        .max_cstate = 4,
        .idle_timeout_ms = 180000,
        .suspend_enabled = true,
    },
    [PM_POLICY_POWERSAVE] = {
        .mode = PM_POLICY_POWERSAVE,
        .min_freq_khz = 400000,
        .max_freq_khz = 2000000,
        .turbo_enabled = false,
        .max_cstate = 6,
        .idle_timeout_ms = 60000,
        .suspend_enabled = true,
        .hibernate_enabled = true,
    },
};

/* ========== 初始化 ========== */

void pm_service_init(void)
{
    console_puts("[PM_SVC] Initializing...\n");
    g_pm_svc.config = g_presets[PM_POLICY_BALANCED];
    g_pm_svc.initialized = true;
}

/* ========== 策略配置 ========== */

hic_status_t pm_service_set_policy(pm_policy_mode_t mode)
{
    if (mode > PM_POLICY_CUSTOM) return HIC_INVALID_PARAM;
    if (mode < PM_POLICY_CUSTOM) {
        g_pm_svc.config = g_presets[mode];
    }
    return HIC_SUCCESS;
}

pm_policy_mode_t pm_service_get_policy(void)
{
    return g_pm_svc.config.mode;
}

hic_status_t pm_service_configure(const pm_policy_config_t *config)
{
    if (!config) return HIC_INVALID_PARAM;
    g_pm_svc.config = *config;
    g_pm_svc.config.mode = PM_POLICY_CUSTOM;
    return HIC_SUCCESS;
}

/* ========== DVFS 策略 ========== */

u32 pm_service_select_frequency(u32 domain_id, u32 load_percent)
{
    u32 min = g_pm_svc.config.min_freq_khz;
    u32 max = g_pm_svc.config.max_freq_khz;
    
    u32 freq;
    switch (g_pm_svc.config.mode) {
        case PM_POLICY_PERFORMANCE:
            freq = g_pm_svc.config.turbo_enabled ? (u32)(max * 1.2) : max;
            break;
        case PM_POLICY_POWERSAVE:
            freq = min + (max - min) * load_percent / 200;
            break;
        default:
            freq = min + (max - min) * load_percent / 100;
            break;
    }
    (void)domain_id;
    return freq;
}

void pm_service_adjust_frequency(u32 domain_id, u32 target_freq)
{
    /* 调用 Core-0 机制 */
    extern hic_status_t pm_set_frequency(u32 domain_id, u32 freq_khz);
    pm_set_frequency(domain_id, target_freq);
}

/* ========== 空闲预测 ========== */

u32 pm_service_predict_cstate(void)
{
    if (g_pm_svc.config.predict_strategy == IDLE_PREDICT_DISABLED ||
        g_pm_svc.idle_history_count == 0) {
        return 1;
    }
    
    u64 total = 0;
    u32 count = g_pm_svc.idle_history_count < 16 ? g_pm_svc.idle_history_count : 16;
    for (u32 i = 0; i < count; i++) {
        total += g_pm_svc.idle_history[i];
    }
    u32 avg = (u32)(total / count);
    
    u32 cstate;
    if (avg < 10) cstate = 1;
    else if (avg < 50) cstate = 2;
    else if (avg < 100) cstate = 3;
    else if (avg < 200) cstate = 4;
    else cstate = 6;
    
    if (cstate > g_pm_svc.config.max_cstate) cstate = g_pm_svc.config.max_cstate;
    return cstate;
}

void pm_service_record_idle(u32 duration_us)
{
    g_pm_svc.idle_history[g_pm_svc.idle_history_idx] = duration_us;
    g_pm_svc.idle_history_idx = (g_pm_svc.idle_history_idx + 1) % 16;
    if (g_pm_svc.idle_history_count < 16) g_pm_svc.idle_history_count++;
    g_pm_svc.total_idle_time += duration_us;
}

/* ========== 睡眠策略 ========== */

bool pm_service_should_suspend(void)
{
    if (!g_pm_svc.config.suspend_enabled) return false;
    u64 idle_ms = g_pm_svc.total_idle_time / 1000;
    return idle_ms >= g_pm_svc.config.idle_timeout_ms;
}

bool pm_service_should_hibernate(void)
{
    if (!g_pm_svc.config.hibernate_enabled) return false;
    u64 idle_ms = g_pm_svc.total_idle_time / 1000;
    return idle_ms >= g_pm_svc.config.idle_timeout_ms * 2;
}

void pm_service_enter_sleep(void)
{
    extern hic_status_t pm_set_sstate(u32 state);
    if (pm_service_should_hibernate()) {
        pm_set_sstate(4);  /* S4 */
    } else if (pm_service_should_suspend()) {
        pm_set_sstate(3);  /* S3 */
    }
}

/* ========== 主循环 ========== */

void pm_service_run(void)
{
    /* DVFS 调整 */
    u32 freq = pm_service_select_frequency(0, 50);
    pm_service_adjust_frequency(0, freq);
    
    /* 检查睡眠 */
    pm_service_enter_sleep();
}