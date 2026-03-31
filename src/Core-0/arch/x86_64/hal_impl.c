/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC x86_64架构特定HAL实现
 * 
 * 本文件提供x86_64架构的HAL接口实现
 * 架构无关代码通过hal.c调用这些函数
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal.h"

/* ==================== 上下文保存和恢复 ==================== */

/**
 * 保存x86_64上下文
 * 这个函数由汇编代码context.S提供
 */
void x86_64_save_context(void *ctx);

/**
 * 恢复x86_64上下文
 * 这个函数由汇编代码context.S提供
 */
void x86_64_restore_context(void *ctx);

/* ==================== CPU控制 ==================== */

/**
 * 停止CPU（HLT指令）
 */
void x86_64_halt(void)
{
    __asm__ volatile("hlt");
}

/**
 * 空转等待（PAUSE指令）
 */
void x86_64_idle(void)
{
    __asm__ volatile("pause");
}

/* ==================== 时间戳 ==================== */

/**
 * 获取时间戳计数器（RDTSC）
 */
uint64_t x86_64_get_timestamp(void)
{
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

/* ==================== 内存屏障 ==================== */

/**
 * 完整内存屏障（MFENCE）
 */
void x86_64_memory_barrier(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

/**
 * 读屏障（编译器屏障）
 */
void x86_64_read_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

/**
 * 写屏障（编译器屏障）
 */
void x86_64_write_barrier(void)
{
    __asm__ volatile("" ::: "memory");
}

/* ==================== 中断控制 ==================== */

/**
 * 禁用中断并返回之前的状态
 */
bool x86_64_disable_interrupts(void)
{
    uint64_t rflags;
    __asm__ volatile("pushf; pop %0" : "=r"(rflags));
    __asm__ volatile("cli");
    return (rflags & (1 << 9)) != 0;
}

/**
 * 启用中断
 */
void x86_64_enable_interrupts(void)
{
    __asm__ volatile("sti");
}

/**
 * 恢复中断状态
 */
void x86_64_restore_interrupts(bool state)
{
    if (state) {
        __asm__ volatile("sti");
    }
}

/* ==================== 特权级查询 ==================== */

/**
 * 获取当前特权级
 */
uint32_t x86_64_get_privilege_level(void)
{
    uint64_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs & 3;
}

/* ==================== IO端口操作 ==================== */

/**
 * 读取8位IO端口
 */
uint8_t x86_64_inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * 写入8位IO端口
 */
void x86_64_outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * 读取16位IO端口
 */
uint16_t x86_64_inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * 写入16位IO端口
 */
void x86_64_outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * 读取32位IO端口
 */
uint32_t x86_64_inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * 写入32位IO端口
 */
void x86_64_outl(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* ==================== 调试 ==================== */

/**
 * 触发断点（INT3）
 */
void x86_64_breakpoint(void)
{
    __asm__ volatile("int3");
}

/* ==================== GDT加载 ==================== */

/**
 * 加载GDT
 * 
 * 修复：使用对齐安全的方式读取gdt_ptr结构
 * 避免未对齐内存访问（虽然x86-64支持，但可能导致性能问题）
 */
void gdt_load(void *gdt_ptr_arg)
{
    /*
     * gdt_ptr_arg 指向 gdt_ptr_t 结构：
     *   uint16_t limit;  // 偏移 0-1
     *   uint64_t base;   // 偏移 2-9 (packed, 无对齐填充)
     * 
     * 由于结构体是 packed，base 字段从偏移 2 开始
     * 直接读取可能导致未对齐访问
     */
    
    /* 方法1：使用 memcpy 安全读取（推荐） */
    uint16_t limit;
    uint64_t base;
    
    /* 读取 limit (偏移 0-1) */
    __builtin_memcpy(&limit, gdt_ptr_arg, sizeof(limit));
    
    /* 读取 base (偏移 2-9) */
    __builtin_memcpy(&base, (uint8_t*)gdt_ptr_arg + 2, sizeof(base));
    
    /* 方法2：构造对齐的 GDT 指针结构（用于 lgdt 指令） */
    /* lgdt 需要 10 字节的结构：{ limit(2), base(8) } */
    /* 注意：lgdt 指令本身对结构的对齐有要求 */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed, aligned(16))) gdt_desc;
    
    gdt_desc.limit = limit;
    gdt_desc.base = base;

    __asm__ volatile("lgdt %0" : : "m"(gdt_desc));
}

/* ==================== 多核支持 ==================== */

/**
 * 获取当前CPU ID（通过CPUID指令）
 * 返回APIC ID作为CPU ID
 */
cpu_id_t x86_64_get_cpu_id(void)
{
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1));

    /* APIC ID在EBX的高8位（bits 24-31） */
    return (cpu_id_t)(ebx >> 24);
}

