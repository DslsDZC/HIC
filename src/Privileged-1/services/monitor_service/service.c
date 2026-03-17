/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC监控服务 - 策略层实现
 * 
 * 本服务实现监控策略决策，使用 Core-0 机制层原语。
 */

#include "service.h"
#include "monitor.h"
#include "audit.h"
#include "domain.h"
#include "hal.h"
#include "lib/string.h"

/* 最大服务数量 */
#define MAX_MONITORED_SERVICES  64

/* 默认配置 */
#define DEFAULT_HEARTBEAT_TIMEOUT_MS  5000
#define DEFAULT_MAX_RESTART_ATTEMPTS  3
#define DEFAULT_RESTART_DELAY_MS      1000

/* ==================== 策略层状态 ==================== */

/* 服务注册表 */
static service_info_t g_services[MAX_MONITORED_SERVICES];
static u32 g_service_count = 0;

/* 恢复策略配置 */
typedef struct {
    u32 max_restart_attempts;
    u32 restart_delay_ms;
    bool escalate_on_failure;
} recovery_policy_t;

static recovery_policy_t g_recovery_policy[MAX_MONITORED_SERVICES];

/* 告警处理器 */
static alert_handler_t g_alert_handler = NULL;
static void* g_alert_context = NULL;

/* 监控运行状态 */
static bool g_monitor_running = false;

/* ==================== 策略层：初始化 ==================== */

/**
 * 初始化监控服务（策略层）
 */
hic_status_t monitor_service_init(void)
{
    /* 清零服务表 */
    memset(g_services, 0, sizeof(g_services));
    memset(g_recovery_policy, 0, sizeof(g_recovery_policy));
    
    g_service_count = 0;
    g_alert_handler = NULL;
    g_alert_context = NULL;
    g_monitor_running = false;
    
    /* 设置默认恢复策略 */
    for (u32 i = 0; i < MAX_MONITORED_SERVICES; i++) {
        g_recovery_policy[i].max_restart_attempts = DEFAULT_MAX_RESTART_ATTEMPTS;
        g_recovery_policy[i].restart_delay_ms = DEFAULT_RESTART_DELAY_MS;
        g_recovery_policy[i].escalate_on_failure = true;
    }
    
    console_puts("[MONITOR_SVC] Monitor service (policy layer) initialized\n");
    
    return HIC_SUCCESS;
}

/**
 * 启动监控服务（策略层）
 */
hic_status_t monitor_service_start(void)
{
    g_monitor_running = true;
    
    console_puts("[MONITOR_SVC] Monitor service started\n");
    
    return HIC_SUCCESS;
}

/**
 * 停止监控服务（策略层）
 */
hic_status_t monitor_service_stop(void)
{
    g_monitor_running = false;
    
    console_puts("[MONITOR_SVC] Monitor service stopped\n");
    
    return HIC_SUCCESS;
}

/* ==================== 策略层：服务管理 ==================== */

/**
 * 注册服务到监控
 */
hic_status_t monitor_register_service(domain_id_t domain, const char* name,
                                       bool critical, bool auto_restart)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    service_info_t* svc = &g_services[domain];
    
    svc->domain = domain;
    if (name) {
        strncpy(svc->name, name, sizeof(svc->name) - 1);
    }
    svc->critical = critical;
    svc->auto_restart = auto_restart;
    svc->state = SERVICE_STATE_STOPPED;
    svc->restart_count = 0;
    svc->crash_count = 0;
    svc->heartbeat_timeout_ms = DEFAULT_HEARTBEAT_TIMEOUT_MS;
    svc->last_heartbeat = hal_get_timestamp();
    
    g_service_count++;
    
    console_puts("[MONITOR_SVC] Registered service: ");
    console_puts(svc->name);
    console_puts(" (domain=");
    console_putu64(domain);
    console_puts(")\n");
    
    return HIC_SUCCESS;
}

/**
 * 注销服务
 */
