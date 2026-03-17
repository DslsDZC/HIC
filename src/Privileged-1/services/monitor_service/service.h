/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC监控服务 - 策略层实现
 * 
 * 本服务实现监控策略决策，使用 Core-0 机制层原语。
 * 
 * 策略层职责：
 * - 监控循环和调度策略
 * - 规则配置和管理
 * - 服务重启决策策略
 * - 健康检查策略
 * - 告警和通知策略
 * - 自动恢复策略
 * 
 * 使用的机制层原语（Core-0）：
 * - monitor_record_event() - 记录事件
 * - monitor_get_event_rate() - 获取速率
 * - monitor_check_threshold() - 检查阈值
 * - monitor_execute_action() - 执行动作
 * - crash_dump_capture() - 捕获崩溃
 */

#ifndef HIC_MONITOR_SERVICE_H
#define HIC_MONITOR_SERVICE_H

#include "../../include/common.h"
#include "../../include/module_types.h"

/* ==================== 策略层：事件类型定义（语义化） ==================== */

/* 事件类型映射到机制层索引 */
typedef enum {
    MONITOR_EVT_SERVICE_START = 0,      /* 服务启动 */
    MONITOR_EVT_SERVICE_STOP,           /* 服务停止 */
    MONITOR_EVT_SERVICE_CRASH,          /* 服务崩溃 */
    MONITOR_EVT_RESOURCE_EXHAUSTED,     /* 资源耗尽 */
    MONITOR_EVT_SECURITY_VIOLATION,     /* 安全违规 */
    MONITOR_EVT_AUDIT_LOG_FULL,         /* 审计日志满 */
    MONITOR_EVT_TIMEOUT,                /* 超时 */
    MONITOR_EVT_CUSTOM                  /* 自定义事件 */
} monitor_event_type_policy_t;

/* 服务状态 */
typedef enum {
    SERVICE_STATE_STOPPED = 0,
    SERVICE_STATE_STARTING,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_CRASHED,
    SERVICE_STATE_RECOVERING
} service_state_t;

/* 服务信息 */
typedef struct {
    domain_id_t domain;
    char name[64];
    service_state_t state;
    u64 restart_count;
    u64 crash_count;
    u64 last_crash_time;
    u64 last_heartbeat;
    u32 heartbeat_timeout_ms;
    bool auto_restart;
    bool critical;
} service_info_t;

/* 告警级别 */
typedef enum {
    ALERT_LEVEL_INFO = 0,
    ALERT_LEVEL_WARNING,
    ALERT_LEVEL_ERROR,
    ALERT_LEVEL_CRITICAL,
    ALERT_LEVEL_EMERGENCY
} alert_level_t;

/* 告警处理器回调 */
typedef void (*alert_handler_t)(alert_level_t level, domain_id_t domain, 
                                 const char* message, void* context);

/* ==================== 策略层：服务接口 ==================== */

/**
 * 初始化监控服务（策略层）
 */
hic_status_t monitor_service_init(void);

/**
 * 启动监控服务（策略层）
 */
hic_status_t monitor_service_start(void);

/**
 * 停止监控服务（策略层）
 */
hic_status_t monitor_service_stop(void);

/**
 * 监控主循环（策略层）
 */
void monitor_service_loop(void);

/* ==================== 策略层：服务管理 ==================== */

/**
 * 注册服务到监控
 */
hic_status_t monitor_register_service(domain_id_t domain, const char* name,
                                       bool critical, bool auto_restart);

/**
 * 注销服务
 */
hic_status_t monitor_unregister_service(domain_id_t domain);

/**
 * 获取服务信息
 */
service_info_t* monitor_get_service_info(domain_id_t domain);

/**
 * 重启服务（策略决策）
 */
hic_status_t monitor_restart_service(domain_id_t domain);

/* ==================== 策略层：健康检查 ==================== */

/**
 * 心跳更新（服务调用）
 */
hic_status_t monitor_heartbeat(domain_id_t domain);

/**
 * 检查系统健康状态
 */
hic_status_t monitor_check_system_health(void);

/**
 * 获取系统统计
 */
void monitor_get_system_stats(u64* running_services, u64* crashed_services,
                               u64* total_cpu, u64* total_memory);

/* ==================== 策略层：规则配置 ==================== */

/**
 * 配置监控规则
 */
hic_status_t monitor_configure_rule(monitor_event_type_policy_t event_type,
                                     u32 threshold, u32 window_ms,
                                     monitor_action_t action);

/**
 * 启用/禁用规则
 */
hic_status_t monitor_enable_rule(monitor_event_type_policy_t event_type, 
                                  bool enabled);

/* ==================== 策略层：告警配置 ==================== */

/**
 * 注册告警处理器
 */
hic_status_t monitor_register_alert_handler(alert_handler_t handler, void* context);

/**
 * 触发告警
 */
void monitor_raise_alert(alert_level_t level, domain_id_t domain, 
                          const char* message);

/* ==================== 策略层：自动恢复 ==================== */

/**
 * 配置自动恢复策略
 */
hic_status_t monitor_set_recovery_policy(domain_id_t domain,
                                          u32 max_restart_attempts,
                                          u32 restart_delay_ms,
                                          bool escalate_on_failure);

/**
 * 执行恢复动作
 */
hic_status_t monitor_execute_recovery(domain_id_t domain);

/* ==================== 策略层：事件报告 ==================== */

/**
 * 报告事件（服务调用）
 */
void monitor_report_event(monitor_event_type_policy_t event_type, 
                          domain_id_t domain,
                          const u64* data, u32 data_count);

#endif /* HIC_MONITOR_SERVICE_H */
