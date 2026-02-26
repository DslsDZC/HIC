/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC监控服务
 * 遵循文档第2.1节：故障隔离与恢复
 * 负责系统监控、服务恢复、资源统计
 */

#ifndef HIC_KERNEL_MONITOR_H
#define HIC_KERNEL_MONITOR_H

#include "types.h"
#include "domain.h"

/* 监控事件类型 */
typedef enum {
    MONITOR_EVENT_SERVICE_START,      /* 服务启动 */
    MONITOR_EVENT_SERVICE_STOP,       /* 服务停止 */
    MONITOR_EVENT_SERVICE_CRASH,      /* 服务崩溃 */
    MONITOR_EVENT_RESOURCE_EXHAUSTED, /* 资源耗尽 */
    MONITOR_EVENT_SECURITY_VIOLATION, /* 安全违规 */
    MONITOR_EVENT_AUDIT_LOG_FULL,     /* 审计日志满 */
} monitor_event_type_t;

/* 监控事件 */
typedef struct {
    monitor_event_type_t type;
    domain_id_t domain;
    u64 timestamp;
    u64 data[4];
} monitor_event_t;

/* 服务状态 */
typedef enum {
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_STARTING,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_CRASHED,
} service_state_t;

/* 服务信息 */
typedef struct {
    domain_id_t domain;
    char name[64];
    service_state_t state;
    u64 restart_count;
    u64 crash_count;
    u64 last_crash_time;
} service_info_t;

/* 监控服务接口 */
void monitor_service_init(void);

/* 报告事件 */
void monitor_report_event(monitor_event_t* event);

/* 获取服务信息 */
service_info_t* monitor_get_service_info(domain_id_t domain);

/* 重启服务 */
hic_status_t monitor_restart_service(domain_id_t domain);

/* 获取系统统计 */
void monitor_get_system_stats(u64* running_services, u64* crashed_services,
                                u64* total_cpu, u64* total_memory);

/* 监控循环 */
void monitor_service_loop(void);

#endif /* HIC_KERNEL_MONITOR_H */