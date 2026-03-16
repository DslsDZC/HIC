/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 安全监控服务（策略层）
 * 
 * 实现安全审计策略、异常行为检测规则、告警响应策略。
 * Core-0 提供机制层原语，本服务定义策略层规则。
 */

#ifndef HIC_SECURITY_MONITOR_SERVICE_H
#define HIC_SECURITY_MONITOR_SERVICE_H

#include "types.h"

/* ==================== 策略配置 ==================== */

/* 检测规则配置 */
typedef struct {
    const char* name;           /* 规则名称 */
    u32 event_type;             /* 事件类型 */
    u32 threshold;              /* 阈值 */
    u32 window_ms;              /* 时间窗口 */
    u32 action;                 /* 动作 */
    bool enabled;               /* 是否启用 */
} security_rule_config_t;

/* 告警策略配置 */
typedef struct {
    u32 max_alerts_per_minute;  /* 每分钟最大告警数 */
    bool log_to_audit;          /* 记录到审计日志 */
    bool notify_admin;          /* 通知管理员 */
    u32 cooldown_ms;            /* 冷却时间 */
} alert_policy_t;

/* 响应策略配置 */
typedef struct {
    bool auto_suspend;          /* 自动暂停恶意域 */
    bool auto_terminate;        /* 自动终止攻击域 */
    u32 grace_period_ms;        /* 宽限期 */
    u32 max_violations;         /* 最大违规次数 */
} response_policy_t;

/* 服务配置 */
typedef struct {
    security_rule_config_t* rules;
    u32 rule_count;
    alert_policy_t alert_policy;
    response_policy_t response_policy;
    u32 scan_interval_ms;       /* 扫描间隔 */
} security_monitor_config_t;

/* ==================== 服务接口 ==================== */

/**
 * 初始化安全监控服务
 */
void security_monitor_init(void);

/**
 * 启动安全监控
 */
void security_monitor_start(void);

/**
 * 停止安全监控
 */
void security_monitor_stop(void);

/**
 * 加载配置
 */
void security_monitor_load_config(const security_monitor_config_t* config);

/**
 * 添加检测规则
 */
int security_monitor_add_rule(const security_rule_config_t* rule);

/**
 * 移除检测规则
 */
int security_monitor_remove_rule(const char* name);

/**
 * 获取告警统计
 */
void security_monitor_get_alert_stats(u64* total_alerts, u64* active_threats);

/**
 * 处理崩溃转储
 */
void security_monitor_handle_crash(u32 domain);

/**
 * 生成安全报告
 */
void security_monitor_generate_report(void);

#endif /* HIC_SECURITY_MONITOR_SERVICE_H */
