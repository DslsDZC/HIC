/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理抽象层实现
 * 
 * 提供跨平台的电源状态管理功能
 */

#include "pm.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "include/hal.h"

/* 架构操作回调 */
static pm_arch_ops_t g_pm_arch_ops = {0};

/* 全局电源管理状态 */
static pm_state_t g_pm_state = {
    .current_cstate = PM_CSTATE_C0,
    .max_cstate = PM_CSTATE_C1,
    .cstate_count = 2,
    .current_sstate = PM_SSTATE_S0,
    .domain_count = 0,
    .domains = NULL,
    .total_idle_time_us = 0,
    .total_wakeups = 0,
    .last_idle_duration_us = 0,
};

/* 默认 C-state 信息（x86-64 典型值） */
static pm_cstate_info_t g_default_cstates[] = {
    { PM_CSTATE_C0, "C0 (Active)",    0,   50000, 0 },
    { PM_CSTATE_C1, "C1 (Halt)",      1,     500, PM_CSTATE_FLAG_HALT_ONLY },
    { PM_CSTATE_C2, "C2 (Stop)",     10,     100, PM_CSTATE_FLAG_CACHE_FLUSH },
    { PM_CSTATE_C3, "C3 (Sleep)",    60,      50, PM_CSTATE_FLAG_CACHE_FLUSH | PM_CSTATE_FLAG_TLB_FLUSH },
    { PM_CSTATE_C4, "C4 (Deep)",    100,      30, PM_CSTATE_FLAG_CONTEXT_SAVE },
    { PM_CSTATE_C6, "C6 (Deepest)", 300,      10, PM_CSTATE_FLAG_CONTEXT_SAVE },
};

/* 默认 S-state 信息 */
static pm_sstate_info_t g_default_sstates[] = {
    { PM_SSTATE_S0, "S0 (Working)",      0,      50000, true  },
    { PM_SSTATE_S1, "S1 (Standby)",      0,      5000,  false },
    { PM_SSTATE_S2, "S2 (Sleep)",        0,      1000,  false },
    { PM_SSTATE_S3, "S3 (Suspend RAM)",  500,    100,   true  },
    { PM_SSTATE_S4, "S4 (Hibernate)",    5000,   10,    true  },
    { PM_SSTATE_S5, "S5 (Soft Off)",     0,      1,     true  },
};

/* ========== 初始化 ========== */

/**
 * 初始化电源管理系统
 */
void pm_init(void)
{
    console_puts("[PM] Initializing power management\n");
    
    /* 初始化全局状态 */
    memzero(&g_pm_state, sizeof(pm_state_t));
    
    /* 设置默认 C-state 信息 */
    g_pm_state.current_cstate = PM_CSTATE_C0;
    g_pm_state.max_cstate = PM_CSTATE_C6;
    g_pm_state.cstate_count = sizeof(g_default_cstates) / sizeof(g_default_cstates[0]);
    g_pm_state.cstates = g_default_cstates;
    
    /* 设置默认 S-state 信息 */
    for (int i = 0; i < PM_SSTATE_MAX; i++) {
        g_pm_state.sstates[i] = g_default_sstates[i];
    }
    g_pm_state.current_sstate = PM_SSTATE_S0;
    
    /* 初始化空闲预测 */
    g_pm_state.idle_predict.mode = PM_IDLE_PREDICT_SIMPLE;
    g_pm_state.idle_predict.history_depth = 8;
    g_pm_state.idle_predict.accuracy_threshold = 80;
    g_pm_state.idle_predict.max_cstate = PM_CSTATE_C3;
    
    console_puts("[PM] Power management initialized\n");
    console_puts("[PM]   Max C-state: C");
    console_putu32(g_pm_state.max_cstate);
    console_puts("\n");
}

/**
 * 关闭电源管理系统
 */
void pm_shutdown(void)
{
    console_puts("[PM] Shutting down power management\n");
    
    /* 释放功耗域资源 */
    if (g_pm_state.domains) {
        /* 由系统释放 */
        g_pm_state.domains = NULL;
    }
}

