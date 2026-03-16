/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 中断控制器 - 精简实现
 *
 * 核心设计：
 * - 构建时静态路由，运行时零查找
 * - 中断直接送达服务，Core-0 仅异常介入
 * - 无优先级/亲和性管理，硬件静态配置
 * - 无嵌套中断，单级处理模型
 * - 无下半部机制，无锁队列替代
 *
 * 目标延迟：0.5-1μs
 */

#include "irq.h"
#include "capability.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "boot_info.h"
#include "build_config.h"

/* 外部变量 */
extern boot_state_t g_boot_state;

/* ===== 中断路由表（构建时静态生成） ===== */

volatile irq_route_entry_t irq_table[256];

/* ===== 初始化（仅构建时调用一次） ===== */

void irq_controller_init(void)
{
    console_puts("[IRQ] Init: static routing, lock-free, <1μs target\n");
    
    /* 清空路由表 */
    memzero((void*)irq_table, sizeof(irq_table));
    
    /* 从构建配置加载静态路由（构建时确定，运行时只读） */
    for (u32 i = 0; i < g_build_config.num_interrupt_routes; i++) {
        interrupt_route_t *route = &g_build_config.interrupt_routes[i];
        u32 vector = route->irq_vector;
        
        if (vector < 256) {
            irq_table[vector].handler_address = route->handler_address;
            irq_table[vector].endpoint_cap = route->endpoint_cap;
            irq_table[vector].trigger_type = 
                (route->flags & IRQ_FLAG_LEVEL) ? IRQ_TRIGGER_LEVEL : IRQ_TRIGGER_EDGE;
            irq_table[vector].initialized = 1;
        }
    }
    
    /* 内存屏障确保写入可见 */
    atomic_full_barrier();
    
    console_puts("[IRQ] Static routes loaded: ");
    console_putu32(g_build_config.num_interrupt_routes);
    console_puts("\n");
}

/* ===== 硬件控制 ===== */

/**
 * IOAPIC 屏蔽/解除屏蔽（x86-64 特定）
 */
static void ioapic_mask_unmask(u32 vector, bool mask)
{
    u32 irq = vector - 32;
    volatile u32* ioapic_base = (volatile u32*)(g_boot_state.hw.io_irq.base_address);
    
    if (!ioapic_base) return;
    
    u32 index = irq * 2;
    
    /* 读取当前配置 */
    ioapic_base[0] = index;
    u32 low = ioapic_base[4];
    
    if (mask) {
        low |= 0x00010000;  /* 设置屏蔽位 */
    } else {
        low &= 0xFFFEFFFF;  /* 清除屏蔽位 */
        
        /* 设置触发模式 */
        if (irq_table[vector].trigger_type == IRQ_TRIGGER_LEVEL) {
            low |= 0x00008000;  /* 电平触发 */
        } else {
            low &= 0xFFFF7FFF;  /* 边缘触发 */
        }
    }
    
    ioapic_base[0] = index;
    ioapic_base[4] = low;
}

void irq_enable(u32 vector)
{
    if (vector >= 32 && vector < 48) {
        ioapic_mask_unmask(vector, false);
    }
}

void irq_disable(u32 vector)
{
    if (vector >= 32 && vector < 48) {
        ioapic_mask_unmask(vector, true);
    }
}

/* ===== 分发（核心路径 - <0.5μs） ===== */

/**
 * 中断分发核心函数
 * 
 * 设计要点：
 * 1. 快速检查 - 直接读取内存，无锁
 * 2. 能力验证 - 内联快路径，~2ns
 * 3. 直接跳转 - 无函数指针，同特权级
 * 4. EOI 发送 - 硬件特定
 */
void irq_dispatch(u32 vector)
{
    volatile irq_route_entry_t *entry = &irq_table[vector];
    
    /* 快速检查：路由是否有效（~1ns） */
    if (!entry->initialized || entry->handler_address == 0) {
        /* Core-0 仅在异常（非法中断）时介入 */
        return;
    }
    
    /* 快速能力验证（内联，~2ns） */
    if (entry->endpoint_cap != CAP_HANDLE_INVALID && 
        !cap_fast_check_rights(entry->endpoint_cap, 0)) {
        return;
    }
    
    /* 直接跳转到服务入口点（无间接层） */
    typedef void (*handler_t)(void);
    ((handler_t)entry->handler_address)();
    
    /* 发送 EOI（End of Interrupt） */
    if (vector >= 32 && vector < 48) {
        /* LAPIC EOI */
        volatile u32* lapic = (volatile u32*)0xFEE00000;
        lapic[0xB0 / 4] = 0;
    }
}
