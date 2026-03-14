/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件抽象层 (HAL) 实现
 * 
 * 本文件提供架构无关的HAL接口实现
 * 通过编译时宏检测架构并调用对应的架构特定函数
 */

#include "hal.h"
#include "hardware_probe.h"
#include "lib/console.h"
#include "lib/mem.h"
#include <stddef.h>

/* ==================== 架构检测 ==================== */

static hal_arch_type_t g_current_arch = HAL_ARCH_UNKNOWN;

/**
 * 检测当前架构
 */
static hal_arch_type_t detect_arch(void)
{
#if defined(__x86_64__)
    return HAL_ARCH_X86_64;
#elif defined(__aarch64__)
    return HAL_ARCH_ARM64;
#elif defined(__riscv) && (__riscv_xlen == 64)
    return HAL_ARCH_RISCV64;
#else
    return HAL_ARCH_UNKNOWN;
#endif
}

/**
 * 获取当前架构类型
 */
hal_arch_type_t hal_get_arch_type(void)
{
    if (g_current_arch == HAL_ARCH_UNKNOWN) {
        g_current_arch = detect_arch();
    }
    return g_current_arch;
}

/**
 * 获取架构名称
 */
const char* hal_get_arch_name(void)
{
    switch (hal_get_arch_type()) {
        case HAL_ARCH_X86_64:   return "x86_64";
        case HAL_ARCH_ARM64:    return "ARM64";
        case HAL_ARCH_RISCV64:  return "RISC-V64";
        default:                return "Unknown";
    }
}

/* ==================== 架构特定函数声明 ==================== */

/* x86_64架构函数 */
#if defined(__x86_64__)
extern void x86_64_save_context(void *ctx);
extern void x86_64_restore_context(void *ctx);
extern void x86_64_halt(void);
extern void x86_64_idle(void);
extern u64 x86_64_get_timestamp(void);
extern void x86_64_memory_barrier(void);
extern void x86_64_read_barrier(void);
extern void x86_64_write_barrier(void);
extern bool x86_64_disable_interrupts(void);
extern void x86_64_enable_interrupts(void);
extern void x86_64_restore_interrupts(bool state);
extern u32 x86_64_get_privilege_level(void);
extern u8 x86_64_inb(u16 port);
extern void x86_64_outb(u16 port, u8 value);
extern u16 x86_64_inw(u16 port);
extern void x86_64_outw(u16 port, u16 value);
extern u32 x86_64_inl(u16 port);
extern void x86_64_outl(u16 port, u32 value);
extern void x86_64_breakpoint(void);
#endif

/* ARM64架构函数 */
#if defined(__aarch64__)
extern void arm64_save_context(void *ctx);
extern void arm64_restore_context(void *ctx);
extern void arm64_halt(void);
extern void arm64_idle(void);
extern u64 arm64_get_timestamp(void);
extern void arm64_memory_barrier(void);
extern void arm64_read_barrier(void);
extern void arm64_write_barrier(void);
extern bool arm64_disable_interrupts(void);
extern void arm64_enable_interrupts(void);
extern void arm64_restore_interrupts(bool state);
extern u32 arm64_get_privilege_level(void);
extern void arm64_breakpoint(void);
#endif

/* RISC-V架构函数 */
#if defined(__riscv)
extern void riscv64_save_context(void *ctx);
extern void riscv64_restore_context(void *ctx);
extern void riscv64_halt(void);
extern void riscv64_idle(void);
extern u64 riscv64_get_timestamp(void);
extern void riscv64_memory_barrier(void);
extern void riscv64_read_barrier(void);
extern void riscv64_write_barrier(void);
extern bool riscv64_disable_interrupts(void);
extern void riscv64_enable_interrupts(void);
extern void riscv64_restore_interrupts(bool state);
extern u32 riscv64_get_privilege_level(void);
extern void riscv64_breakpoint(void);
#endif

/* ==================== HAL初始化 ==================== */

/**
 * 初始化HAL层
 */
void hal_init(void)
{
    g_current_arch = detect_arch();
    /* 架构特定的初始化在引导层完成 */
}

/**
 * 获取页大小
 */
