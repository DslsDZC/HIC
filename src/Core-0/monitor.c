/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC监控服务 - 机制层实现
 * 
 * 本文件仅包含机制层原语实现，不包含策略决策。
 * 策略层实现已移动到 Privileged-1/monitor_service/
 * 
 * 设计原则：
 * - 机制层只提供原语，不包含业务逻辑
 * - 所有决策由策略层（Privileged-1）做出
 * - 机制层可以被形式化验证
 */

#include "monitor.h"
#include "hal.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 最大域数量 */
#define MAX_DOMAINS  64

/* ==================== 机制层数据结构 ==================== */

/* 事件统计表（每域每种事件） */
static event_stat_t g_event_stats[MAX_DOMAINS][MONITOR_EVENT_TYPE_COUNT];

/* 检测规则表（由策略层设置） */
static monitor_rule_t g_monitor_rules[MONITOR_EVENT_TYPE_COUNT];

/* 崩溃转储存储 */
static crash_dump_header_t g_crash_dumps[MAX_DOMAINS];
static u8 g_crash_dump_data[MAX_DOMAINS][CRASH_DUMP_MAX_SIZE];

/* ==================== 机制层：初始化 ==================== */

/**
 * 初始化监控机制层
 */
void monitor_mechanism_init(void)
{
    /* 清零事件统计表 */
    memzero(g_event_stats, sizeof(g_event_stats));
    
    /* 清零规则表 */
    memzero(g_monitor_rules, sizeof(g_monitor_rules));
    
    /* 清零崩溃转储 */
    memzero(g_crash_dumps, sizeof(g_crash_dumps));
    memzero(g_crash_dump_data, sizeof(g_crash_dump_data));
    
    console_puts("[MONITOR] Mechanism layer initialized\n");
}

/* ==================== 机制层：事件计数原语实现 ==================== */

/**
 * 记录事件计数（机制层）
 */
void monitor_record_event(monitor_event_type_t event_type, domain_id_t domain)
{
    if (domain >= MAX_DOMAINS || event_type >= MONITOR_EVENT_TYPE_COUNT) {
        return;
    }
    
    bool irq = atomic_enter_critical();
    
    u64 now = hal_get_timestamp();
    event_stat_t* stat = &g_event_stats[domain][event_type];
    
    /* 检查是否需要重置窗口 */
    u64 window_us = (u64)MONITOR_STATS_WINDOW_MS * 1000;
    if (now - stat->window_start > window_us) {
        stat->count = 0;
        stat->window_start = now;
    }
    
    stat->count++;
    
    atomic_exit_critical(irq);
}

/**
 * 获取事件速率（机制层）
 */
u32 monitor_get_event_rate(monitor_event_type_t event_type, domain_id_t domain)
{
    if (domain >= MAX_DOMAINS || event_type >= MONITOR_EVENT_TYPE_COUNT) {
        return 0;
    }
    
    bool irq = atomic_enter_critical();
    
    event_stat_t* stat = &g_event_stats[domain][event_type];
    u64 now = hal_get_timestamp();
    u64 window_us = (u64)MONITOR_STATS_WINDOW_MS * 1000;
    
    /* 如果窗口过期，返回0 */
    if (now - stat->window_start > window_us) {
        atomic_exit_critical(irq);
        return 0;
    }
    
    /* 计算速率（每秒次数） */
    u64 elapsed_ms = (now - stat->window_start) / 1000;
    if (elapsed_ms == 0) elapsed_ms = 1;
    
    u32 rate = (u32)((stat->count * 1000) / elapsed_ms);
    
    atomic_exit_critical(irq);
    
    return rate;
}

/**
 * 获取事件计数（机制层）
 */
u64 monitor_get_event_count(monitor_event_type_t event_type, domain_id_t domain)
{
    if (domain >= MAX_DOMAINS || event_type >= MONITOR_EVENT_TYPE_COUNT) {
        return 0;
    }
    
    bool irq = atomic_enter_critical();
    u64 count = g_event_stats[domain][event_type].count;
    atomic_exit_critical(irq);
    
    return count;
}

/* ==================== 机制层：阈值检查原语实现 ==================== */

/**
 * 检查是否超过阈值（机制层）
 */
bool monitor_check_threshold(const monitor_rule_t* rule, domain_id_t domain)
{
    if (!rule || !rule->enabled || domain >= MAX_DOMAINS) {
        return false;
    }
    
    u32 rate = monitor_get_event_rate(rule->event_type, domain);
    return rate > rule->threshold;
}

/* ==================== 机制层：动作执行原语实现 ==================== */

/**
 * 执行阈值动作（机制层）
 */