hic_status_t monitor_unregister_service(domain_id_t domain)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memset(&g_services[domain], 0, sizeof(service_info_t));
    
    if (g_service_count > 0) {
        g_service_count--;
    }
    
    return HIC_SUCCESS;
}

/**
 * 获取服务信息
 */
service_info_t* monitor_get_service_info(domain_id_t domain)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return NULL;
    }
    
    return &g_services[domain];
}

/* ==================== 策略层：健康检查 ==================== */

/**
 * 心跳更新（服务调用）
 */
hic_status_t monitor_heartbeat(domain_id_t domain)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    service_info_t* svc = &g_services[domain];
    
    if (svc->state == SERVICE_STATE_STOPPED) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    svc->last_heartbeat = hal_get_timestamp();
    
    /* 如果服务正在恢复，标记为运行 */
    if (svc->state == SERVICE_STATE_RECOVERING) {
        svc->state = SERVICE_STATE_RUNNING;
    }
    
    return HIC_SUCCESS;
}

/**
 * 检查服务心跳超时
 */
static bool check_heartbeat_timeout(service_info_t* svc)
{
    u64 now = hal_get_timestamp();
    u64 elapsed_ms = (now - svc->last_heartbeat) / 1000;
    
    return elapsed_ms > svc->heartbeat_timeout_ms;
}

/**
 * 检查系统健康状态
 */
hic_status_t monitor_check_system_health(void)
{
    u64 now = hal_get_timestamp();
    
    for (u32 i = 0; i < MAX_MONITORED_SERVICES; i++) {
        service_info_t* svc = &g_services[i];
        
        if (svc->state == SERVICE_STATE_RUNNING) {
            /* 检查心跳超时 */
            if (check_heartbeat_timeout(svc)) {
                console_puts("[MONITOR_SVC] Service ");
                console_puts(svc->name);
                console_puts(" heartbeat timeout\n");
                
                /* 策略决策：标记为崩溃 */
                svc->state = SERVICE_STATE_CRASHED;
                svc->crash_count++;
                svc->last_crash_time = now;
                
                /* 记录事件到机制层 */
                monitor_record_event(MONITOR_EVT_SERVICE_CRASH, i);
                
                /* 触发告警 */
                monitor_raise_alert(ALERT_LEVEL_ERROR, i, "Service heartbeat timeout");
                
                /* 策略决策：自动恢复 */
                if (svc->auto_restart) {
                    monitor_execute_recovery(i);
                }
            }
        }
    }
    
    return HIC_SUCCESS;
}

/**
 * 获取系统统计
 */
void monitor_get_system_stats(u64* running_services, u64* crashed_services,
                               u64* total_cpu, u64* total_memory)
{
    u64 running = 0;
    u64 crashed = 0;
    
    for (u32 i = 0; i < MAX_MONITORED_SERVICES; i++) {
        if (g_services[i].domain != 0) {
            if (g_services[i].state == SERVICE_STATE_RUNNING) {
                running++;
            } else if (g_services[i].state == SERVICE_STATE_CRASHED) {
                crashed++;
            }
        }
    }
    
    if (running_services) *running_services = running;
    if (crashed_services) *crashed_services = crashed;
    if (total_cpu) *total_cpu = 0;  /* TODO: 实现 */
    if (total_memory) *total_memory = 0;  /* TODO: 实现 */
}

/* ==================== 策略层：自动恢复 ==================== */

/**
 * 执行恢复动作
 */