/**
 * 注册架构操作回调
 */
void pm_register_arch_ops(const pm_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_pm_arch_ops, ops, sizeof(pm_arch_ops_t));
        console_puts("[PM] Architecture ops registered\n");
    }
}

/* ========== CPU 电源状态管理 ========== */

/**
 * 获取当前 C-state
 */
pm_cstate_t pm_get_cstate(void)
{
    return g_pm_state.current_cstate;
}

/**
 * 设置 C-state
 */
hic_status_t pm_set_cstate(pm_cstate_t state)
{
    if (state > g_pm_state.max_cstate) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (g_pm_arch_ops.enter_cstate) {
        g_pm_arch_ops.enter_cstate(state);
    }
    
    g_pm_state.current_cstate = state;
    return HIC_SUCCESS;
}

/**
 * 获取 C-state 信息
 */
pm_cstate_info_t* pm_get_cstate_info(pm_cstate_t state)
{
    if (state >= g_pm_state.cstate_count) {
        return NULL;
    }
    
    return &g_pm_state.cstates[state];
}

/**
 * 获取支持的 C-state 数量
 */
u32 pm_get_supported_cstates(void)
{
    return g_pm_state.cstate_count;
}

/* ========== 设备电源状态管理 ========== */

/**
 * 获取设备电源状态
 */
pm_dstate_t pm_get_device_state(u64 device_id)
{
    /* 默认返回 D0 */
    (void)device_id;
    return PM_DSTATE_D0;
}

/**
 * 设置设备电源状态
 */
hic_status_t pm_set_device_state(u64 device_id, pm_dstate_t state)
{
    (void)device_id;
    (void)state;
    /* TODO: 实现设备电源状态管理 */
    return HIC_SUCCESS;
}

/**
 * 挂起设备
 */
hic_status_t pm_device_suspend(u64 device_id)
{
    return pm_set_device_state(device_id, PM_DSTATE_D3HOT);
}

/**
 * 恢复设备
 */
hic_status_t pm_device_resume(u64 device_id)
{
    return pm_set_device_state(device_id, PM_DSTATE_D0);
}

/**
 * 检查设备是否支持唤醒
 */
bool pm_device_can_wakeup(u64 device_id)
{
    (void)device_id;
    return false;
}

/**
 * 启用/禁用设备唤醒
 */
hic_status_t pm_device_enable_wakeup(u64 device_id, bool enable)
{
    (void)device_id;
    (void)enable;
    return HIC_SUCCESS;
}

/* ========== 系统电源状态管理 ========== */

/**
 * 获取当前 S-state
 */
pm_sstate_t pm_get_sstate(void)
{
    return g_pm_state.current_sstate;
}

/**
 * 设置 S-state
 */
