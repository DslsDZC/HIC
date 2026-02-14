/**
 * HIK中断控制器实现
 * 遵循文档第2.1节：中断处理机制
 */

#include "irq.h"
#include "capability.h"
#include "lib/console.h"

/* 中断路由表（运行时版本） */
static protected_entry_t irq_table[256];

/* 初始化中断控制器 */
void irq_controller_init(void)
{
    console_puts("[IRQ] Initializing interrupt controller...\n");
    
    /* 清空中断表 */
    for (u32 i = 0; i < 256; i++) {
        irq_table[i].domain_id = 0;
        irq_table[i].handler_address = 0;
        irq_table[i].endpoint_cap = HIK_CAP_INVALID;
    }
    
    /* 从构建配置加载中断路由 */
    for (u32 i = 0; i < g_build_config.num_interrupt_routes; i++) {
        interrupt_route_t *route = &g_build_config.interrupt_routes[i];
        u32 vector = route->irq_vector;
        
        if (vector < 256) {
            irq_table[vector].domain_id = route->target_domain;
            irq_table[vector].handler_address = route->handler_address;
            /* endpoint_cap在服务注册时设置 */
        }
    }
    
    console_puts("[IRQ] Interrupt routes loaded from build config\n");
}

/* 注册中断处理函数 */
hik_status_t irq_register_handler(u32 irq_vector, domain_id_t domain, 
                                   u64 handler, cap_id_t endpoint_cap)
{
    if (irq_vector >= 256 || handler == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 验证能力 */
    hik_status_t status = cap_check_access(domain, endpoint_cap, 0);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_PERMISSION;
    }
    
    /* 注册到中断表 */
    irq_table[irq_vector].domain_id = domain;
    irq_table[irq_vector].handler_address = handler;
    irq_table[irq_vector].endpoint_cap = endpoint_cap;
    
    return HIK_SUCCESS;
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
    }
    } else if (irq_vector >= 32 && irq_vector < 48) {
        /* APIC */
        /* 完整实现：APIC中断启用 */
        u32 apic_irq = irq_vector - 32;
        
        /* 获取IOAPIC重定向表项 */
        volatile u32* ioapic_base = (volatile u32*)get_ioapic_base();
        
        /* 计算重定向表项索引 */
        u32 ioapic_index = apic_irq * 2;
        
        /* 读取低32位 */
        __asm__ volatile (
            "movl %0, (%%edi)\n"
            "movl (%%esi), %%eax\n"
            :
            : "a"(ioapic_index), "D"(ioapic_base), "S"(ioapic_base + 4)
            : "eax"
        );
        
        /* 清除mask位 */
        u32 low_dword = 0;
        __asm__ volatile (
            "movl (%%esi), %%eax\n"
            "andl $0xFFFFFFF7, %%eax\n"  /* 清除bit 3 (mask) */
            "movl %%eax, (%%edi)\n"
            :
            : "D"(ioapic_index), "S"(ioapic_base + 4)
            : "eax"
        );
        
        /* 写回 */
        __asm__ volatile (
            "movl %0, (%%edi)\n"
            :
            : "a"(low_dword), "D"(ioapic_base)
        );
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
        u32 low_dword = 0;
        __asm__ volatile (
            "movl %0, (%%edi)\n"
            "movl (%%esi), %%eax\n"
            "movl %%eax, %1\n"
            :
            : "a"(ioapic_index), "m"(low_dword), "D"(ioapic_base), "S"(ioapic_base + 4)
            : "eax"
        );
        
        /* 设置mask位 */
        low_dword |= 0x00000008;  /* 设置bit 3 (mask) */
        
        /* 写回 */
        __asm__ volatile (
            "movl %0, (%%edi)\n"
            :
            : "a"(low_dword), "D"(ioapic_base)
        );
    }
}

/* 中断分发核心函数 */
void irq_dispatch(u32 irq_vector)
{
    protected_entry_t *entry = &irq_table[irq_vector];
    
    /* 检查是否有注册的处理函数 */
    if (entry->handler_address == 0) {
        console_puts("[IRQ] Unhandled IRQ: ");
        console_putu64(irq_vector);
        console_puts("\n");
        return;
    }
    
    /* 验证能力 */
    hik_status_t status = cap_check_access(entry->domain_id, 
                                            entry->endpoint_cap, 0);
    if (status != HIK_SUCCESS) {
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