hic_status_t monitor_execute_action(monitor_action_t action, domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 导入域管理函数 */
    extern hic_status_t domain_suspend(domain_id_t domain_id);
    
    switch (action) {
        case MONITOR_ACTION_ALERT:
            /* 告警：仅记录到审计日志 */
            console_puts("[MONITOR] ALERT for domain ");
            console_putu64(domain);
            console_puts("\n");
            /* 审计日志写入由策略层决定是否调用 */
            break;
            
        case MONITOR_ACTION_THROTTLE:
            /* 限流：设置标记（策略层读取并执行具体限流） */
            console_puts("[MONITOR] THROTTLE domain ");
            console_putu64(domain);
            console_puts("\n");
            break;
            
        case MONITOR_ACTION_SUSPEND:
            /* 暂停：调用域管理机制 */
            console_puts("[MONITOR] SUSPEND domain ");
            console_putu64(domain);
            console_puts("\n");
            return domain_suspend(domain);
            
        case MONITOR_ACTION_TERMINATE:
            /* 终止：调用域销毁机制 */
            console_puts("[MONITOR] TERMINATE domain ");
            console_putu64(domain);
            console_puts("\n");
            /* 域销毁由策略层协调 */
            break;
            
        default:
            break;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 机制层：崩溃转储原语实现 ==================== */

/* 简单校验和计算 */
static u64 compute_checksum(const void* data, size_t size)
{
    u64 sum = 0;
    const u64* ptr = (const u64*)data;
    size_t count = size / sizeof(u64);
    
    for (size_t i = 0; i < count; i++) {
        sum ^= ptr[i];
    }
    
    return sum;
}

/**
 * 捕获崩溃现场（机制层）
 */
hic_status_t crash_dump_capture(domain_id_t domain, u64 stack_ptr, u64 instr_ptr)
{
    if (domain >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    crash_dump_header_t* header = &g_crash_dumps[domain];
    
    /* 初始化头 */
    header->magic = CRASH_DUMP_MAGIC;
    header->timestamp = hal_get_timestamp();
    header->domain = domain;
    header->type = CRASH_DUMP_STACK;
    header->stack_ptr = stack_ptr;
    header->instr_ptr = instr_ptr;
    
    /* 捕获栈数据（最多 CRASH_DUMP_MAX_SIZE） */
    size_t capture_size = CRASH_DUMP_MAX_SIZE;
    memzero(g_crash_dump_data[domain], capture_size);
    
    /* 
     * 安全复制栈数据
     * 注意：实际实现需要检查内存边界
     * 这里使用占位数据，策略层应实现安全的内存访问
     */
    if (stack_ptr != 0) {
        /* TODO: 实现安全的内存复制，检查地址有效性 */
        /* 这里暂时填充标记值 */
        for (size_t i = 0; i < capture_size / sizeof(u64); i++) {
            ((u64*)g_crash_dump_data[domain])[i] = 0xDEADBEEFCAFEBABEULL;
        }
    }
    
    header->size = (u32)capture_size;
    header->checksum = compute_checksum(g_crash_dump_data[domain], capture_size);
    
    atomic_exit_critical(irq);
    
    console_puts("[MONITOR] Captured crash dump for domain ");
    console_putu64(domain);
    console_puts(", SP=0x");
    console_puthex64(stack_ptr);
    console_puts(", IP=0x");
    console_puthex64(instr_ptr);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/**
 * 检索崩溃转储（机制层）
 */
hic_status_t crash_dump_retrieve(domain_id_t domain, void* buffer, 
                                  size_t buffer_size, size_t* out_size)
{
    if (domain >= MAX_DOMAINS || !buffer) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    crash_dump_header_t* header = &g_crash_dumps[domain];
    
    /* 验证转储有效性 */
    if (header->magic != CRASH_DUMP_MAGIC) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    /* 计算输出大小 */
    size_t total_size = sizeof(crash_dump_header_t) + header->size;
    if (out_size) {
        *out_size = total_size;
    }
    
    /* 检查缓冲区大小 */
    if (buffer_size < total_size) {
        atomic_exit_critical(irq);
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    /* 复制头 */
    memcopy(buffer, header, sizeof(crash_dump_header_t));
    
    /* 复制数据 */
    memcopy((u8*)buffer + sizeof(crash_dump_header_t), 
            g_crash_dump_data[domain], header->size);
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 清除崩溃转储（机制层）
 */
void crash_dump_clear(domain_id_t domain)
{
    if (domain >= MAX_DOMAINS) {
        return;
    }
    
    bool irq = atomic_enter_critical();
    
    memzero(&g_crash_dumps[domain], sizeof(crash_dump_header_t));
    memzero(g_crash_dump_data[domain], CRASH_DUMP_MAX_SIZE);
    
    atomic_exit_critical(irq);
}

/* ==================== 机制层：统计原语实现 ==================== */

/**
 * 获取所有事件统计（机制层）
 */
void monitor_get_all_stats(event_stat_t* stats, u32 max_count, u32* out_count)
{
    if (!stats) {
        if (out_count) *out_count = 0;
        return;
    }
    
    bool irq = atomic_enter_critical();
    
    u32 count = 0;
    u32 total = MAX_DOMAINS * MONITOR_EVENT_TYPE_COUNT;
    
    if (max_count < total) {
        total = max_count;
    }
    
    for (u32 d = 0; d < MAX_DOMAINS && count < total; d++) {
        for (u32 e = 0; e < MONITOR_EVENT_TYPE_COUNT && count < total; e++) {
            stats[count++] = g_event_stats[d][e];
        }
    }
    
    atomic_exit_critical(irq);
    
    if (out_count) *out_count = count;
}

/* ==================== 供策略层调用的规则接口 ==================== */

/**
 * 设置检测规则（策略层调用）
 */
hic_status_t monitor_set_rule(const monitor_rule_t* rule)
{
    if (!rule || rule->event_type >= MONITOR_EVENT_TYPE_COUNT) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    g_monitor_rules[rule->event_type] = *rule;
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 获取检测规则（策略层调用）
 */
hic_status_t monitor_get_rule(monitor_event_type_t event_type, monitor_rule_t* rule)
{
    if (event_type >= MONITOR_EVENT_TYPE_COUNT || !rule) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    *rule = g_monitor_rules[event_type];
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}