hic_status_t pm_set_sstate(pm_sstate_t state)
{
    if (state >= PM_SSTATE_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (!g_pm_state.sstates[state].supported) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    console_puts("[PM] Entering S-state: ");
    console_puts(g_pm_state.sstates[state].name);
    console_puts("\n");
    
    if (g_pm_arch_ops.system_suspend) {
        g_pm_arch_ops.system_suspend(state);
    }
    
    g_pm_state.current_sstate = state;
    return HIC_SUCCESS;
}

/**
 * 挂起系统到内存
 */
hic_status_t pm_suspend(void)
{
    return pm_set_sstate(PM_SSTATE_S3);
}

/**
 * 挂起系统到磁盘
 */
hic_status_t pm_hibernate(void)
{
    return pm_set_sstate(PM_SSTATE_S4);
}

/**
 * 软关机
 */
hic_status_t pm_shutdown_system(void)
{
    console_puts("[PM] System shutdown requested\n");
    return pm_set_sstate(PM_SSTATE_S5);
}

/**
 * 重启系统
 */
hic_status_t pm_reboot(void)
{
    console_puts("[PM] System reboot requested\n");
    
    if (g_pm_arch_ops.system_suspend) {
        g_pm_arch_ops.system_suspend(PM_SSTATE_S5);
    }
    
    /* 架构相关的重启逻辑 */
    hal_reboot();
    
    return HIC_SUCCESS;
}

/**
 * 检查 S-state 是否支持
 */
bool pm_sstate_supported(pm_sstate_t state)
{
    if (state >= PM_SSTATE_MAX) {
        return false;
    }
    return g_pm_state.sstates[state].supported;
}

/* ========== DVFS 管理 ========== */

/**
 * 获取当前频率
 */
u32 pm_get_frequency(u32 domain_id)
{
    (void)domain_id;
    
    if (g_pm_arch_ops.get_frequency) {
        return g_pm_arch_ops.get_frequency(domain_id);
    }
    
    return 0;
}

/**
 * 设置频率
 */
hic_status_t pm_set_frequency(u32 domain_id, u32 freq_khz)
{
    (void)domain_id;
    (void)freq_khz;
    
    /* TODO: 实现频率设置，需要查找对应的电压 */
    
    return HIC_SUCCESS;
}

/**
 * 获取 DVFS 模式
 */
pm_dvfs_mode_t pm_get_dvfs_mode(u32 domain_id)
{
    (void)domain_id;
    return PM_DVFS_MODE_BALANCED;
}

/**
 * 设置 DVFS 模式
 */
hic_status_t pm_set_dvfs_mode(u32 domain_id, pm_dvfs_mode_t mode)
{
    (void)domain_id;
    (void)mode;
    return HIC_SUCCESS;
}

/**
 * 获取支持的频率数量
 */
u32 pm_get_frequency_count(u32 domain_id)
{
    (void)domain_id;
    return 0;
}

/**
 * 获取频率级别列表
 */
pm_freq_level_t* pm_get_frequency_levels(u32 domain_id, u32 *count)
{
    (void)domain_id;
    if (count) {
        *count = 0;
    }
    return NULL;
}

/* ========== 功耗域管理 ========== */

/**
 * 获取功耗域
 */
pm_domain_t* pm_get_domain(u32 domain_id)
{
    (void)domain_id;
    return NULL;
}

/**
 * 开启功耗域
 */
hic_status_t pm_domain_power_on(u32 domain_id)
{
    if (g_pm_arch_ops.domain_power_on) {
        g_pm_arch_ops.domain_power_on(domain_id);
    }
    return HIC_SUCCESS;
}

/**
 * 关闭功耗域
 */
hic_status_t pm_domain_power_off(u32 domain_id)
{
    if (g_pm_arch_ops.domain_power_off) {
        g_pm_arch_ops.domain_power_off(domain_id);
    }
    return HIC_SUCCESS;
}

/**
 * 检查功耗域是否开启
 */
bool pm_domain_is_on(u32 domain_id)
{
    (void)domain_id;
    return true;
}

/**
 * 获取功耗域数量
 */
u32 pm_get_domain_count(void)
{
    return g_pm_state.domain_count;
}

/* ========== 空闲预测 ========== */

/* 空闲历史记录 */
#define PM_IDLE_HISTORY_SIZE 16
static u32 g_idle_history[PM_IDLE_HISTORY_SIZE];
static u32 g_idle_history_index = 0;
static u32 g_idle_history_count = 0;

/**
 * 初始化空闲预测
 */
void pm_idle_predict_init(pm_idle_predict_mode_t mode)
{
    g_pm_state.idle_predict.mode = mode;
    g_idle_history_index = 0;
    g_idle_history_count = 0;
    memzero(g_idle_history, sizeof(g_idle_history));
}

/**
 * 预测下一个 C-state
 */
u32 pm_idle_predict_next_cstate(void)
{
    if (g_pm_state.idle_predict.mode == PM_IDLE_PREDICT_DISABLED) {
        return PM_CSTATE_C1;
    }
    
    /* 简单预测：基于历史平均值 */
    if (g_idle_history_count == 0) {
        return PM_CSTATE_C1;
    }
    
    u64 total = 0;
    u32 count = g_idle_history_count < PM_IDLE_HISTORY_SIZE ? 
                g_idle_history_count : PM_IDLE_HISTORY_SIZE;
    
    for (u32 i = 0; i < count; i++) {
        total += g_idle_history[i];
    }
    
    u32 avg_duration = (u32)(total / count);
    
    /* 根据预测的空闲时间选择 C-state */
    if (avg_duration < 10) {
        return PM_CSTATE_C1;
    } else if (avg_duration < 100) {
        return PM_CSTATE_C2;
    } else if (avg_duration < 500) {
        return PM_CSTATE_C3;
    } else {
        return PM_CSTATE_C4;
    }
}

/**
 * 更新空闲预测
 */
void pm_idle_predict_update(u32 actual_duration_us)
{
    g_idle_history[g_idle_history_index] = actual_duration_us;
    g_idle_history_index = (g_idle_history_index + 1) % PM_IDLE_HISTORY_SIZE;
    g_idle_history_count++;
    
    g_pm_state.last_idle_duration_us = actual_duration_us;
    g_pm_state.total_idle_time_us += actual_duration_us;
}

/* ========== 空闲处理 ========== */

/**
 * 进入空闲状态
 */
void pm_idle(void)
{
    u64 start_time = hal_get_timestamp();
    
    /* 预测最佳 C-state */
    pm_cstate_t target_state = (pm_cstate_t)pm_idle_predict_next_cstate();
    
    /* 限制最大 C-state */
    if (target_state > g_pm_state.idle_predict.max_cstate) {
        target_state = g_pm_state.idle_predict.max_cstate;
    }
    
    /* 进入 C-state */
    if (g_pm_arch_ops.enter_cstate) {
        g_pm_arch_ops.enter_cstate(target_state);
    }
    
    g_pm_state.current_cstate = target_state;
    g_pm_state.total_wakeups++;
    
    /* 计算空闲时间 */
    u64 end_time = hal_get_timestamp();
    u32 duration_us = (u32)((end_time - start_time) / 1000);
    
    /* 更新预测 */
    pm_idle_predict_update(duration_us);
    
    /* 退出 C-state */
    if (g_pm_arch_ops.exit_cstate) {
        g_pm_arch_ops.exit_cstate();
    }
    
    g_pm_state.current_cstate = PM_CSTATE_C0;
}

/**
 * 进入深度空闲状态
 */
void pm_deep_idle(void)
{
    g_pm_state.idle_predict.max_cstate = PM_CSTATE_C6;
    pm_idle();
    g_pm_state.idle_predict.max_cstate = PM_CSTATE_C3;
}

/* ========== 统计信息 ========== */

/**
 * 获取电源管理统计
 */
void pm_get_stats(pm_state_t *state)
{
    if (state) {
        memcopy(state, &g_pm_state, sizeof(pm_state_t));
    }
}

/**
 * 打印电源管理统计
 */
void pm_print_stats(void)
{
    console_puts("\n[PM] ===== Power Management Statistics =====\n");
    
    console_puts("[PM] Current C-state: C");
    console_putu32(g_pm_state.current_cstate);
    console_puts("\n");
    
    console_puts("[PM] Current S-state: S");
    console_putu32(g_pm_state.current_sstate);
    console_puts("\n");
    
    console_puts("[PM] Total idle time: ");
    console_putu64(g_pm_state.total_idle_time_us / 1000);
    console_puts(" ms\n");
    
    console_puts("[PM] Total wakeups: ");
    console_putu64(g_pm_state.total_wakeups);
    console_puts("\n");
    
    console_puts("[PM] Last idle duration: ");
    console_putu32(g_pm_state.last_idle_duration_us);
    console_puts(" us\n");
    
    console_puts("[PM] ========================================\n\n");
}