/* ==================== UART 串口实现 ==================== */

/* UART 寄存器偏移 (8250/16550 兼容) */
#define UART_RBR    0x00    /* 接收缓冲寄存器 */
#define UART_THR    0x00    /* 发送保持寄存器 */
#define UART_IER    0x01    /* 中断使能寄存器 */
#define UART_FCR    0x02    /* FIFO控制寄存器 */
#define UART_LCR    0x03    /* 线路控制寄存器 */
#define UART_MCR    0x04    /* 调制解调控制寄存器 */
#define UART_LSR    0x05    /* 线路状态寄存器 */
#define UART_DLL    0x00    /* 波特率除数低字节 (DLAB=1) */
#define UART_DLM    0x01    /* 波特率除数高字节 (DLAB=1) */

/* 线路状态寄存器位 */
#define UART_LSR_DR     (1 << 0)    /* 数据就绪 */
#define UART_LSR_THRE   (1 << 5)    /* 发送保持寄存器空 */

/* 线路控制寄存器位 */
#define UART_LCR_DLAB   (1 << 7)    /* 除数锁存访问位 */
#define UART_LCR_8N1    0x03        /* 8数据位，无校验，1停止位 */

/**
 * 初始化 UART (x86_64 使用 8250/16550 兼容 UART)
 */
void x86_64_uart_init(phys_addr_t base, uint32_t baud)
{
    /* 计算波特率除数：115200 = 1843200 / (16 * 1) */
    uint16_t divisor = 1;  /* 默认 115200 */
    if (baud > 0 && baud != 115200) {
        divisor = (uint16_t)(1843200 / (16 * baud));
    }
    
    /* 禁用中断 */
    x86_64_outb((uint16_t)(base + UART_IER), 0x00);
    
    /* 启用 DLAB，设置波特率 */
    x86_64_outb((uint16_t)(base + UART_LCR), UART_LCR_DLAB);
    x86_64_outb((uint16_t)(base + UART_DLL), (uint8_t)(divisor & 0xFF));
    x86_64_outb((uint16_t)(base + UART_DLM), (uint8_t)((divisor >> 8) & 0xFF));
    
    /* 8N1 配置（同时清除 DLAB） */
    x86_64_outb((uint16_t)(base + UART_LCR), UART_LCR_8N1);
    
    /* 禁用 FIFO */
    x86_64_outb((uint16_t)(base + UART_FCR), 0x00);
    
    /* 禁用 RTS/DTR */
    x86_64_outb((uint16_t)(base + UART_MCR), 0x00);
}

/**
 * 发送单个字符
 */
void x86_64_uart_putc(phys_addr_t base, char c)
{
    /* 等待发送保持寄存器空 */
    while ((x86_64_inb((uint16_t)(base + UART_LSR)) & UART_LSR_THRE) == 0) {
        x86_64_idle();
    }
    x86_64_outb((uint16_t)(base + UART_THR), (uint8_t)c);
}

/**
 * 接收单个字符（阻塞）
 */
char x86_64_uart_getc(phys_addr_t base)
{
    /* 等待数据就绪 */
    while ((x86_64_inb((uint16_t)(base + UART_LSR)) & UART_LSR_DR) == 0) {
        x86_64_idle();
    }
    return (char)x86_64_inb((uint16_t)(base + UART_RBR));
}

/**
 * 检查是否有数据可读
 */
bool x86_64_uart_rx_ready(phys_addr_t base)
{
    return (x86_64_inb((uint16_t)(base + UART_LSR)) & UART_LSR_DR) != 0;
}

/**
 * 检查是否可以发送
 */
bool x86_64_uart_tx_ready(phys_addr_t base)
{
    return (x86_64_inb((uint16_t)(base + UART_LSR)) & UART_LSR_THRE) != 0;
}

/**
 * 获取默认 UART 基地址
 */
phys_addr_t x86_64_uart_get_default_base(void)
{
    return 0x3F8;  /* COM1 */
}

/* ==================== 通用接口别名 ==================== */
/* 这些函数供 hal.c 调用，实现架构无关接口 */

void arch_uart_init(phys_addr_t base, uint32_t baud)
{
    x86_64_uart_init(base, baud);
}

void arch_uart_putc(phys_addr_t base, char c)
{
    x86_64_uart_putc(base, c);
}

char arch_uart_getc(phys_addr_t base)
{
    return x86_64_uart_getc(base);
}

bool arch_uart_rx_ready(phys_addr_t base)
{
    return x86_64_uart_rx_ready(base);
}

bool arch_uart_tx_ready(phys_addr_t base)
{
    return x86_64_uart_tx_ready(base);
}

