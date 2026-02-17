/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86_64 HAL简化实现 - 使用内联汇编减少架构依赖
 */

#include "../types.h"

/* 停止CPU */
void x86_64_halt(void) {
    __asm__ volatile ("hlt");
}

/* CPU空闲 */
void x86_64_idle(void) {
    __asm__ volatile ("pause");
}

/* 时间戳 */
u64 x86_64_get_timestamp(void) {
    u64 tsc;
    __asm__ volatile ("rdtsc" : "=A"(tsc));
    return tsc;
}

/* 内存屏障 */
void x86_64_memory_barrier(void) {
    __asm__ volatile ("mfence" ::: "memory");
}

/* 读屏障 */
void x86_64_read_barrier(void) {
    __asm__ volatile ("lfence" ::: "memory");
}

/* 写屏障 */
void x86_64_write_barrier(void) {
    __asm__ volatile ("sfence" ::: "memory");
}

/* 禁用中断 */
bool x86_64_disable_interrupts(void) {
    u64 flags;
    __asm__ volatile (
        "pushfq\n"
        "popq %0\n"
        "cli\n"
        : "=r"(flags)
        :: "memory"
    );
    return (flags & 0x200) != 0;
}

/* 启用中断 */
void x86_64_enable_interrupts(void) {
    __asm__ volatile ("sti");
}

/* 恢复中断 */
void x86_64_restore_interrupts(bool state) {
    if (state) {
        __asm__ volatile ("sti");
    }
}

/* 获取特权级 */
u32 x86_64_get_privilege_level(void) {
    u64 cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return (u32)(cs & 0x3);
}

/* IO端口读取 - 8位 */
u8 x86_64_inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* IO端口写入 - 8位 */
void x86_64_outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* IO端口读取 - 16位 */
u16 x86_64_inw(u16 port) {
    u16 value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* IO端口写入 - 16位 */
void x86_64_outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/* IO端口读取 - 32位 */
u32 x86_64_inl(u16 port) {
    u32 value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* IO端口写入 - 32位 */
void x86_64_outl(u16 port, u32 value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* 断点 */
void x86_64_breakpoint(void) {
    __asm__ volatile ("int3");
}

/* 保存上下文 */
void x86_64_save_context(void *ctx) {
    (void)ctx;
}

/* 恢复上下文 */
void x86_64_restore_context(void *ctx) {
    (void)ctx;
}

/* 上下文切换 */
void context_switch(void *old_ctx, void *new_ctx) {
    (void)old_ctx;
    (void)new_ctx;
}
