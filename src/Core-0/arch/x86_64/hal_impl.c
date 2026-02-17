/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK x86_64架构特定HAL实现
 * 
 * 本文件提供x86_64架构的HAL接口实现
 * 架构无关代码通过hal.c调用这些函数
 */

#include <stdint.h>

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
    return (cs >> 3) & 3;
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