/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC监控服务 - 机制层接口
 * 
 * 本文件仅包含机制层原语，不包含策略决策。
 * 策略层实现已移动到 Privileged-1/monitor_service/
 * 
 * 机制层职责：
 * - 提供事件计数原语（不决策如何响应）
 * - 提供阈值检查原语（不决策阈值是多少）
 * - 提供动作执行原语（不决策何时执行）
 * - 提供崩溃转储捕获原语（不决策如何处理）
 * 
 * 策略层职责（Privileged-1/monitor_service）：
 * - 监控循环和调度
 * - 规则配置和管理
 * - 服务重启决策
 * - 健康检查策略
 * - 告警和通知策略
 */

#ifndef HIC_KERNEL_MONITOR_H
#define HIC_KERNEL_MONITOR_H

#include "types.h"
#include "domain.h"

/* ==================== 机制层：事件计数原语 ==================== */

/* 事件类型（机制层枚举，不含语义） */
typedef enum {
    MONITOR_EVENT_TYPE_0 = 0,
    MONITOR_EVENT_TYPE_1,
    MONITOR_EVENT_TYPE_2,
    MONITOR_EVENT_TYPE_3,
    MONITOR_EVENT_TYPE_4,
    MONITOR_EVENT_TYPE_5,
    MONITOR_EVENT_TYPE_6,
    MONITOR_EVENT_TYPE_7,
    MONITOR_EVENT_TYPE_COUNT
} monitor_event_type_t;

/* 统计窗口大小（毫秒） */
#define MONITOR_STATS_WINDOW_MS   1000

/* 事件统计结构 */
typedef struct {
    u64 count;           /* 计数 */
    u64 window_start;    /* 窗口起始时间 */
} event_stat_t;

/**
 * 记录事件计数（机制层）
 * 
 * 功能：原子性地增加事件计数器
 * 不决策：不判断事件是否异常，不触发任何响应
 * 
 * @param event_type 事件类型索引
 * @param domain 域ID
 */
void monitor_record_event(monitor_event_type_t event_type, domain_id_t domain);

/**
 * 获取事件速率（机制层）
 * 
 * 功能：计算时间窗口内的事件速率
 * 不决策：不判断速率是否过高
 * 
 * @param event_type 事件类型索引
 * @param domain 域ID
 * @return 事件速率（每秒次数）
 */
u32 monitor_get_event_rate(monitor_event_type_t event_type, domain_id_t domain);

/**
 * 获取事件计数（机制层）
 * 
 * 功能：获取原始事件计数
 * 
 * @param event_type 事件类型索引
 * @param domain 域ID
 * @return 事件计数
 */
u64 monitor_get_event_count(monitor_event_type_t event_type, domain_id_t domain);

/* ==================== 机制层：阈值检查原语 ==================== */

/* 阈值动作类型（机制层枚举） */
typedef enum {
    MONITOR_ACTION_NONE = 0,
    MONITOR_ACTION_ALERT,
    MONITOR_ACTION_THROTTLE,
    MONITOR_ACTION_SUSPEND,
    MONITOR_ACTION_TERMINATE,
} monitor_action_t;

/* 检测规则（由策略层定义，机制层执行） */
typedef struct {
    monitor_event_type_t event_type;  /* 事件类型 */
    u32 threshold;                     /* 阈值 */
    u32 window_ms;                     /* 时间窗口 */
    monitor_action_t action;           /* 触发动作 */
    bool enabled;                      /* 是否启用 */
} monitor_rule_t;

/**
 * 检查是否超过阈值（机制层）
 * 
 * 功能：比较速率与阈值
 * 不决策：不设置阈值，只返回比较结果
 * 
 * @param rule 检测规则（由策略层提供）
 * @param domain 域ID
 * @return 是否超过阈值
 */
bool monitor_check_threshold(const monitor_rule_t* rule, domain_id_t domain);

/* ==================== 机制层：动作执行原语 ==================== */

/**
 * 执行阈值动作（机制层）
 * 
 * 功能：执行指定的动作
 * 不决策：不判断是否应该执行，只执行
 * 
 * @param action 动作类型
 * @param domain 目标域
 * @return 操作状态
 */
hic_status_t monitor_execute_action(monitor_action_t action, domain_id_t domain);

/* ==================== 机制层：崩溃转储原语 ==================== */

/* 崩溃转储类型 */
typedef enum {
    CRASH_DUMP_NONE = 0,
    CRASH_DUMP_REGISTER,   /* 寄存器状态 */
    CRASH_DUMP_STACK,      /* 调用栈 */
    CRASH_DUMP_FULL,       /* 完整内存 */
} crash_dump_type_t;

/* 崩溃转储头 */
typedef struct {
    u64 magic;             /* 魔数 */
    u64 timestamp;         /* 时间戳 */
    domain_id_t domain;    /* 崩溃域 */
    crash_dump_type_t type;/* 转储类型 */
    u32 size;              /* 数据大小 */
    u64 stack_ptr;         /* 栈指针 */
    u64 instr_ptr;         /* 指令指针 */
    u64 checksum;          /* 校验和 */
} crash_dump_header_t;

#define CRASH_DUMP_MAGIC  0x4849435F44554D50ULL
#define CRASH_DUMP_MAX_SIZE  4096

/**
 * 捕获崩溃现场（机制层）
 * 
 * 功能：保存崩溃时的寄存器和栈状态
 * 不决策：不判断崩溃原因，不决定是否重启
 * 
 * @param domain 崩溃域
 * @param stack_ptr 栈指针
 * @param instr_ptr 指令指针
 * @return 操作状态
 */
hic_status_t crash_dump_capture(domain_id_t domain, u64 stack_ptr, u64 instr_ptr);

/**
 * 检索崩溃转储（机制层）
 * 
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
 * 
 * @param domain 目标域
 */
void crash_dump_clear(domain_id_t domain);

/* ==================== 机制层：统计原语 ==================== */

/**
 * 获取所有事件统计（机制层）
 * 
 * @param stats 输出统计数组
 * @param max_count 最大数量
 * @param out_count 实际数量
 */
void monitor_get_all_stats(event_stat_t* stats, u32 max_count, u32* out_count);

/* ==================== 机制层：初始化 ==================== */

/**
 * 初始化监控机制层
 * 
 * 功能：初始化数据结构，不启动监控循环
 */
void monitor_mechanism_init(void);

/* ==================== 供策略层调用的规则接口 ==================== */

/**
 * 设置检测规则（策略层调用）
 * 
 * @param rule 规则配置
 * @return 操作状态
 */
hic_status_t monitor_set_rule(const monitor_rule_t* rule);

/**
 * 获取检测规则（策略层调用）
 * 
 * @param event_type 事件类型
 * @param rule 输出规则
 * @return 操作状态
 */
hic_status_t monitor_get_rule(monitor_event_type_t event_type, monitor_rule_t* rule);

#endif /* HIC_KERNEL_MONITOR_H */
