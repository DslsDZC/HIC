/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 审计服务 (策略层)
 * 
 * 实现审计策略：
 * - 内存安全检测策略
 * - 告警处理策略
 * - 审计规则配置
 * - 安全事件响应
 */

#ifndef AUDIT_SERVICE_H
#define AUDIT_SERVICE_H

#include <common.h>

/* ========== 告警级别 ========== */

typedef enum audit_alert_level {
    AUDIT_ALERT_INFO = 0,
    AUDIT_ALERT_WARNING,
    AUDIT_ALERT_ERROR,
    AUDIT_ALERT_CRITICAL,
} audit_alert_level_t;

/* ========== 审计规则 ========== */

typedef struct audit_rule {
    u32 event_type;             /* 监控的事件类型 */
    bool enabled;               /* 是否启用 */
    u32 threshold;              /* 阈值 */
    audit_alert_level_t level;  /* 告警级别 */
} audit_rule_t;

/* ========== 服务接口 ========== */

void audit_service_init(void);

/* 规则配置 */
hic_status_t audit_service_add_rule(const audit_rule_t *rule);
hic_status_t audit_service_remove_rule(u32 event_type);
void audit_service_enable_rule(u32 event_type, bool enable);

/* 内存安全检测（策略） */
void audit_service_check_null_pointer(void *ptr, const char *context);
void audit_service_check_buffer_overflow(void *ptr, size_t size, size_t max, const char *context);
void audit_service_check_invalid_memory(void *ptr, const char *context);

/* 告警处理 */
void audit_service_raise_alert(audit_alert_level_t level, const char *message);
void audit_service_process_alerts(void);

/* 安全事件响应 */
void audit_service_handle_security_violation(domain_id_t domain, u64 reason);
void audit_service_handle_service_crash(domain_id_t domain, u64 reason);

/* 主循环 */
void audit_service_run(void);

#endif /* AUDIT_SERVICE_H */
