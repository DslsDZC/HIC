/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 审计日志机制层 (Audit Log Mechanism Layer)
 * 
 * Core-0 提供审计日志记录和查询的原语接口：
 * - 事件记录
 * - 日志查询
 * - 缓冲区管理
 * 
 * 策略层（audit_service）负责：
 * - 内存安全检测策略
 * - 告警处理策略
 * - 审计规则配置
 * - 安全事件响应
 */

#ifndef HIC_KERNEL_AUDIT_H
#define HIC_KERNEL_AUDIT_H

#include "types.h"

/* ========== 事件类型定义 ========== */

typedef enum {
    AUDIT_EVENT_CAP_VERIFY,
    AUDIT_EVENT_CAP_CREATE,
    AUDIT_EVENT_CAP_TRANSFER,
    AUDIT_EVENT_CAP_DERIVE,
    AUDIT_EVENT_CAP_REVOKE,
    AUDIT_EVENT_DOMAIN_CREATE,
    AUDIT_EVENT_DOMAIN_DESTROY,
    AUDIT_EVENT_DOMAIN_SUSPEND,
    AUDIT_EVENT_DOMAIN_RESUME,
    AUDIT_EVENT_THREAD_CREATE,
    AUDIT_EVENT_THREAD_DESTROY,
    AUDIT_EVENT_THREAD_SWITCH,
    AUDIT_EVENT_SYSCALL,
    AUDIT_EVENT_IRQ,
    AUDIT_EVENT_IPC_CALL,
    AUDIT_EVENT_EXCEPTION,
    AUDIT_EVENT_SECURITY_VIOLATION,
    AUDIT_EVENT_PMM_ALLOC,
    AUDIT_EVENT_PMM_FREE,
    AUDIT_EVENT_SERVICE_START,
    AUDIT_EVENT_SERVICE_STOP,
    AUDIT_EVENT_SERVICE_CRASH,
    AUDIT_EVENT_MODULE_LOAD,
    AUDIT_EVENT_MODULE_UNLOAD,
    AUDIT_EVENT_NULL_POINTER,
    AUDIT_EVENT_BUFFER_OVERFLOW,
    AUDIT_EVENT_INVALID_MEMORY,
    AUDIT_EVENT_MAX
} audit_event_type_t;

/* ========== 日志条目结构 ========== */

typedef struct audit_entry {
    u64 timestamp;
    u32 sequence;
    audit_event_type_t type;
    domain_id_t domain;
    cap_id_t cap_id;
    thread_id_t thread_id;
    u64 data[4];
    u8 result;
    u8 reserved[3];
} audit_entry_t;

/* ========== 查询过滤器 ========== */

typedef struct audit_query_filter {
    domain_id_t domain;           /* 过滤域 */
    audit_event_type_t type;      /* 过滤事件类型 */
    u64 start_time;               /* 起始时间 */
    u64 end_time;                 /* 结束时间 */
    u32 max_results;              /* 最大结果数 */
    u32 offset;                   /* 偏移 */
} audit_query_filter_t;

/* ========== 机制层接口 ========== */

/* 初始化 */
void audit_system_init(void);
void audit_system_init_buffer(phys_addr_t base, size_t size);

/* 记录事件 */
void audit_log_event(audit_event_type_t type, domain_id_t domain,
                     cap_id_t cap, thread_id_t thread_id,
                     u64 *data, u32 data_count, u8 result);

/* 查询接口 */
hic_status_t audit_query(const audit_query_filter_t *filter,
                          void *buffer, size_t buffer_size, size_t *out_size);
hic_status_t audit_query_by_domain(domain_id_t domain,
                                    audit_entry_t *buffer,
                                    u32 buffer_count, u32 *out_count);
hic_status_t audit_query_by_type(audit_event_type_t type,
                                  audit_entry_t *buffer,
                                  u32 buffer_count, u32 *out_count);
hic_status_t audit_query_latest(audit_entry_t *buffer, u32 count, u32 *out_count);

/* 统计 */
u64 audit_get_entry_count(void);
u64 audit_get_buffer_usage(void);

/* ========== 便捷宏 ========== */

#define AUDIT_LOG_CAP_CREATE(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_CREATE, domain, cap, 0, NULL, 0, result)

#define AUDIT_LOG_DOMAIN_CREATE(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_SERVICE_START(domain, result) \
    audit_log_event(AUDIT_EVENT_SERVICE_START, domain, 0, 0, NULL, 0, result)

#define AUDIT_LOG_MODULE_LOAD(domain, code_size, data_size, code_phys, result) \
    do { u64 _d[4] = {domain, code_size, data_size, code_phys}; \
         audit_log_event(AUDIT_EVENT_MODULE_LOAD, domain, 0, 0, _d, 4, result); } while(0)

#define AUDIT_LOG_SECURITY_VIOLATION(domain, reason) \
do { u64 _r = reason; \
audit_log_event(AUDIT_EVENT_SECURITY_VIOLATION, domain, 0, 0, &_r, 1, 0); } while(0)

#define AUDIT_LOG_DOMAIN_SWITCH(from, to, cap) \
audit_log_event(AUDIT_EVENT_THREAD_SWITCH, to, cap, from, NULL, 0, 0)
#endif /* HIC_KERNEL_AUDIT_H */
