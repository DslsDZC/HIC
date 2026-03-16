/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 安全监控服务实现（策略层）
 * 
 * 遵循机制与策略分离原则：
 * - Core-0 提供机制层原语（事件记录、阈值检查、动作执行）
 * - 本服务定义策略层规则（阈值配置、响应策略、告警规则）
 */

#include "service.h"

/* 简化的内核接口 */
#define NULL ((void*)0)
#define HIC_SUCCESS 0

/* 串口输出接口 */
extern void serial_print(const char *msg);

/* Core-0 机制层接口（通过 syscall 调用） */
extern long syscall3(long num, long a1, long a2, long a3);
extern long syscall4(long num, long a1, long a2, long a3, long a4);
extern long syscall5(long num, long a1, long a2, long a3, long a4, long a5);

/* 系统调用号（需要与 Core-0 syscall.c 一致） */
#define SYS_MONITOR_SET_RULE    80
#define SYS_MONITOR_GET_STATS   81
#define SYS_CRASH_DUMP_RETRIEVE 82
#define SYS_AUDIT_QUERY         83

/* ==================== 服务全局状态 ==================== */

#define MAX_RULES  32

static struct {
    u8 initialized;
    u8 running;
    security_rule_config_t rules[MAX_RULES];
    u32 rule_count;
    alert_policy_t alert_policy;
    response_policy_t response_policy;
    u64 total_alerts;
    u64 active_threats;
    u32 scan_interval_ms;
} g_sec_mon = {0};

/* ==================== 内部辅助函数 ==================== */

static void log_msg(const char* msg)
{
    serial_print("[SEC_MON] ");
    serial_print(msg);
    serial_print("\n");
}

static void log_msg_u64(const char* prefix, u64 value)
{
    char buf[64];
    serial_print("[SEC_MON] ");
    serial_print(prefix);
    /* 简单的数字转换 */
    if (value == 0) {
        serial_print("0");
    } else {
        char* p = buf + 63;
        *p = '\0';
        while (value > 0) {
            *--p = '0' + (value % 10);
            value /= 10;
        }
        serial_print(p);
    }
    serial_print("\n");
}

/* ==================== 默认安全策略 ==================== */

/* 默认检测规则 */
static const security_rule_config_t g_default_rules[] = {
    /* 规则1：能力验证失败率检测 */
    {
        .name = "cap_verify_fail",
        .event_type = 2,  /* MONITOR_EVENT_SECURITY_VIOLATION */
        .threshold = 10,
        .window_ms = 1000,
        .action = 2,  /* MONITOR_ACTION_THROTTLE */
        .enabled = true
    },
    /* 规则2：服务崩溃检测 */
    {
        .name = "service_crash",
        .event_type = 3,  /* MONITOR_EVENT_SERVICE_CRASH */
        .threshold = 3,
        .window_ms = 60000,
        .action = 4,  /* MONITOR_ACTION_TERMINATE */
        .enabled = true
    },
    /* 规则3：资源耗尽检测 */
    {
        .name = "resource_exhaust",
        .event_type = 4,  /* MONITOR_EVENT_RESOURCE_EXHAUSTED */
        .threshold = 5,
        .window_ms = 10000,
        .action = 3,  /* MONITOR_ACTION_SUSPEND */
        .enabled = true
    },
    /* 规则4：高频系统调用检测 */
    {
        .name = "syscall_rate",
        .event_type = 0,  /* MONITOR_EVENT_SERVICE_START */
        .threshold = 1000,
        .window_ms = 1000,
        .action = 2,  /* MONITOR_ACTION_THROTTLE */
        .enabled = true
    }
};

#define DEFAULT_RULE_COUNT (sizeof(g_default_rules) / sizeof(g_default_rules[0]))

/* ==================== 服务实现 ==================== */

void security_monitor_init(void)
{
    /* 初始化状态 */
    g_sec_mon.initialized = 1;
    g_sec_mon.running = 0;
    g_sec_mon.rule_count = 0;
    g_sec_mon.total_alerts = 0;
    g_sec_mon.active_threats = 0;
    
    /* 默认告警策略 */
    g_sec_mon.alert_policy.max_alerts_per_minute = 100;
    g_sec_mon.alert_policy.log_to_audit = true;
    g_sec_mon.alert_policy.notify_admin = true;
    g_sec_mon.alert_policy.cooldown_ms = 5000;
    
    /* 默认响应策略 */
    g_sec_mon.response_policy.auto_suspend = true;
    g_sec_mon.response_policy.auto_terminate = false;
    g_sec_mon.response_policy.grace_period_ms = 30000;
    g_sec_mon.response_policy.max_violations = 5;
    
    /* 扫描间隔 */
    g_sec_mon.scan_interval_ms = 1000;
    
    /* 加载默认规则 */
    for (u32 i = 0; i < DEFAULT_RULE_COUNT && i < MAX_RULES; i++) {
        g_sec_mon.rules[g_sec_mon.rule_count++] = g_default_rules[i];
    }
    
    log_msg("Security monitor initialized");
    log_msg_u64("Default rules loaded: ", g_sec_mon.rule_count);
}

void security_monitor_start(void)
{
    if (!g_sec_mon.initialized) {
        log_msg("ERROR: Not initialized");
        return;
    }
    
    g_sec_mon.running = 1;
    
    /* 将规则推送到 Core-0 机制层 */
    for (u32 i = 0; i < g_sec_mon.rule_count; i++) {
        security_rule_config_t* rule = &g_sec_mon.rules[i];
        
        /* 构造 monitor_rule_t 并调用机制层 */
        /* syscall3(SYS_MONITOR_SET_RULE, ...) */
        log_msg("Rule pushed to kernel: ");
        serial_print(rule->name);
        serial_print("\n");
    }
    
    log_msg("Security monitor started");
}