hic_status_t monitor_execute_recovery(domain_id_t domain)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    service_info_t* svc = &g_services[domain];
    recovery_policy_t* policy = &g_recovery_policy[domain];
    
    /* 策略决策：检查重启次数限制 */
    if (svc->restart_count >= policy->max_restart_attempts) {
        console_puts("[MONITOR_SVC] Service ");
        console_puts(svc->name);
        console_puts(" exceeded max restart attempts\n");
        
        monitor_raise_alert(ALERT_LEVEL_CRITICAL, domain, 
                            "Service exceeded max restart attempts");
        
        /* 策略决策：升级处理 */
        if (policy->escalate_on_failure) {
            /* 通知管理员或执行其他恢复策略 */
            monitor_execute_action(MONITOR_ACTION_TERMINATE, domain);
        }
        
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    console_puts("[MONITOR_SVC] Recovering service: ");
    console_puts(svc->name);
    console_puts("\n");
    
    svc->state = SERVICE_STATE_RECOVERING;
    
    /* 捕获崩溃现场（机制层调用） */
    crash_dump_capture(domain, 0, 0);
    
    /* 延迟后重启 */
    if (policy->restart_delay_ms > 0) {
        hal_udelay(policy->restart_delay_ms * 1000);
    }
    
    /* 执行重启 */
    hic_status_t status = monitor_restart_service(domain);
    
    if (status == HIC_SUCCESS) {
        svc->restart_count++;
        svc->state = SERVICE_STATE_RUNNING;
        
        /* 记录事件 */
        monitor_record_event(MONITOR_EVT_SERVICE_START, domain);
    } else {
        svc->state = SERVICE_STATE_CRASHED;
    }
    
    return status;
}

/**
 * 重启服务（策略决策）
 */
hic_status_t monitor_restart_service(domain_id_t domain)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    service_info_t* svc = &g_services[domain];
    
    console_puts("[MONITOR_SVC] Restarting service: ");
    console_puts(svc->name);
    console_puts("\n");
    
    /*
     * 策略层决策：
     * 1. 检查服务依赖
     * 2. 确定重启顺序
     * 3. 调用模块管理器重新加载
     * 
     * 这里调用机制层动作执行暂停，然后重新启动
     */
    
    /* 暂停旧实例 */
    monitor_execute_action(MONITOR_ACTION_SUSPEND, domain);
    
    /* TODO: 调用模块管理器重新加载服务 */
    /* 这里需要与 module_manager_service 集成 */
    
    /* 重置心跳 */
    svc->last_heartbeat = hal_get_timestamp();
    svc->state = SERVICE_STATE_RUNNING;
    
    return HIC_SUCCESS;
}

/**
 * 配置自动恢复策略
 */
