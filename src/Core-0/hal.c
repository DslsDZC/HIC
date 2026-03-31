/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件抽象层 (HAL) 实现
 * 
 * 本文件提供架构无关的HAL接口实现。
 * 所有架构特定操作通过函数指针表调用，
 * 架构特定代码在 arch/<arch>/hal_impl.c 中实现。
 */

#include "include/hal.h"
#include "hardware_probe.h"
#include "lib/console.h"
#include "lib/mem.h"
#include <stddef.h>

/* ==================== 全局状态 ==================== */

static hal_arch_type_t g_current_arch = HAL_ARCH_UNKNOWN;
static const hal_arch_ops_t *g_arch_ops = NULL;

/* 默认串口配置 */
static hal_uart_config_t g_uart_config = {
    .base_addr = 0x3F8,
    .baud_rate = 115200,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
};

/* ==================== 架构注册 ==================== */

/**
 * 注册架构操作表
 */
void hal_register_arch_ops(const hal_arch_ops_t *ops)
{
    if (ops == NULL) return;
    g_arch_ops = ops;
    
    /* 根据名称设置架构类型 */
    if (ops->arch_name) {
        if (ops->arch_name[0] == 'x' || ops->arch_name[0] == 'X') {
            g_current_arch = HAL_ARCH_X86_64;
        } else if (ops->arch_name[0] == 'A' || ops->arch_name[0] == 'a') {
            g_current_arch = HAL_ARCH_ARM64;
        } else if (ops->arch_name[0] == 'R' || ops->arch_name[0] == 'r') {
            g_current_arch = HAL_ARCH_RISCV64;
        }
    }
    
    console_puts("[HAL] Registered architecture: ");
    console_puts(ops->arch_name ? ops->arch_name : "unknown");
    console_puts("\n");
}

/* ==================== 架构查询 ==================== */

hal_arch_type_t hal_get_arch_type(void)
{
    return g_current_arch;
}

const char* hal_get_arch_name(void)
{
    if (g_arch_ops && g_arch_ops->arch_name) {
        return g_arch_ops->arch_name;
    }
    return "Unknown";
}

u64 hal_get_page_size(void)
{
    return HAL_PAGE_SIZE;
}

bool hal_supports_io_ports(void)
{
    return g_arch_ops ? g_arch_ops->supports_io_ports : false;
}

/* ==================== 内存屏障 ==================== */

void hal_memory_barrier(void)
{
    if (g_arch_ops && g_arch_ops->memory_barrier) {
        g_arch_ops->memory_barrier();
    }
}

void hal_read_barrier(void)
{
    if (g_arch_ops && g_arch_ops->read_barrier) {
        g_arch_ops->read_barrier();
    }
}

void hal_write_barrier(void)
{
    if (g_arch_ops && g_arch_ops->write_barrier) {
        g_arch_ops->write_barrier();
    }
}

/* ==================== 中断控制 ==================== */

bool hal_disable_interrupts(void)
{
    if (g_arch_ops && g_arch_ops->disable_interrupts) {
        return g_arch_ops->disable_interrupts();
    }
    return false;
}

void hal_enable_interrupts(void)
{
    if (g_arch_ops && g_arch_ops->enable_interrupts) {
        g_arch_ops->enable_interrupts();
    }
}

void hal_restore_interrupts(bool state)
{
    if (g_arch_ops && g_arch_ops->restore_interrupts) {
        g_arch_ops->restore_interrupts(state);
    }
}

/* ==================== 时间接口 ==================== */

u64 hal_get_timestamp(void)
{
    if (g_arch_ops && g_arch_ops->get_timestamp) {
        return g_arch_ops->get_timestamp();
    }
    return 0;
}

void hal_udelay(u32 us)
{
    if (g_arch_ops && g_arch_ops->udelay) {
        g_arch_ops->udelay(us);
    }
}

/* ==================== 特权级 ==================== */

bool hal_is_kernel_mode(void)
{
    return hal_get_privilege_level() == 0;
}

u32 hal_get_privilege_level(void)
{
    if (g_arch_ops && g_arch_ops->get_privilege_level) {
        return g_arch_ops->get_privilege_level();
    }
    return 0;
}

/* ==================== 内存接口 ==================== */

void* hal_phys_to_virt(phys_addr_t phys)
{
    return (void*)phys;
}

phys_addr_t hal_virt_to_phys(void* virt)
{
    return (phys_addr_t)virt;
}

/* ==================== 上下文接口 ==================== */

void hal_save_context(void *context)
{
    if (g_arch_ops && g_arch_ops->save_context) {
        g_arch_ops->save_context(context);
    }
}

void hal_restore_context(void *context)
{
    if (g_arch_ops && g_arch_ops->restore_context) {
        g_arch_ops->restore_context(context);
    }
}

void hal_context_switch(void *prev, void *next)
{
    if (g_arch_ops && g_arch_ops->context_switch) {
        g_arch_ops->context_switch(prev, next);
    }
}

