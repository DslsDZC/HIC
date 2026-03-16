/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 中断控制器 - 精简设计
 *
 * 设计原则：
 * 1. 构建时静态路由 - 运行时零查找
 * 2. 中断直接送达服务 - Core-0 仅异常介入
 * 3. 无优先级管理 - 硬件静态配置
 * 4. 无亲和性管理 - 构建时静态绑定
 * 5. 无中断描述符 - 直接跳转
 * 6. 无嵌套中断 - 单级处理模型
 * 7. 无下半部机制 - 无锁队列替代
 *
 * 目标延迟：0.5-1μs
 */

#ifndef HIC_KERNEL_IRQ_H
#define HIC_KERNEL_IRQ_H

#include "types.h"
#include "capability.h"

/* ===== 中断路由表条目（无锁设计） ===== */

/**
 * 中断路由表条目
 * 
 * 构建时由硬件合成系统静态配置，运行时只读。
 * 无优先级字段：优先级由硬件控制器静态配置。
 * 无亲和性字段：亲和性由构建时绑定到逻辑核心固化。
 */
typedef struct irq_route_entry {
    volatile u64        handler_address;  /* 服务入口点，直接跳转 */
    volatile cap_handle_t endpoint_cap;   /* 能力句柄，快速验证 */
    volatile u8         initialized;      /* 路由是否有效 */
    volatile u8         trigger_type;     /* 触发类型：0=边缘, 1=电平 */
} irq_route_entry_t;

/* 触发类型 */
#define IRQ_TRIGGER_EDGE   0
#define IRQ_TRIGGER_LEVEL  1

/* 中断路由表（构建时静态生成，256项覆盖全部向量） */
extern volatile irq_route_entry_t irq_table[256];

/* ===== 初始化（仅构建时调用一次） ===== */

/**
 * 初始化中断控制器
 * - 从构建配置加载静态路由表
 * - 配置硬件中断控制器
 */
void irq_controller_init(void);

/* ===== 硬件控制（仅启用/禁用，无优先级/亲和性） ===== */

/**
 * 启用指定中断向量
 * @param vector 中断向量号
 */
void irq_enable(u32 vector);

/**
 * 禁用指定中断向量
 * @param vector 中断向量号
 */
void irq_disable(u32 vector);

/* ===== 分发（核心路径，直接跳转） ===== */

/**
 * 中断分发核心函数
 * 
 * 执行流程：
 * 1. 快速检查路由是否有效
 * 2. 快速能力验证（~2ns）
 * 3. 直接跳转到服务入口点
 * 4. 发送 EOI
 * 
 * 目标延迟：<0.5μs
 */
void irq_dispatch(u32 vector);

#endif /* HIC_KERNEL_IRQ_H */
