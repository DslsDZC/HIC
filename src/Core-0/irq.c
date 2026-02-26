/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC中断控制器（简化但完整实现）
 * 目标延迟：0.5-1μs
 * 设计：无锁静态路由表
 */

#include "irq.h"
#include "capability.h"
#include "atomic.h"
#include "lib/mem.h"
#include "hardware_probe.h"
#include "lib/console.h"
#include "hardware_probe.h"
#include "boot_info.h"

/* 外部变量 */
extern boot_state_t g_boot_state;

/* 中断路由表（256项 - 文档要求） */
volatile irq_route_entry_t irq_table[256];

/* ==================== 初始化 ==================== */

void irq_controller_init(void)
{
    console_puts("[IRQ] Init (256 entries, lock-free)\n");
    
    /* 清空并初始化路由表 */
    memzero((void*)irq_table, sizeof(irq_table));
    
    /* 从构建配置加载静态路由 */
    for (u32 i = 0; i < g_build_config.num_interrupt_routes; i++) {
        interrupt_route_t *route = &g_build_config.interrupt_routes[i];
        u32 vector = route->irq_vector;
        
        if (vector < 256) {
            irq_table[vector].domain_id = route->target_domain;
            irq_table[vector].handler_address = route->handler_address;
            irq_table[vector].flags = route->flags;
            irq_table[vector].initialized = 1;
        }
    }
    
    atomic_full_barrier();
}

/* ==================== 注册 ==================== */

hic_status_t irq_register_handler(u32 irq_vector, domain_id_t domain, 
                                   u64 handler, cap_id_t endpoint_cap)
{
    if (irq_vector >= 256 || handler == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (cap_check_access(domain, endpoint_cap, 0) != HIC_SUCCESS) {
        return HIC_ERROR_PERMISSION;
    }
    
    bool irq_state = atomic_enter_critical();
    irq_table[irq_vector].domain_id = domain;
    irq_table[irq_vector].handler_address = handler;
    irq_table[irq_vector].endpoint_cap = endpoint_cap;
    irq_table[irq_vector].initialized = 1;
    atomic_exit_critical(irq_state);
    
    return HIC_SUCCESS;
}

/* ==================== 启用/禁用 ==================== */

static void ioapic_mask_unmask(u32 irq_vector, bool mask)
{
    u32 apic_irq = irq_vector - 32;
    volatile u32* ioapic_base = (volatile u32*)(g_boot_state.hw.io_irq.base_address);
    u32 ioapic_index = apic_irq * 2;
    
    ioapic_base[0] = ioapic_index;
    u32 low_dword = ioapic_base[4];
    
    if (mask) {
        low_dword |= 0x00000008;  /* 设置mask位 */
    } else {
        low_dword &= 0xFFFFFFF7;  /* 清除mask位 */
        
        /* 设置触发模式（根据flags） */
        u32 flags = irq_table[irq_vector].flags;
        
        if (flags & IRQ_FLAG_LEVEL) {
            /* 电平触发：设置bit 15 */
            low_dword |= 0x00008000;
        } else {
            /* 边缘触发：清除bit 15 */
            low_dword &= 0xFFFF7FFF;
        }
    }
    
    ioapic_base[0] = ioapic_index;
    ioapic_base[4] = low_dword;
}

void irq_enable(u32 irq_vector)
{
    if (irq_vector < 32) {
        /* PIC */
        __asm__ volatile (
            "inb $0x21, %%al\n"
            "and $0xFE, %%al\n"
            "outb %%al, $0x21\n"
            : : : "al"
        );
    } else if (irq_vector >= 32 && irq_vector < 48) {
        ioapic_mask_unmask(irq_vector, false);
    }
}

void irq_disable(u32 irq_vector)
{
    if (irq_vector < 32) {
        /* PIC */
        __asm__ volatile (
            "inb $0x21, %%al\n"
            "or $0x01, %%al\n"
            "outb %%al, $0x21\n"
            : : : "al"
        );
    } else if (irq_vector >= 32 && irq_vector < 48) {
        ioapic_mask_unmask(irq_vector, true);
    }
}

/* ==================== 分发（核心路径 - <0.5μs） ==================== */

void irq_dispatch(u32 irq_vector)
{
    atomic_acquire_barrier();
    
    volatile irq_route_entry_t *entry = &irq_table[irq_vector];
    
    if (!entry->initialized || entry->handler_address == 0) {
        return;
    }
    
    if (cap_check_access(entry->domain_id, entry->endpoint_cap, 0) != HIC_SUCCESS) {
        return;
    }
    
    /* 直接调用（同特权级，无切换） */
    typedef void (*handler_func_t)(void);
    ((handler_func_t)entry->handler_address)();
    
    /* 发送EOI（End of Interrupt）信号 */
    if (irq_vector < 32) {
        /* PIC */
        hal_outb(0x20, 0x20);  /* 主PIC EOI */
        if (irq_vector >= 8) {
            hal_outb(0xA0, 0x20);  /* 从PIC EOI */
        }
    } else if (irq_vector >= 32 && irq_vector < 48) {
        /* APIC - 写入EOI寄存器 */
        volatile u32* lapic_base = (volatile u32*)0xFEE00000;
        lapic_base[0xB0 / 4] = 0;  /* EOI寄存器偏移0xB0 */
    }
}

void irq_common_handler(u32 irq_vector)
{
    irq_dispatch(irq_vector);
}

/* ==================== 快速路径 ==================== */

void irq_handler_fast(u32 irq_vector)
{
    switch (irq_vector) {
        case 32: /* Timer */
            break;
        case 33: /* Keyboard */
            break;
        default:
            break;
    }
    
    /* EOI */
    hal_outb(0x20, 0x20);
    hal_outb(0xA0, 0x20);
}

/* ==================== 待处理中断 ==================== */

bool lapic_irqs_pending(void)
{
    volatile u32* irr_low = (volatile u32*)0xFEE00200;
    volatile u32* irr_high = (volatile u32*)0xFEE00210;
    
    return (*irr_low != 0) || (*irr_high != 0);
}

void lapic_handle_pending(void)
{
    volatile u32* irr_low = (volatile u32*)0xFEE00200;
    volatile u32* irr_high = (volatile u32*)0xFEE00210;
    
    u32 irr_values[2] = {*irr_low, *irr_high};
    
    for (u32 i = 0; i < 256; i++) {
        if (irr_values[i / 32] & (1U << (i % 32))) {
            irq_handler_fast(i);
        }
    }
}

bool interrupts_pending(void)
{
    return lapic_irqs_pending();
}

void handle_pending_interrupts(void)
{
    lapic_handle_pending();
}

/* ==================== 向后兼容 ==================== */

void irq_handler(u64 *interrupt_frame)
{
    (void)interrupt_frame;
    hal_enable_interrupts();
}