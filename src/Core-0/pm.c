/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理机制层实现
 * 
 * Core-0 提供电源状态转换的原语实现
 * 策略决策由 Privileged-1 的 pm_service 负责
 */

#include "pm.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"

/* 架构操作回调 */
static pm_arch_ops_t g_pm_arch_ops = {0};

/* C-state 信息表 */
static pm_cstate_info_t g_cstates[] = {
    { PM_CSTATE_C0, "C0 (Active)",    0,   50000, 0 },
    { PM_CSTATE_C1, "C1 (Halt)",      1,     500, PM_CSTATE_FLAG_HALT_ONLY },
    { PM_CSTATE_C2, "C2 (Stop)",     10,     100, PM_CSTATE_FLAG_CACHE_FLUSH },
    { PM_CSTATE_C3, "C3 (Sleep)",    60,      50, PM_CSTATE_FLAG_CACHE_FLUSH | PM_CSTATE_FLAG_TLB_FLUSH },
    { PM_CSTATE_C4, "C4 (Deep)",    100,      30, PM_CSTATE_FLAG_CONTEXT_SAVE },
    { PM_CSTATE_C6, "C6 (Deepest)", 300,      10, PM_CSTATE_FLAG_CONTEXT_SAVE },
};
#define CSTATE_COUNT (sizeof(g_cstates) / sizeof(g_cstates[0]))

/* S-state 信息表 */
static pm_sstate_info_t g_sstates[] = {
    { PM_SSTATE_S0, "S0 (Working)",      0,      50000, true  },
    { PM_SSTATE_S1, "S1 (Standby)",      0,      5000,  false },
    { PM_SSTATE_S3, "S3 (Suspend RAM)",  500,    100,   true  },
    { PM_SSTATE_S4, "S4 (Hibernate)",    5000,   10,    true  },
    { PM_SSTATE_S5, "S5 (Soft Off)",     0,      1,     true  },
};
#define SSTATE_COUNT (sizeof(g_sstates) / sizeof(g_sstates[0]))

/* 全局状态 */
static struct {
    pm_cstate_t current_cstate;
    pm_cstate_t max_cstate;
    pm_sstate_t current_sstate;
    u64 total_idle_time_us;
    u64 total_wakeups;
    u32 last_idle_duration_us;
    u32 domain_count;
} g_pm = {
    .current_cstate = PM_CSTATE_C0,
    .max_cstate = PM_CSTATE_C6,
    .current_sstate = PM_SSTATE_S0,
};

/* ========== 初始化 ========== */

void pm_init(void)
{
    console_puts("[PM] Power management mechanism layer initialized\n");
    console_puts("[PM]   Max C-state: C");
    console_putu32(g_pm.max_cstate);
    console_puts("\n");
}

void pm_shutdown(void)
{
    console_puts("[PM] Power management shutdown\n");
}

void pm_register_arch_ops(const pm_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_pm_arch_ops, ops, sizeof(pm_arch_ops_t));
        console_puts("[PM] Architecture ops registered\n");
    }
}

/* ========== C-state 操作 ========== */

pm_cstate_t pm_get_cstate(void)
{
    return g_pm.current_cstate;
}

hic_status_t pm_set_cstate(pm_cstate_t state)
{
    if (state >= PM_CSTATE_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (state > g_pm.max_cstate) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_pm.current_cstate = state;
    return HIC_SUCCESS;
}

pm_cstate_info_t* pm_get_cstate_info(pm_cstate_t state)
{
    if (state >= CSTATE_COUNT) {
        return NULL;
    }
    return &g_cstates[state];
}

u32 pm_get_supported_cstates(void)
{
    return CSTATE_COUNT;
}

/* ========== S-state 操作 ========== */

pm_sstate_t pm_get_sstate(void)
{
    return g_pm.current_sstate;
}

hic_status_t pm_set_sstate(pm_sstate_t state)
{
    if (state >= SSTATE_COUNT) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (!g_sstates[state].supported) {
        return HIC_ERROR_NOT_SUPPORTED;
    }
    
    console_puts("[PM] Entering S-state: ");
    console_puts(g_sstates[state].name);
    console_puts("\n");
    
    if (g_pm_arch_ops.enter_sstate) {
        g_pm_arch_ops.enter_sstate(state);
    }
    
    g_pm.current_sstate = state;
    return HIC_SUCCESS;
}

bool pm_sstate_supported(pm_sstate_t state)
{
    if (state >= SSTATE_COUNT) {
        return false;
    }
    return g_sstates[state].supported;
}

hic_status_t pm_reboot(void)
{
    console_puts("[PM] System reboot\n");
    
    if (g_pm_arch_ops.reboot) {
        g_pm_arch_ops.reboot();
    }
    
    /* 如果架构未实现，死循环 */
    while (1) { }
    
    return HIC_SUCCESS;
}

hic_status_t pm_shutdown_system(void)
{
    console_puts("[PM] System shutdown\n");
    
    if (g_pm_arch_ops.shutdown) {
        g_pm_arch_ops.shutdown();
    }
    
    return pm_set_sstate(PM_SSTATE_S5);
}

/* ========== 频率操作 ========== */

u32 pm_get_frequency(u32 domain_id)
{
    (void)domain_id;
    
    if (g_pm_arch_ops.get_frequency) {
        return g_pm_arch_ops.get_frequency(domain_id);
    }
    
    return 0;
}

hic_status_t pm_set_frequency(u32 domain_id, u32 freq_khz)
{
    if (g_pm_arch_ops.set_frequency) {
        return g_pm_arch_ops.set_frequency(domain_id, freq_khz);
    }
    
    return HIC_SUCCESS;
}

/* ========== 功耗域操作 ========== */

hic_status_t pm_domain_power_on(u32 domain_id)
{
    if (g_pm_arch_ops.domain_power_on) {
        return g_pm_arch_ops.domain_power_on(domain_id);
    }
    return HIC_SUCCESS;
}

hic_status_t pm_domain_power_off(u32 domain_id)
{
    if (g_pm_arch_ops.domain_power_off) {
        return g_pm_arch_ops.domain_power_off(domain_id);
    }
    return HIC_SUCCESS;
}

bool pm_domain_is_on(u32 domain_id)
{
    (void)domain_id;
    return true;
}

u32 pm_get_domain_count(void)
{
    return g_pm.domain_count;
}

/* ========== 统计查询 ========== */

void pm_get_stats(pm_stats_t *stats)
{
    if (stats) {
        stats->current_cstate = g_pm.current_cstate;
        stats->max_cstate = g_pm.max_cstate;
        stats->current_sstate = g_pm.current_sstate;
        stats->total_idle_time_us = g_pm.total_idle_time_us;
        stats->total_wakeups = g_pm.total_wakeups;
        stats->last_idle_duration_us = g_pm.last_idle_duration_us;
        stats->domain_count = g_pm.domain_count;
    }
}

/* ========== 空闲入口（由策略层调用） ========== */

void pm_enter_cstate(pm_cstate_t state)
{
    if (state >= PM_CSTATE_MAX || state > g_pm.max_cstate) {
        state = PM_CSTATE_C1;  /* 安全回退 */
    }
    
    if (g_pm_arch_ops.enter_cstate) {
        g_pm_arch_ops.enter_cstate(state);
    }
    
    g_pm.current_cstate = state;
}

void pm_exit_cstate(void)
{
    if (g_pm_arch_ops.exit_cstate) {
        g_pm_arch_ops.exit_cstate();
    }
    
    g_pm.current_cstate = PM_CSTATE_C0;
    g_pm.total_wakeups++;
}