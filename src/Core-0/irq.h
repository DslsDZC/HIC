/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK中断控制器
 * 遵循文档第2.1节：处理所有异常和硬件中断
 * 中断首先交付给Core-0，由Core-0根据构建时配置的中断路由表，
 * 直接调用相应Privileged-1服务注册的中断处理函数
 */

#ifndef HIK_KERNEL_IRQ_H
#define HIK_KERNEL_IRQ_H

#include "types.h"
#include "build_config.h"

/* 中断处理函数类型 */
typedef void (*irq_handler_t)(void);

/* 受保护入口点信息 */
typedef struct protected_entry {
    domain_id_t domain_id;
    u64         handler_address;
    cap_id_t    endpoint_cap;  /* 验证用的能力 */
} protected_entry_t;

/* 初始化中断控制器 */
void irq_controller_init(void);

/* 注册中断处理函数 */
hik_status_t irq_register_handler(u32 irq_vector, domain_id_t domain, 
                                   u64 handler, cap_id_t endpoint_cap);

/* 启用中断 */
void irq_enable(u32 irq_vector);

/* 禁用中断 */
void irq_disable(u32 irq_vector);

/* 通用中断处理（汇编调用） */
void irq_common_handler(u32 irq_vector);

/* 中断分发核心函数 */
void irq_dispatch(u32 irq_vector);

#endif /* HIK_KERNEL_IRQ_H */