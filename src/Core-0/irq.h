/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 中断控制器 - 静态为主、动态为辅设计
 *
 * 设计原则：
 * 1. 构建时静态路由 - 固定设备获得极致性能
 * 2. 动态中断池 - 预留给热插拔设备（USB、PCIe）
 * 3. 中断直接送达服务 - Core-0 仅异常介入
 * 4. 无优先级管理 - 硬件静态配置
 * 5. 无亲和性管理 - 构建时静态绑定
 * 6. 无嵌套中断 - 单级处理模型
 * 7. 无下半部机制 - 无锁队列替代
 *
 * 静态设备延迟目标：<0.5μs
 * 动态设备延迟目标：<1μs
 */

#ifndef HIC_KERNEL_IRQ_H
#define HIC_KERNEL_IRQ_H

#include "types.h"
#include "capability.h"

/* ===== 中断向量范围划分 ===== */

/* 静态中断范围：构建时分配给固定设备 */
#define IRQ_STATIC_START        32
#define IRQ_STATIC_END          191
#define IRQ_STATIC_COUNT        (IRQ_STATIC_END - IRQ_STATIC_START + 1)

/* 动态中断池：预留给热插拔设备 */
#define IRQ_DYNAMIC_START       192
#define IRQ_DYNAMIC_END         223
#define IRQ_DYNAMIC_COUNT       (IRQ_DYNAMIC_END - IRQ_DYNAMIC_START + 1)

/* 异常向量：0-31 为 CPU 异常保留 */
#define IRQ_EXCEPTION_START     0
#define IRQ_EXCEPTION_END       31

/* 其他保留向量 */
#define IRQ_RESERVED_START      224
#define IRQ_RESERVED_END        255

/* ===== 中断路由表条目 ===== */

/**
 * 中断路由表条目
 * 
 * 静态范围：构建时配置，运行时只读
 * 动态范围：运行时可修改（由热插拔协调服务使用）
 */
typedef struct irq_route_entry {
    volatile u64        handler_address;  /* 服务入口点，直接跳转 */
    volatile cap_handle_t endpoint_cap;   /* 能力句柄，快速验证 */
    volatile u8         initialized;      /* 路由是否有效 */
    volatile u8         trigger_type;     /* 触发类型：0=边缘, 1=电平 */
    volatile u8         is_dynamic;       /* 是否属于动态池 */
    volatile u8         reserved;         /* 保留对齐 */
} irq_route_entry_t;

/* 触发类型 */
#define IRQ_TRIGGER_EDGE       0
#define IRQ_TRIGGER_LEVEL      1

/* 路由标志 */
#define IRQ_FLAG_DYNAMIC       0x01    /* 动态分配的中断 */

/* 中断路由表（构建时静态生成，动态部分运行时可修改） */
extern volatile irq_route_entry_t irq_table[256];

/* ===== 初始化 ===== */

/**
 * 初始化中断控制器
 * - 从构建配置加载静态路由表
 * - 初始化动态中断池
 * - 配置硬件中断控制器
 */
void irq_controller_init(void);

/* ===== 硬件控制 ===== */

/**
 * 启用指定中断向量
 */
void irq_enable(u32 vector);

/**
 * 禁用指定中断向量
 */
void irq_disable(u32 vector);

/* ===== 静态中断查询（只读） ===== */

/**
 * 检查中断向量是否属于静态范围
 */
static inline bool irq_is_static(u32 vector)
{
    return vector >= IRQ_STATIC_START && vector <= IRQ_STATIC_END;
}

/**
 * 检查中断向量是否属于动态池
 */
static inline bool irq_is_dynamic(u32 vector)
{
    return vector >= IRQ_DYNAMIC_START && vector <= IRQ_DYNAMIC_END;
}

/**
 * 检查中断路由是否有效
 */
static inline bool irq_route_valid(u32 vector)
{
    return irq_table[vector].initialized != 0;
}

/* ===== 动态中断管理（热插拔设备使用） ===== */

/**
 * 从动态池分配空闲中断向量
 * 
 * @return 分配的向量号，失败返回 (u32)-1
 */
u32 irq_dynamic_alloc(void);

/**
 * 释放动态中断向量回池
 * 
 * @param vector 要释放的向量号
 */
void irq_dynamic_free(u32 vector);

/**
 * 动态注册中断路由
 * 
 * 仅允许注册到动态池范围，静态范围拒绝修改。
 * 
 * @param vector 中断向量号
 * @param handler 处理函数地址
 * @param cap 能力句柄
 * @param trigger_type 触发类型
 * @return 成功返回 true
 */
bool irq_dynamic_register(u32 vector, u64 handler, 
                          cap_handle_t cap, u8 trigger_type);

/**
 * 动态注销中断路由
 * 
 * 仅允许注销动态池范围。
 */
void irq_dynamic_unregister(u32 vector);

/**
 * 获取动态池使用统计
 */
typedef struct irq_dynamic_stats {
    u32 total_count;      /* 动态池总向量数 */
    u32 allocated_count;  /* 已分配数量 */
    u32 free_count;       /* 空闲数量 */
    u32 high_watermark;   /* 历史最高使用量 */
} irq_dynamic_stats_t;

void irq_dynamic_get_stats(irq_dynamic_stats_t *stats);

/* ===== 分发（核心路径） ===== */

/**
 * 中断分发核心函数
 * 
 * 执行流程：
 * 1. 快速检查路由是否有效
 * 2. 快速能力验证（~2ns）
 * 3. 直接跳转到服务入口点
 * 4. 发送 EOI
 * 
 * 目标延迟：<0.5μs（静态），<1μs（动态）
 */
void irq_dispatch(u32 vector);

#endif /* HIC_KERNEL_IRQ_H */