u64 hal_get_page_size(void)
{
    return HAL_PAGE_SIZE;
}

/**
 * 检查当前架构是否支持IO端口
 */
bool hal_supports_io_ports(void)
{
    return hal_get_arch_type() == HAL_ARCH_X86_64;
}

/* ==================== 内存屏障接口 ==================== */

void hal_memory_barrier(void)
{
#if defined(__x86_64__)
    x86_64_memory_barrier();
#elif defined(__aarch64__)
    arm64_memory_barrier();
#elif defined(__riscv)
    riscv64_memory_barrier();
#else
    __asm__ volatile("" ::: "memory");
#endif
}

void hal_read_barrier(void)
{
#if defined(__x86_64__)
    x86_64_read_barrier();
#elif defined(__aarch64__)
    arm64_read_barrier();
#elif defined(__riscv)
    riscv64_read_barrier();
#else
    __asm__ volatile("" ::: "memory");
#endif
}

void hal_write_barrier(void)
{
#if defined(__x86_64__)
    x86_64_write_barrier();
#elif defined(__aarch64__)
    arm64_write_barrier();
#elif defined(__riscv)
    riscv64_write_barrier();
#else
    __asm__ volatile("" ::: "memory");
#endif
}

/* ==================== 中断控制接口 ==================== */

bool hal_disable_interrupts(void)
{
#if defined(__x86_64__)
    return x86_64_disable_interrupts();
#elif defined(__aarch64__)
    return arm64_disable_interrupts();
#elif defined(__riscv)
    return riscv64_disable_interrupts();
#else
    return false;
#endif
}

void hal_enable_interrupts(void)
{
#if defined(__x86_64__)
    x86_64_enable_interrupts();
#elif defined(__aarch64__)
    arm64_enable_interrupts();
#elif defined(__riscv)
    riscv64_enable_interrupts();
#endif
}

void hal_restore_interrupts(bool state)
{
#if defined(__x86_64__)
    x86_64_restore_interrupts(state);
#elif defined(__aarch64__)
    arm64_restore_interrupts(state);
#elif defined(__riscv)
    riscv64_restore_interrupts(state);
#endif
}

/* ==================== 时间接口 ==================== */

u64 hal_get_timestamp(void)
{
#if defined(__x86_64__)
    return x86_64_get_timestamp();
#elif defined(__aarch64__)
    return arm64_get_timestamp();
#elif defined(__riscv)
    return riscv64_get_timestamp();
#else
    return 0;
#endif
}

void hal_udelay(u32 us)
{
    u64 start, end;
    u64 elapsed = 0;
    u64 target = (u64)us * 1000;  /* 假设1GHz时钟 */
    
    start = hal_get_timestamp();
    
    do {
        end = hal_get_timestamp();
        elapsed = end - start;
        hal_idle();
    } while (elapsed < target);
}

/* ==================== 特权级接口 ==================== */

bool hal_is_kernel_mode(void)
{
    return hal_get_privilege_level() == 0;
}

u32 hal_get_privilege_level(void)
{
#if defined(__x86_64__)
    return x86_64_get_privilege_level();
#elif defined(__aarch64__)
    return arm64_get_privilege_level();
#elif defined(__riscv)
    return riscv64_get_privilege_level();
#else
    return 0;
#endif
}

/* ==================== 内存接口 ==================== */

void* hal_phys_to_virt(phys_addr_t phys)
{
    /* 恒等映射 */
    return (void*)phys;
}

phys_addr_t hal_virt_to_phys(void* virt)
{
    /* 恒等映射 */
    return (phys_addr_t)virt;
}

/* ==================== 上下文接口 ==================== */

void hal_save_context(void *context)
{
#if defined(__x86_64__)
    x86_64_save_context(context);
#elif defined(__aarch64__)
    arm64_save_context(context);
#elif defined(__riscv)
    riscv64_save_context(context);
#endif
}

void hal_restore_context(void *context)
{
#if defined(__x86_64__)
    x86_64_restore_context(context);
#elif defined(__aarch64__)
    arm64_restore_context(context);
#elif defined(__riscv)
    riscv64_restore_context(context);
#endif
}