phys_addr_t arch_uart_get_default_base(void)
{
    return x86_64_uart_get_default_base();
}

/* ==================== 系统调用接口 ==================== */

/**
 * 执行系统调用 (x86_64: 使用 SYSCALL 指令)
 */
void arch_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
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
}

/**
 * 系统调用返回 (x86_64: 使用 SYSRETQ 指令)
 */
void arch_syscall_return(u64 ret_val)
{
    __asm__ volatile(
        "mov %0, %%rax\n"
        "sysretq\n"
        :
        : "r"(ret_val)
        : "rax", "rcx", "r11", "memory"
    );
}

/* ==================== 异常处理接口 ==================== */

/**
 * 触发异常 (x86_64: 使用 UD2 指令)
 */
void arch_trigger_exception(u32 exc_num)
{
    /* x86_64: INT指令需要立即数，使用UD2触发未定义指令异常 */
    /* 对于特定的异常号，需要使用IDT门或中断控制器 */
    (void)exc_num;
    __asm__ volatile("ud2");
}

/* ==================== 调试接口 ==================== */

/**
 * 栈回溯 (x86_64: 通过 RBP 寄存器遍历栈帧)
 */
void arch_stack_trace(void (*print_func)(const char*, ...))
{
    u64 *rbp;
    u64 *ret_addr;

    /* 获取当前 RBP */
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

    /* 遍历栈帧 */
    for (int i = 0; i < 16 && rbp != NULL; i++) {
        /* 获取返回地址 */
        ret_addr = (u64*)(rbp + 1);

        /* 打印返回地址 */
        print_func("  [0x%016lX]\n", (u64)*ret_addr);

        /* 移动到下一个栈帧 */
        rbp = (u64*)*rbp;

        /* 检查栈帧是否有效 */
        if ((u64)rbp < 0x1000 || (u64)rbp > 0xFFFFFFFFFFFF) {
            break;
        }
    }
}

/* ==================== 上下文初始化 ==================== */

/**
 * 获取架构特定的初始标志值
 * x86_64: RFLAGS with IF=1 (bit 9)
 */
u64 arch_context_init_flags(void)
{
    return 0x202;  /* IF = 1 (启用中断) */
}

/* ==================== 多核支持接口 ==================== */

/**
 * 获取当前 CPU ID (x86_64: 通过 CPUID 获取 APIC ID)
 * 注意：此函数已在上方实现为 x86_64_get_cpu_id
 * 这里提供别名以保持一致性
 */
cpu_id_t arch_get_cpu_id(void)
{
    return x86_64_get_cpu_id();
}

/* ==================== HAL 操作表注册 ==================== */

/* 前向声明 */
extern void x86_64_context_switch(void *prev, void *next);
extern void x86_64_context_init(void *ctx, void *entry, void *stack);

/* x86_64 HAL 操作表 */
static const hal_arch_ops_t x86_64_hal_ops = {
    .arch_name = "x86_64",
    
    /* 内存屏障 */
    .memory_barrier = x86_64_memory_barrier,
    .read_barrier = x86_64_read_barrier,
    .write_barrier = x86_64_write_barrier,
    
    /* 中断控制 */
    .disable_interrupts = x86_64_disable_interrupts,
    .enable_interrupts = x86_64_enable_interrupts,
    .restore_interrupts = x86_64_restore_interrupts,
    
    /* 时间 */
    .get_timestamp = x86_64_get_timestamp,
    .udelay = NULL,  /* TODO: 实现 */
    
    /* 特权级 */
    .get_privilege_level = x86_64_get_privilege_level,
    
    /* 上下文 */
    .save_context = x86_64_save_context,
    .restore_context = x86_64_restore_context,
    .context_switch = x86_64_context_switch,
    .context_init = x86_64_context_init,
    .context_init_flags = arch_context_init_flags,
    
    /* 系统调用 */
    .syscall_invoke = arch_syscall_invoke,
    .syscall_return = arch_syscall_return,
    
    /* 异常 */
    .trigger_exception = arch_trigger_exception,
    .halt = x86_64_halt,
    .idle = x86_64_idle,
    .breakpoint = x86_64_breakpoint,
    .stack_trace = NULL,  /* TODO: 实现 */
    
    /* IO 端口 */
    .inb = x86_64_inb,
    .inw = x86_64_inw,
    .inl = x86_64_inl,
    .outb = x86_64_outb,
    .outw = x86_64_outw,
    .outl = x86_64_outl,
    
    /* CPU */
    .get_cpu_id = x86_64_get_cpu_id,
    
    /* 特性 */
    .supports_io_ports = true,
};

/**
 * 初始化 x86_64 HAL 并注册操作表
 */
void x86_64_hal_init(void)
{
    hal_register_arch_ops(&x86_64_hal_ops);
}