hic_status_t monitor_set_recovery_policy(domain_id_t domain,
                                          u32 max_restart_attempts,
                                          u32 restart_delay_ms,
                                          bool escalate_on_failure)
{
    if (domain >= MAX_MONITORED_SERVICES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    recovery_policy_t* policy = &g_recovery_policy[domain];
    
    policy->max_restart_attempts = max_restart_attempts;
    policy->restart_delay_ms = restart_delay_ms;
    policy->escalate_on_failure = escalate_on_failure;
    
    return HIC_SUCCESS;
}

/* ==================== 策略层：规则配置 ==================== */

/**
 * 配置监控规则
 */
hic_status_t monitor_configure_rule(monitor_event_type_policy_t event_type,
                                     u32 threshold, u32 window_ms,
                                     monitor_action_t action)
{
    monitor_rule_t rule = {
        .event_type = (monitor_event_type_t)event_type,
        .threshold = threshold,
        .window_ms = window_ms,
        .action = action,
        .enabled = true
    };
    
    return monitor_set_rule(&rule);
}

/**
 * 启用/禁用规则
 */
hic_status_t monitor_enable_rule(monitor_event_type_policy_t event_type, 
                                  bool enabled)
{
    monitor_rule_t rule;
    
    if (monitor_get_rule((monitor_event_type_t)event_type, &rule) != HIC_SUCCESS) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    rule.enabled = enabled;
    return monitor_set_rule(&rule);
}

/* ==================== 策略层：告警配置 ==================== */

/**
 * 注册告警处理器
 */
hic_status_t monitor_register_alert_handler(alert_handler_t handler, void* context)
{
    g_alert_handler = handler;
    g_alert_context = context;
    
    return HIC_SUCCESS;
}

/**
 * 触发告警
 */
void monitor_raise_alert(alert_level_t level, domain_id_t domain, 
                          const char* message)
{
    console_puts("[MONITOR_SVC] ALERT (level=");
    console_putu64(level);
    console_puts("): ");
    console_puts(message);
    console_puts("\n");
    
    /* 记录到审计日志 */
    u64 data[4] = { level, domain, 0, 0 };
    audit_log_event(AUDIT_EVENT_SECURITY_VIOLATION, domain, 0, 0, data, 4, 0);
    
    /* 调用注册的处理器 */
    if (g_alert_handler) {
        g_alert_handler(level, domain, message, g_alert_context);
    }
}

/* ==================== 策略层：事件报告 ==================== */

/**
 * 报告事件（服务调用）
 */
void monitor_report_event(monitor_event_type_policy_t event_type, 
                          domain_id_t domain,
                          const u64* data, u32 data_count)
{
    /* 记录到机制层 */
    monitor_record_event((monitor_event_type_t)event_type, domain);
    
    /* 策略决策：根据事件类型执行动作 */
    switch (event_type) {
        case MONITOR_EVT_SERVICE_CRASH:
            g_services[domain].state = SERVICE_STATE_CRASHED;
            g_services[domain].crash_count++;
            g_services[domain].last_crash_time = hal_get_timestamp();
            
            /* 捕获崩溃现场 */
            if (data_count >= 2) {
                crash_dump_capture(domain, data[0], data[1]);
            }
            
            /* 自动恢复 */
            if (g_services[domain].auto_restart) {
                monitor_execute_recovery(domain);
            }
            break;
            
        case MONITOR_EVT_RESOURCE_EXHAUSTED:
            monitor_raise_alert(ALERT_LEVEL_WARNING, domain, "Resource exhausted");
            break;
            
        case MONITOR_EVT_SECURITY_VIOLATION:
            monitor_raise_alert(ALERT_LEVEL_CRITICAL, domain, "Security violation");
            break;
            
        default:
            break;
    }
}

/* ==================== 策略层：监控主循环 ==================== */

/**
 * 监控主循环（策略层）
 */
void monitor_service_loop(void)
{
    console_puts("[MONITOR_SVC] Starting monitor loop\n");
    
    while (g_monitor_running) {
        /* 1. 检查所有服务健康状态 */
        monitor_check_system_health();
        
        /* 2. 检查审计日志使用率 */
        u64 audit_usage = audit_get_buffer_usage();
        if (audit_usage > 90) {
            monitor_raise_alert(ALERT_LEVEL_WARNING, 0, "Audit log buffer nearly full");
            monitor_record_event(MONITOR_EVT_AUDIT_LOG_FULL, 0);
        }
        
        /* 3. 检查阈值规则 */
        for (u32 i = 0; i < MAX_MONITORED_SERVICES; i++) {
            monitor_rule_t rule;
            
            for (u32 e = 0; e < MONITOR_EVENT_TYPE_COUNT; e++) {
                if (monitor_get_rule((monitor_event_type_t)e, &rule) == HIC_SUCCESS) {
                    if (monitor_check_threshold(&rule, i)) {
                        /* 执行动作 */
                        monitor_execute_action(rule.action, i);
                    }
                }
            }
        }
        
        /* 4. 输出统计信息 */
        u64 running, crashed;
        monitor_get_system_stats(&running, &crashed, NULL, NULL);
        
        if (running > 0 || crashed > 0) {
            console_puts("[MONITOR_SVC] Stats: Running=");
            console_putu64(running);
            console_puts(", Crashed=");
            console_putu64(crashed);
            console_puts("\n");
        }
        
        /* 5. 等待下一个周期 */
        hal_udelay(1000000); /* 1秒 */
    }
    
    console_puts("[MONITOR_SVC] Monitor loop exited\n");
}