void hal_context_switch(void *prev, void *next)
{
    /* 上下文切换实现由架构特定代码提供 */
    hal_save_context(prev);
    hal_restore_context(next);
}

void hal_context_init(void *context, void *entry_point, void *stack_top)
{
    /* 上下文初始化：设置初始状态 */
    if (!context) return;

    /* 清零上下文 */
    memzero(context, sizeof(hal_context_t));

    /* 设置栈指针 */
    hal_context_t *ctx = (hal_context_t*)context;
    ctx->sp = (u64)stack_top;

    /* 设置程序计数器（入口点） */
    ctx->pc = (u64)entry_point;

    /* 设置标志寄存器（启用中断） */
#if defined(__x86_64__)
    ctx->flags = 0x202;  /* IF = 1 (启用中断) */
#elif defined(__aarch64__)
    ctx->flags = 0x0;    /* ARM64标志 */
#elif defined(__riscv)
    ctx->flags = 0x0;    /* RISC-V标志 */
#endif
}

/* ==================== 系统调用接口 ==================== */

void hal_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    /* 系统调用实现：通过软件中断或专用指令 */
#if defined(__x86_64__)
    /* x86_64: 使用INT 0x80或SYSCALL指令 */
    /* 参数传递：RAX=系统调用号, RDI=arg1, RSI=arg2, RDX=arg3, RCX=arg4 */
    __asm__ volatile(
        "mov %0, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "mov %4, %%rcx\n"
        "syscall\n"
        :
        : "r"(syscall_num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    /* ARM64: 使用SVC指令 */
    /* 参数传递：X8=系统调用号, X0=arg1, X1=arg2, X2=arg3, X3=arg4 */
    __asm__ volatile(
        "mov x8, %0\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, %4\n"
        "svc #0\n"
        :
        : "r"(syscall_num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
        : "x0", "x1", "x2", "x3", "x8", "memory"
    );
#elif defined(__riscv)
    /* RISC-V: 使用ECALL指令 */
    /* 参数传递：a7=系统调用号, a0=arg1, a1=arg2, a2=arg3, a3=arg4 */
    __asm__ volatile(
        "mv a7, %0\n"
        "mv a0, %1\n"
        "mv a1, %2\n"
        "mv a2, %3\n"
        "mv a3, %4\n"
        "ecall\n"
        :
        : "r"(syscall_num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
        : "a0", "a1", "a2", "a3", "a7", "memory"
    );
#endif
}

void hal_syscall_return(u64 ret_val)
{
    /* 系统调用返回：设置返回值并返回 */
#if defined(__x86_64__)
    __asm__ volatile(
        "mov %0, %%rax\n"
        "sysretq\n"
        :
        : "r"(ret_val)
        : "rax", "rcx", "r11", "memory"
    );
#elif defined(__aarch64__)
    __asm__ volatile(
        "mov x0, %0\n"
        "eret\n"
        :
        : "r"(ret_val)
        : "x0", "memory"
    );
#elif defined(__riscv)
    __asm__ volatile(
        "mv a0, %0\n"
        "sret\n"
        :
        : "r"(ret_val)
        : "a0", "memory"
    );
#endif
}

/* ==================== 设备接口 ==================== */

u8 hal_inb(u16 port)
{
#if defined(__x86_64__)
    return x86_64_inb(port);
#else
    (void)port;
    return 0xFF;
#endif
}

void hal_outb(u16 port, u8 value)
{
#if defined(__x86_64__)
    x86_64_outb(port, value);
#else
    (void)port;
    (void)value;
#endif
}

u16 hal_inw(u16 port)
{
#if defined(__x86_64__)
    return x86_64_inw(port);
#else
    (void)port;
    return 0xFFFF;
#endif
}

void hal_outw(u16 port, u16 value)
{
#if defined(__x86_64__)
    x86_64_outw(port, value);
#else
    (void)port;
    (void)value;
#endif
}

u32 hal_inl(u16 port)
{
#if defined(__x86_64__)
    return x86_64_inl(port);
#else
    (void)port;
    return 0xFFFFFFFF;
#endif
}

void hal_outl(u16 port, u32 value)
{
#if defined(__x86_64__)
    x86_64_outl(port, value);
#else
    (void)port;
    (void)value;
#endif
}

/* ==================== 错误处理接口 ==================== */

void hal_trigger_exception(u32 exc_num)
{
    /* 异常触发：通过软件中断或异常指令 */
#if defined(__x86_64__)
    /* x86_64: INT指令需要立即数，使用UD2触发未定义指令异常 */
    /* 对于特定的异常号，需要使用IDT门或中断控制器 */
    (void)exc_num;
    __asm__ volatile("ud2");  /* 触发未定义指令异常 */
#elif defined(__aarch64__)
    /* ARM64: 使用BRK指令触发异常，编码异常号 */
    __asm__ volatile("brk %0" : : "I"((u16)exc_num));
#elif defined(__riscv)
    /* RISC-V: 使用EBREAK触发断点，异常号需要通过其他方式传递 */
    (void)exc_num;
    __asm__ volatile("ebreak");
#endif
}

void hal_halt(void)
{
#if defined(__x86_64__)
    x86_64_halt();
#elif defined(__aarch64__)
    arm64_halt();
#elif defined(__riscv)
    riscv64_halt();
#endif
}

void hal_idle(void)
{
#if defined(__x86_64__)
    x86_64_idle();
#elif defined(__aarch64__)
    arm64_idle();
#elif defined(__riscv)
    riscv64_idle();
#endif
}

/* ==================== 调试接口 ==================== */

void hal_breakpoint(void)
{
#if defined(__x86_64__)
    x86_64_breakpoint();
#elif defined(__aarch64__)
    arm64_breakpoint();
#elif defined(__riscv)
    riscv64_breakpoint();
#endif
}

void hal_stack_trace(void)
{
    /* 栈回溯实现：遍历调用栈并打印地址 */
    console_puts("[HAL] Stack trace:\n");

#if defined(__x86_64__)
    /* x86_64: 通过RBP寄存器遍历栈帧 */
    u64 *rbp;
    u64 *ret_addr;

    /* 获取当前RBP */
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

    /* 遍历栈帧 */
    for (int i = 0; i < 16 && rbp != NULL; i++) {
        /* 获取返回地址 */
        ret_addr = (u64*)(rbp + 1);

        /* 打印返回地址 */
        console_puts("  [");
        console_puthex64((u64)*ret_addr);
        console_puts("]\n");

        /* 移动到下一个栈帧 */
        rbp = (u64*)*rbp;

        /* 检查栈帧是否有效 */
        if ((u64)rbp < 0x1000 || (u64)rbp > 0xFFFFFFFFFFFF) {
            break;
        }
    }
#elif defined(__aarch64__)
    /* ARM64: 通过FP寄存器（x29）遍历栈帧 */
    u64 *fp;
    u64 *ret_addr;

    /* 获取当前FP */
    __asm__ volatile("mov %0, x29" : "=r"(fp));

    /* 遍历栈帧 */
    for (int i = 0; i < 16 && fp != NULL; i++) {
        /* 获取返回地址（FP指向的前一个帧的返回地址） */
        ret_addr = (u64*)(fp + 1);

        /* 打印返回地址 */
        console_puts("  [");
        console_puthex64((u64)*ret_addr);
        console_puts("]\n");

        /* 移动到下一个栈帧 */
        fp = (u64*)*fp;

        /* 检查栈帧是否有效 */
        if ((u64)fp < 0x1000 || (u64)fp > 0xFFFFFFFFFFFF) {
            break;
        }
    }
#elif defined(__riscv)
    /* RISC-V: 通过FP寄存器（x8/s0）遍历栈帧 */
    u64 *fp;
    u64 *ret_addr;

    /* 获取当前FP */
    __asm__ volatile("mv %0, x8" : "=r"(fp));

    /* 遍历栈帧 */
    for (int i = 0; i < 16 && fp != NULL; i++) {
        /* 获取返回地址 */
        ret_addr = (u64*)(fp + 1);

        /* 打印返回地址 */
        console_puts("  [");
        console_puthex64((u64)*ret_addr);
        console_puts("]\n");

        /* 移动到下一个栈帧 */
        fp = (u64*)*fp;

        /* 检查栈帧是否有效 */
        if ((u64)fp < 0x1000 || (u64)fp > 0xFFFFFFFFFFFF) {
            break;
        }
    }
#endif

    console_puts("[HAL] End of stack trace\n");
}

/* ==================== 架构特定GDT加载 ==================== */

/* 外部函数声明（由架构特定代码提供） */
#if defined(__x86_64__)
extern void gdt_load(void *gdt_ptr);
extern cpu_id_t x86_64_get_cpu_id(void);
#endif

/* ==================== 多核支持接口 ==================== */

/**
 * 获取当前CPU ID
 */
cpu_id_t hal_get_cpu_id(void)
{
#if defined(__x86_64__)
    return x86_64_get_cpu_id();
#elif defined(__aarch64__)
    /* ARM64: 读取MPIDR_EL1获取CPU ID */
    u64 mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (cpu_id_t)(mpidr & 0xFF);  /* 取Aff0字段 */
#elif defined(__riscv)
    /* RISC-V: 读取mhartid获取硬件线程ID */
    u64 hartid;
    __asm__ volatile("csrr %0, mhartid" : "=r"(hartid));
    return (cpu_id_t)hartid;
#else
    return 0; /* 未知架构返回默认值 */
#endif
}

/**
 * 获取CPU数量
 */
u32 hal_get_cpu_count(void)
{
    extern cpu_info_t g_cpu_info;
    return g_cpu_info.logical_cores;
}

/**
 * 检查当前CPU是否是BSP
 */
bool hal_is_bsp(void)
{
    cpu_id_t cpu_id = hal_get_cpu_id();
    /* 假设BSP的CPU ID为0 */
    return (cpu_id == 0);
}

/* ==================== UART 串口接口实现 ==================== */

/*
 * 架构分离设计：
 * - 通用接口在 hal.h 中声明
 * - 架构特定实现在 arch/<arch>/hal_uart.c 中
 * - hal.c 只做函数调用转发，不包含架构特定常量
 */

/* 架构特定 UART 函数声明（在 arch/<arch>/hal_uart.c 中实现） */
extern void arch_uart_init(phys_addr_t base, u32 baud);
extern void arch_uart_putc(phys_addr_t base, char c);
extern char arch_uart_getc(phys_addr_t base);
extern bool arch_uart_rx_ready(phys_addr_t base);
extern bool arch_uart_tx_ready(phys_addr_t base);
extern phys_addr_t arch_uart_get_default_base(void);

/* 默认 UART 配置 */
static hal_uart_config_t g_hal_uart_config = {
    .base_addr = 0,
    .baud_rate = 115200,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
};

/**
 * 获取默认 UART 基地址
 */
phys_addr_t hal_uart_get_default_base(void)
{
    return arch_uart_get_default_base();
}

/**
 * 初始化 UART
 */
void hal_uart_init(const hal_uart_config_t *config)
{
    if (config) {
        g_hal_uart_config = *config;
    } else {
        g_hal_uart_config.base_addr = arch_uart_get_default_base();
        g_hal_uart_config.baud_rate = 115200;
    }
    
    arch_uart_init(g_hal_uart_config.base_addr, g_hal_uart_config.baud_rate);
}

/**
 * 发送单个字符
 */
void hal_uart_putc(char c)
{
    arch_uart_putc(g_hal_uart_config.base_addr, c);
}

/**
 * 发送字符串
 */
void hal_uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            hal_uart_putc('\r');
        }
        hal_uart_putc(*str);
        str++;
    }
}

/**
 * 接收单个字符（阻塞）
 */
char hal_uart_getc(void)
{
    return arch_uart_getc(g_hal_uart_config.base_addr);
}

/**
 * 检查是否有数据可读
 */
bool hal_uart_rx_ready(void)
{
    return arch_uart_rx_ready(g_hal_uart_config.base_addr);
}

/**
 * 检查是否可以发送
 */
bool hal_uart_tx_ready(void)
{
    return arch_uart_tx_ready(g_hal_uart_config.base_addr);
}
