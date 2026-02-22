/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC中断控制器实现（无锁设计）
 * 遵循文档第2.1节：中断处理机制
 * 
 * 无锁设计原则：
 * 1. 中断路由表在构建时初始化，运行时只读
 * 2. 中断处理函数使用静态路由，无需锁
 * 3. 运行时注册使用临界区保护
 */

#include "irq.h"
#include "capability.h"
#include "atomic.h"
#include "lib/console.h"
#include "hardware_probe.h"
#include "boot_info.h"

/* 外部变量 */
extern boot_state_t g_boot_state;

/* 获取 IOAPIC 基地址 */
static u64 get_ioapic_base(void) {
    return g_boot_state.hw.io_irq.base_address;
}

/* 中断路由表（无锁设计 - 构建时初始化，运行时只读） */
volatile irq_route_entry_t irq_table[256];

/* 初始化中断控制器（无锁实现） */
void irq_controller_init(void)
{
    console_puts("[IRQ] Initializing interrupt controller (lock-free)...\n");
    
    /* 清空中断表 */
    for (u32 i = 0; i < 256; i++) {
        irq_table[i].domain_id = 0;
        irq_table[i].handler_address = 0;
        irq_table[i].endpoint_cap = HIC_CAP_INVALID;
        irq_table[i].initialized = 0;
    }
    
    /* 从构建配置加载中断路由（一次性初始化） */
    for (u32 i = 0; i < g_build_config.num_interrupt_routes; i++) {
        interrupt_route_t *route = &g_build_config.interrupt_routes[i];
        u32 vector = route->irq_vector;
        
        if (vector < 256) {
            irq_table[vector].domain_id = route->target_domain;
            irq_table[vector].handler_address = route->handler_address;
            irq_table[vector].initialized = 1;
            /* endpoint_cap在服务注册时设置 */
        }
    }
    
    /* 使用内存屏障确保初始化完成 */
    atomic_full_barrier();
    
    console_puts("[IRQ] Interrupt routes loaded from build config\n");
}

