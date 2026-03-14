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
 */
void gdt_load(void *gdt_ptr)
{
    uint16_t limit = *(uint16_t*)gdt_ptr;
    uint64_t base = *(uint64_t*)((uint8_t*)gdt_ptr + 2);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr_struct;

    gdt_ptr_struct.limit = limit;
    gdt_ptr_struct.base = base;

    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr_struct));
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