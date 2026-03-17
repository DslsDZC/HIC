/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 审计服务实现 (策略层)
 */

#include "service.h"

/* ========== 外部接口 ========== */

extern void console_puts(const char *s);
extern void console_putu64(u64 v);
extern void console_puthex64(u64 v);

/* Core-0 审计机制 */
extern void audit_log_event(u32 type, u32 domain, u32 cap, u32 thread,
                            u64 *data, u32 count, u8 result);

/* ========== 服务状态 ========== */

#define MAX_AUDIT_RULES 32

static struct {
    audit_rule_t rules[MAX_AUDIT_RULES];
    u32 rule_count;
    bool initialized;
    
    /* 告警统计 */
    u64 alert_counts[4];  /* 按级别统计 */
} g_audit_svc = {0};

/* ========== 初始化 ========== */

void audit_service_init(void)
{
    console_puts("[AUDIT_SVC] Initializing audit service...\n");
    
    g_audit_svc.rule_count = 0;
    g_audit_svc.initialized = true;
    
    /* 默认规则 */
    audit_rule_t default_rules[] = {
        { 27, true, 1, AUDIT_ALERT_ERROR },     /* NULL_POINTER */
        { 28, true, 1, AUDIT_ALERT_CRITICAL },  /* BUFFER_OVERFLOW */
        { 29, true, 1, AUDIT_ALERT_ERROR },     /* INVALID_MEMORY */
        { 18, true, 5, AUDIT_ALERT_WARNING },   /* SECURITY_VIOLATION */
    };
    
    for (int i = 0; i < 4; i++) {
        audit_service_add_rule(&default_rules[i]);
    }
    
    console_puts("[AUDIT_SVC] Audit service initialized\n");
}

/* ========== 规则配置 ========== */

hic_status_t audit_service_add_rule(const audit_rule_t *rule)
{
    if (!rule || g_audit_svc.rule_count >= MAX_AUDIT_RULES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_audit_svc.rules[g_audit_svc.rule_count++] = *rule;
    return HIC_SUCCESS;
}

hic_status_t audit_service_remove_rule(u32 event_type)
{
    for (u32 i = 0; i < g_audit_svc.rule_count; i++) {
        if (g_audit_svc.rules[i].event_type == event_type) {
            /* 移动后面的规则 */
            for (u32 j = i; j < g_audit_svc.rule_count - 1; j++) {
                g_audit_svc.rules[j] = g_audit_svc.rules[j + 1];
            }
            g_audit_svc.rule_count--;
            return HIC_SUCCESS;
        }
    }
    return HIC_ERROR_NOT_FOUND;
}

void audit_service_enable_rule(u32 event_type, bool enable)
{
    for (u32 i = 0; i < g_audit_svc.rule_count; i++) {
        if (g_audit_svc.rules[i].event_type == event_type) {
            g_audit_svc.rules[i].enabled = enable;
            return;
        }
    }
}

/* ========== 内存安全检测 ========== */

void audit_service_check_null_pointer(void *ptr, const char *context)
{
    if (ptr == NULL) {
        u64 data[2] = { 0, 0 };
        if (context) {
            /* 简化：只复制部分上下文 */
            data[1] = (u64)context;
        }
        
        console_puts("[AUDIT_SVC] NULL POINTER: ");
        if (context) console_puts(context);
        console_puts("\n");
        
        audit_log_event(27, 0, 0, 0, data, 2, 0);  /* AUDIT_EVENT_NULL_POINTER */
        audit_service_raise_alert(AUDIT_ALERT_ERROR, "Null pointer detected");
    }
}

void audit_service_check_buffer_overflow(void *ptr, size_t size, size_t max, const char *context)
{
    if (size > max) {
        u64 data[4] = { (u64)ptr, size, max, 0 };
        
        console_puts("[AUDIT_SVC] BUFFER OVERFLOW: size=");
        console_putu64(size);
        console_puts(", max=");
        console_putu64(max);
        console_puts("\n");
        
        audit_log_event(28, 0, 0, 0, data, 4, 0);  /* AUDIT_EVENT_BUFFER_OVERFLOW */
        audit_service_raise_alert(AUDIT_ALERT_CRITICAL, "Buffer overflow detected");
    }
}

void audit_service_check_invalid_memory(void *ptr, const char *context)
{
    u64 addr = (u64)ptr;
    
    /* 检查是否在合理范围内 */
    if (addr < 0x1000 || addr >= 0x800000000000ULL) {
        u64 data[2] = { addr, 0 };
        
        console_puts("[AUDIT_SVC] INVALID MEMORY: 0x");
        console_puthex64(addr);
        console_puts("\n");
        
        audit_log_event(29, 0, 0, 0, data, 2, 0);  /* AUDIT_EVENT_INVALID_MEMORY */
        audit_service_raise_alert(AUDIT_ALERT_ERROR, "Invalid memory access");
    }
}

/* ========== 告警处理 ========== */

void audit_service_raise_alert(audit_alert_level_t level, const char *message)
{
    g_audit_svc.alert_counts[level]++;
    
    console_puts("[AUDIT_SVC] ALERT[");
    console_putu64(level);
    console_puts("]: ");
    if (message) console_puts(message);
    console_puts("\n");
    
    /* 根据级别采取不同行动 */
    switch (level) {
        case AUDIT_ALERT_CRITICAL:
            /* 关键告警：可能需要暂停相关服务 */
            console_puts("[AUDIT_SVC] Critical alert - consider service suspension\n");
            break;
        case AUDIT_ALERT_ERROR:
            /* 错误：记录并监控 */
            break;
        case AUDIT_ALERT_WARNING:
            /* 警告：仅记录 */
            break;
        default:
            break;
    }
}

void audit_service_process_alerts(void)
{
    /* 检查告警统计，决定是否需要采取措施 */
    if (g_audit_svc.alert_counts[AUDIT_ALERT_CRITICAL] > 10) {
        console_puts("[AUDIT_SVC] Too many critical alerts!\n");
    }
}

/* ========== 安全事件响应 ========== */

void audit_service_handle_security_violation(domain_id_t domain, u64 reason)
{
    console_puts("[AUDIT_SVC] Security violation in domain ");
    console_putu64(domain);
    console_puts(", reason: ");
    console_putu64(reason);
    console_puts("\n");
    
    /* 策略：记录告警，可能暂停域 */
    audit_service_raise_alert(AUDIT_ALERT_CRITICAL, "Security violation");
    
    /* 可以调用 Core-0 机制暂停域 */
    /* domain_suspend(domain); */
}

void audit_service_handle_service_crash(domain_id_t domain, u64 reason)
{
    console_puts("[AUDIT_SVC] Service crash in domain ");
    console_putu64(domain);
    console_puts("\n");
    
    /* 策略：记录并通知监控服务 */
    u64 data[2] = { domain, reason };
    audit_log_event(23, domain, 0, 0, data, 2, 0);  /* AUDIT_EVENT_SERVICE_CRASH */
}

/* ========== 主循环 ========== */

void audit_service_run(void)
{
    /* 处理告警 */
    audit_service_process_alerts();
    
    /* 检查审计缓冲区使用率 */
    extern u64 audit_get_buffer_usage(void);
    u64 usage = audit_get_buffer_usage();
    
    if (usage > 80) {
        console_puts("[AUDIT_SVC] Buffer usage high: ");
        console_putu64(usage);
        console_puts("%\n");
    }
}
