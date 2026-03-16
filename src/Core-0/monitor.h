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

/* ==================== 机制层：异常行为检测原语 ==================== */

/* 事件统计计数器（每域每种事件） */
#define MONITOR_EVENT_TYPE_COUNT  8
#define MONITOR_STATS_WINDOW_MS   1000  /* 统计窗口：1秒 */

typedef struct {
    u64 count;           /* 计数 */
    u64 window_start;    /* 窗口起始时间 */
} event_stat_t;

/* 阈值动作 */
typedef enum {
    MONITOR_ACTION_NONE,        /* 无动作 */
    MONITOR_ACTION_ALERT,       /* 告警 */
    MONITOR_ACTION_THROTTLE,    /* 限流 */
    MONITOR_ACTION_SUSPEND,     /* 暂停 */
    MONITOR_ACTION_TERMINATE,   /* 终止 */
} monitor_action_t;

/* 检测规则（策略层定义，机制层执行） */
typedef struct {
    monitor_event_type_t event_type;  /* 事件类型 */
    u32 threshold;                     /* 阈值 */
    u32 window_ms;                     /* 时间窗口（毫秒） */
    monitor_action_t action;           /* 触发动作 */
    bool enabled;                      /* 是否启用 */
} monitor_rule_t;

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

/* ==================== 机制层：异常行为检测原语接口 ==================== */

/**
 * 记录事件计数（机制层）
 * @param event_type 事件类型
 * @param domain 域ID
 */
void monitor_record_event(monitor_event_type_t event_type, domain_id_t domain);

/**
 * 获取事件速率（机制层）
 * @param event_type 事件类型
 * @param domain 域ID
 * @return 事件速率（每秒次数）
 */
u32 monitor_get_event_rate(monitor_event_type_t event_type, domain_id_t domain);

/**
 * 检查是否超过阈值（机制层）
 * @param rule 检测规则
 * @param domain 域ID
 * @return 是否超过阈值
 */
bool monitor_check_threshold(const monitor_rule_t* rule, domain_id_t domain);

/**
 * 执行阈值动作（机制层）
 * @param action 动作类型
 * @param domain 目标域
 * @return 操作状态
 */
hic_status_t monitor_execute_action(monitor_action_t action, domain_id_t domain);

/* ==================== 机制层：崩溃转储原语接口 ==================== */

/* 崩溃转储类型 */
typedef enum {
    CRASH_DUMP_NONE,
    CRASH_DUMP_REGISTER,   /* 寄存器状态 */
    CRASH_DUMP_STACK,      /* 调用栈 */
    CRASH_DUMP_FULL,       /* 完整内存 */
} crash_dump_type_t;

/* 崩溃转储头 */
typedef struct {
    u64 magic;             /* 魔数: 0x4849435F44554D50 ("HIC_DUMP") */
    u64 timestamp;         /* 时间戳 */
    domain_id_t domain;    /* 崩溃域 */
    crash_dump_type_t type;/* 转储类型 */
    u32 size;              /* 数据大小 */
    u64 stack_ptr;         /* 栈指针 */
    u64 instr_ptr;         /* 指令指针 */
    u64 checksum;          /* 校验和 */
} crash_dump_header_t;

#define CRASH_DUMP_MAGIC  0x4849435F44554D50ULL

/* 崩溃转储缓冲区大小 */
#define CRASH_DUMP_MAX_SIZE  4096

/**
 * 捕获崩溃现场（机制层）
 * @param domain 崩溃域
 * @param stack_ptr 栈指针
 * @param instr_ptr 指令指针
 * @return 操作状态
 */
hic_status_t crash_dump_capture(domain_id_t domain, u64 stack_ptr, u64 instr_ptr);

/**
 * 检索崩溃转储（机制层）
 * @param domain 目标域
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param out_size 实际大小
 * @return 操作状态
 */
hic_status_t crash_dump_retrieve(domain_id_t domain, void* buffer, 
                                  size_t buffer_size, size_t* out_size);

/**
 * 清除崩溃转储（机制层）
 * @param domain 目标域
 */
void crash_dump_clear(domain_id_t domain);

/* ==================== 策略层接口（供特权服务调用） ==================== */

/**
 * 设置检测规则（策略层调用）
 * @param rule 规则配置
 * @return 操作状态
 */
hic_status_t monitor_set_rule(const monitor_rule_t* rule);

/**
 * 获取检测规则（策略层调用）
 * @param event_type 事件类型
 * @param rule 输出规则
 * @return 操作状态
 */
hic_status_t monitor_get_rule(monitor_event_type_t event_type, monitor_rule_t* rule);

/**
 * 获取所有事件统计（策略层调用）
 * @param stats 输出统计数组
 * @param max_count 最大数量
 * @param out_count 实际数量
 */
void monitor_get_all_stats(event_stat_t* stats, u32 max_count, u32* out_count);

#endif /* HIC_KERNEL_MONITOR_H */