void hal_context_init(void *context, void *entry_point, void *stack_top)
{
    if (!context) return;
    
    memzero(context, sizeof(hal_context_t));
    
    hal_context_t *ctx = (hal_context_t*)context;
    ctx->sp = (u64)stack_top;
    ctx->pc = (u64)entry_point;
    
    if (g_arch_ops && g_arch_ops->context_init_flags) {
        ctx->flags = g_arch_ops->context_init_flags();
    }
    
    if (g_arch_ops && g_arch_ops->context_init) {
        g_arch_ops->context_init(context, entry_point, stack_top);
    }
}

/* ==================== 系统调用接口 ==================== */

void hal_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    if (g_arch_ops && g_arch_ops->syscall_invoke) {
        g_arch_ops->syscall_invoke(syscall_num, arg1, arg2, arg3, arg4);
    }
}

void hal_syscall_return(u64 ret_val)
{
    if (g_arch_ops && g_arch_ops->syscall_return) {
        g_arch_ops->syscall_return(ret_val);
    }
}

/* ==================== 设备接口 ==================== */

u8 hal_inb(u16 port)
{
    if (g_arch_ops && g_arch_ops->inb) {
        return g_arch_ops->inb(port);
    }
    return 0xFF;
}

void hal_outb(u16 port, u8 value)
{
    if (g_arch_ops && g_arch_ops->outb) {
        g_arch_ops->outb(port, value);
    }
}

u16 hal_inw(u16 port)
{
    if (g_arch_ops && g_arch_ops->inw) {
        return g_arch_ops->inw(port);
    }
    return 0xFFFF;
}

void hal_outw(u16 port, u16 value)
{
    if (g_arch_ops && g_arch_ops->outw) {
        g_arch_ops->outw(port, value);
    }
}

u32 hal_inl(u16 port)
{
    if (g_arch_ops && g_arch_ops->inl) {
        return g_arch_ops->inl(port);
    }
    return 0xFFFFFFFF;
}

void hal_outl(u16 port, u32 value)
{
    if (g_arch_ops && g_arch_ops->outl) {
        g_arch_ops->outl(port, value);
    }
}

/* ==================== 错误处理接口 ==================== */

void hal_trigger_exception(u32 exc_num)
{
    if (g_arch_ops && g_arch_ops->trigger_exception) {
        g_arch_ops->trigger_exception(exc_num);
    }
}

void hal_halt(void)
{
    if (g_arch_ops && g_arch_ops->halt) {
        g_arch_ops->halt();
    }
}

void hal_idle(void)
{
    if (g_arch_ops && g_arch_ops->idle) {
        g_arch_ops->idle();
    }
}

/* ==================== 调试接口 ==================== */

void hal_breakpoint(void)
{
    if (g_arch_ops && g_arch_ops->breakpoint) {
        g_arch_ops->breakpoint();
    }
}

void hal_stack_trace(void)
{
    if (g_arch_ops && g_arch_ops->stack_trace) {
        g_arch_ops->stack_trace();
    } else {
        console_puts("[HAL] Stack trace not implemented for this architecture\n");
    }
}

/* ==================== CPU 接口 ==================== */

cpu_id_t hal_get_cpu_id(void)
{
    if (g_arch_ops && g_arch_ops->get_cpu_id) {
        return g_arch_ops->get_cpu_id();
    }
    return 0;
}

/* ==================== UART 串口接口 ==================== */

extern void arch_uart_init(phys_addr_t base, u32 baud);
extern void arch_uart_putc(phys_addr_t base, char c);
extern char arch_uart_getc(phys_addr_t base);
extern bool arch_uart_rx_ready(phys_addr_t base);
extern bool arch_uart_tx_ready(phys_addr_t base);
extern phys_addr_t arch_uart_get_default_base(void);

phys_addr_t hal_uart_get_default_base(void)
{
    return arch_uart_get_default_base();
}

void hal_uart_init(const hal_uart_config_t *config)
{
    if (config) {
        g_uart_config = *config;
    } else {
        g_uart_config.base_addr = arch_uart_get_default_base();
        g_uart_config.baud_rate = 115200;
    }
    arch_uart_init(g_uart_config.base_addr, g_uart_config.baud_rate);
}

void hal_uart_putc(char c)
{
    arch_uart_putc(g_uart_config.base_addr, c);
}

void hal_uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            hal_uart_putc('\r');
        }
        hal_uart_putc(*str++);
    }
}

char hal_uart_getc(void)
{
    return arch_uart_getc(g_uart_config.base_addr);
}

bool hal_uart_rx_ready(void)
{
    return arch_uart_rx_ready(g_uart_config.base_addr);
}

bool hal_uart_tx_ready(void)
{
    return arch_uart_tx_ready(g_uart_config.base_addr);
}

/* ==================== 初始化 ==================== */

void hal_init(void)
{
    console_puts("[HAL] Initializing...\n");
    
    /* 架构特定初始化由 arch/<arch>/hal_impl.c 调用 hal_register_arch_ops */
    if (g_arch_ops == NULL) {
        console_puts("[HAL] Warning: No architecture operations registered!\n");
    }
    
    console_puts("[HAL] Architecture: ");
    console_puts(hal_get_arch_name());
    console_puts("\n");
}