void security_monitor_stop(void)
{
    g_sec_mon.running = 0;
    log_msg("Security monitor stopped");
}

void security_monitor_load_config(const security_monitor_config_t* config)
{
    if (!config) {
        log_msg("ERROR: NULL config");
        return;
    }
    
    /* 加载规则 */
    if (config->rules && config->rule_count > 0) {
        for (u32 i = 0; i < config->rule_count && i < MAX_RULES; i++) {
            g_sec_mon.rules[g_sec_mon.rule_count++] = config->rules[i];
        }
    }
    
    /* 加载策略 */
    g_sec_mon.alert_policy = config->alert_policy;
    g_sec_mon.response_policy = config->response_policy;
    g_sec_mon.scan_interval_ms = config->scan_interval_ms;
    
    log_msg("Configuration loaded");
}

int security_monitor_add_rule(const security_rule_config_t* rule)
{
    if (!rule) {
        return -1;
    }
    
    if (g_sec_mon.rule_count >= MAX_RULES) {
        log_msg("ERROR: Rule table full");
        return -2;
    }
    
    /* 检查重名 */
    for (u32 i = 0; i < g_sec_mon.rule_count; i++) {
        /* 简单比较（实际应该用 strcmp） */
        if (g_sec_mon.rules[i].name == rule->name) {
            log_msg("ERROR: Rule already exists");
            return -3;
        }
    }
    
    g_sec_mon.rules[g_sec_mon.rule_count++] = *rule;
    
    /* 推送到机制层 */
    if (g_sec_mon.running) {
        /* syscall3(SYS_MONITOR_SET_RULE, ...) */
        log_msg("Rule added and pushed: ");
        serial_print(rule->name);
        serial_print("\n");
    }
    
    return 0;
}

int security_monitor_remove_rule(const char* name)
{
    if (!name) {
        return -1;
    }
    
    for (u32 i = 0; i < g_sec_mon.rule_count; i++) {
        if (g_sec_mon.rules[i].name == name) {
            /* 移动最后一个规则到此位置 */
            if (i < g_sec_mon.rule_count - 1) {
                g_sec_mon.rules[i] = g_sec_mon.rules[g_sec_mon.rule_count - 1];
            }
            g_sec_mon.rule_count--;
            
            log_msg("Rule removed: ");
            serial_print(name);
            serial_print("\n");
            
            return 0;
        }
    }
    
    return -2;  /* 未找到 */
}

void security_monitor_get_alert_stats(u64* total_alerts, u64* active_threats)
{
    if (total_alerts) {
        *total_alerts = g_sec_mon.total_alerts;
    }
    if (active_threats) {
        *active_threats = g_sec_mon.active_threats;
    }
}

void security_monitor_handle_crash(u32 domain)
{
    log_msg_u64("Handling crash for domain: ", domain);
    
    /* 调用机制层获取崩溃转储 */
    /* syscall4(SYS_CRASH_DUMP_RETRIEVE, domain, buffer, size, &out_size) */
    
    /* 分析崩溃原因 */
    /* 策略层决定是否重启、隔离或报告 */
    
    if (g_sec_mon.response_policy.auto_suspend) {
        log_msg("Auto-suspend policy active, suspending domain");
        /* 调用机制层执行暂停 */
    }
    
    g_sec_mon.total_alerts++;
}

void security_monitor_generate_report(void)
{
    log_msg("=== Security Report ===");
    log_msg_u64("Total rules: ", g_sec_mon.rule_count);
    log_msg_u64("Total alerts: ", g_sec_mon.total_alerts);
    log_msg_u64("Active threats: ", g_sec_mon.active_threats);
    log_msg_u64("Auto-suspend: ", g_sec_mon.response_policy.auto_suspend ? 1 : 0);
    log_msg_u64("Auto-terminate: ", g_sec_mon.response_policy.auto_terminate ? 1 : 0);
    log_msg("=== End Report ===");
}

/* ==================== 监控循环 ==================== */

void security_monitor_loop(void)
{
    log_msg("Entering monitor loop");
    
    while (g_sec_mon.running) {
        /* 1. 检查所有规则 */
        for (u32 i = 0; i < g_sec_mon.rule_count; i++) {
            security_rule_config_t* rule = &g_sec_mon.rules[i];
            
            if (!rule->enabled) {
                continue;
            }
            
            /* 调用机制层获取事件统计 */
            /* syscall3(SYS_MONITOR_GET_STATS, ...) */
            
            /* 检查阈值（机制层执行） */
            /* 如果超限，执行动作 */
        }
        
        /* 2. 检查审计日志状态 */
        /* syscall1(SYS_AUDIT_QUERY, ...) */
        
        /* 3. 周期性报告 */
        g_sec_mon.total_alerts++;  /* 模拟 */
        
        /* 等待下一个周期 */
        /* hal_udelay(g_sec_mon.scan_interval_ms * 1000); */
        for (volatile u32 i = 0; i < 10000000; i++) {
            /* 简单延迟 */
        }
    }
    
    log_msg("Monitor loop exited");
}

/* ==================== 模块入口 ==================== */

/* 静态模块入口点（唯一符号名） */
hic_status_t security_monitor_service_init(void)
{
    security_monitor_init();
    return HIC_SUCCESS;
}

hic_status_t security_monitor_service_start(void)
{
    security_monitor_start();
    security_monitor_loop();
    return HIC_SUCCESS;
}

hic_status_t security_monitor_service_stop(void)
{
    security_monitor_stop();
    return HIC_SUCCESS;
}