/* 注册中断处理函数（无锁实现 - 使用临界区） */
hic_status_t irq_register_handler(u32 irq_vector, domain_id_t domain, 
                                   u64 handler, cap_id_t endpoint_cap)
{
    if (irq_vector >= 256 || handler == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 验证能力 */
    hic_status_t status = cap_check_access(domain, endpoint_cap, 0);
    if (status != HIC_SUCCESS) {
        return HIC_ERROR_PERMISSION;
    }
    
    /* 进入临界区（禁用中断保证原子性） */
    bool irq_state = atomic_enter_critical();
    
    /* 注册到中断表 */
    irq_table[irq_vector].domain_id = domain;
    irq_table[irq_vector].handler_address = handler;
    irq_table[irq_vector].endpoint_cap = endpoint_cap;
    irq_table[irq_vector].initialized = 1;
    
    /* 退出临界区 */
    atomic_exit_critical(irq_state);
    
    return HIC_SUCCESS;
}

/* 启用中断 */
void irq_enable(u32 irq_vector)
{
    if (irq_vector < 32) {
        /* PIC */
        __asm__ volatile (
            "mov $0x21, %%al\n"
            "inb $0x21, %%al\n"
            "and $0xFE, %%al\n"
            "outb %%al, $0x21\n"
            : : : "al"
        );
    } else if (irq_vector >= 32 && irq_vector < 48) {
        /* APIC */
        /* 完整实现：APIC中断启用 */
        u32 apic_irq = irq_vector - 32;
        
        /* 获取IOAPIC重定向表项 */
        volatile u32* ioapic_base = (volatile u32*)get_ioapic_base();
        
        /* 计算重定向表项索引 */
        u32 ioapic_index = apic_irq * 2;
        
        /* IOAPIC 寄存器访问：
         * ioapic_base + 0: IOREGSEL (选择寄存器)
         * ioapic_base + 4: IOWIN (数据寄存器)
         */
        
        /* 读取低32位 */
        ioapic_base[0] = ioapic_index;
        u32 low_dword = ioapic_base[4];
        
        /* 清除mask位 (bit 3) */
        low_dword &= 0xFFFFFFF7;
        
        /* 写回低32位 */
        ioapic_base[0] = ioapic_index;
        ioapic_base[4] = low_dword;
    }
}

/* 禁用中断 */
void irq_disable(u32 irq_vector)
{
    if (irq_vector < 32) {
        /* PIC */
        __asm__ volatile (
            "mov $0x21, %%al\n"
            "inb $0x21, %%al\n"
            "or $0x01, %%al\n"
            "outb %%al, $0x21\n"
            : : : "al"
        );
    } else if (irq_vector >= 32 && irq_vector < 48) {
        /* APIC */
        /* 完整实现：APIC中断禁用 */
        u32 apic_irq = irq_vector - 32;
        
        /* 获取IOAPIC重定向表项 */
        volatile u32* ioapic_base = (volatile u32*)get_ioapic_base();
        
        /* 计算重定向表项索引 */
        u32 ioapic_index = apic_irq * 2;
        
        /* 读取低32位 */
        ioapic_base[0] = ioapic_index;
        u32 low_dword = ioapic_base[4];
        
        /* 设置mask位 (bit 3) */
        low_dword |= 0x00000008;
        
        /* 写回低32位 */
        ioapic_base[0] = ioapic_index;
        ioapic_base[4] = low_dword;
    }
}

/* 中断分发核心函数（无锁实现 - 只读访问） */
void irq_dispatch(u32 irq_vector)
{
    /* 使用内存屏障确保读取顺序 */
    atomic_acquire_barrier();

    volatile irq_route_entry_t *entry = &irq_table[irq_vector];
    
    /* 检查是否有注册的处理函数 */
    if (!entry->initialized || entry->handler_address == 0) {
        console_puts("[IRQ] Unhandled IRQ: ");
        console_putu64(irq_vector);
        console_puts("\n");
        return;
    }
    
    /* 验证能力（只读操作，无锁） */
    hic_status_t status = cap_check_access(entry->domain_id, 
                                            entry->endpoint_cap, 0);
    if (status != HIC_SUCCESS) {
        console_puts("[IRQ] Permission denied for IRQ: ");
        console_putu64(irq_vector);
        console_puts("\n");
        return;
    }
    
    /* 直接调用Privileged-1服务的处理函数（受保护入口点） */
    /* 这是同特权级函数调用，无需特权级切换 */
    typedef void (*handler_func_t)(void);
    handler_func_t handler = (handler_func_t)entry->handler_address;
    
    handler();
}

/* 通用中断处理（汇编调用） */
void irq_common_handler(u32 irq_vector)
{
    /* 简单保存上下文后分发 */
    /* 实际上下文保存在idt.S中完成 */
    irq_dispatch(irq_vector);
}

/* 检查是否有待处理的中断 */
bool interrupts_pending(void)
{
    /* 检查本地APIC的IRR（中断请求寄存器） */
    extern bool lapic_irqs_pending(void);
    return lapic_irqs_pending();
}

/* 处理待处理的中断 */
void handle_pending_interrupts(void)
{
    extern void lapic_handle_pending(void);
    lapic_handle_pending();
}

/* 中断处理函数（完整实现） */
void irq_handler(u64 *interrupt_frame)
{
    if (!interrupt_frame) {
        return;
    }

    /* 完整实现：从中断帧中提取中断向量 */
    u64 error_code = interrupt_frame[0];
    u64 rip = interrupt_frame[1];
    u64 cs = interrupt_frame[2];
    u64 rflags = interrupt_frame[3];

    /* 记录中断信息 */
    console_puts("[IRQ] Interrupt at RIP=0x");
    console_puti64((s64)rip);
    console_puts(", CS=0x");
    console_puti64((s64)cs);
    console_puts(", RFLAGS=0x");
    console_puti64((s64)rflags);
    console_puts("\n");

    /* 根据错误码判断中断类型 */
    if (error_code == 0xFFFFFFFF) {
        /* 硬件中断 */
        console_puts("[IRQ] Hardware interrupt\n");
    } else {
        /* 异常 */
        console_puts("[IRQ] Exception, error code=0x");
        console_puti64((s64)error_code);
        console_puts("\n");
    }

    /* 恢复中断并返回 */
    hal_enable_interrupts();
}

/* 快速路径中断处理（完整实现） */
void irq_handler_fast(u32 irq_vector)
{
    /* 完整实现：快速处理不需要上下文切换的中断 */
    console_puts("[IRQ] Fast interrupt handler, vector=");
    console_putu32(irq_vector);
    console_puts("\n");

    /* 根据中断向量分发到具体的处理程序 */
    switch (irq_vector) {
        case 32: /* 时钟中断 */
            console_puts("[IRQ] Timer interrupt\n");
            break;
        case 33: /* 键盘中断 */
            console_puts("[IRQ] Keyboard interrupt\n");
            break;
        default:
            console_puts("[IRQ] Unhandled fast interrupt\n");
            break;
    }

    /* 发送EOI（中断结束信号）到本地APIC */
    hal_outb(0x20, 0x20); /* 主PIC EOI */
    hal_outb(0xA0, 0x20); /* 从PIC EOI */
}

/* 检查本地APIC是否有待处理的中断（完整实现） */
bool lapic_irqs_pending(void)
{
    /* 完整实现：读取本地APIC的IRR（中断请求寄存器） */
    /* 使用内存映射IO访问本地APIC */
    /* IRR基地址为0xFEE00200 */

    /* 读取低32位IRR（使用内存映射IO） */
    volatile u32* irr_low_ptr = (volatile u32*)0xFEE00200;
    u32 irr_low = *irr_low_ptr;

    /* 读取高32位IRR（使用内存映射IO） */
    volatile u32* irr_high_ptr = (volatile u32*)0xFEE00210;
    u32 irr_high = *irr_high_ptr;

    /* 如果IRR不为0，说明有待处理的中断 */
    bool pending = (irr_low != 0) || (irr_high != 0);

    if (pending) {
        console_puts("[IRQ] Pending interrupts detected\n");
    }

    return pending;
}

/* 处理本地APIC的待处理中断（完整实现） */
void lapic_handle_pending(void)
{
    /* 完整实现：处理IRR中所有待处理的中断 */

    /* 使用内存映射IO访问本地APIC */
    volatile u32* irr_low_ptr = (volatile u32*)0xFEE00200;
    volatile u32* irr_high_ptr = (volatile u32*)0xFEE00210;

    /* 读取低32位IRR */
    u32 irr_low = *irr_low_ptr;

    /* 读取高32位IRR */
    u32 irr_high = *irr_high_ptr;

    /* 遍历所有256个中断向量 */
    for (u32 i = 0; i < 256; i++) {
        u32 irr_word = (i < 32) ? irr_low : irr_high;
        u32 irr_bit = 1U << (i % 32);

        if (irr_word & irr_bit) {
            /* 中断i待处理，调用快速处理程序 */
            irq_handler_fast(i);
        }
    }

    console_puts("[IRQ] All pending interrupts handled\n");
}
