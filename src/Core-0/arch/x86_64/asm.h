/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64汇编接口
 */

#ifndef HIC_ARCH_X86_64_ASM_H
#define HIC_ARCH_X86_64_ASM_H

#include <stdint.h>

/* 特权级 */
#define RING0   0
#define RING3   3

/* 段选择子
 * 格式：索引 << 3 | RPL
 * 其中索引是 GDT 中的条目索引（从1开始），RPL 是请求特权级
 */
#define KERNEL_CS  (0x08)          /* 索引1，RPL0 */
#define KERNEL_DS  (0x10)          /* 索引2，RPL0 */
#define USER_CS    (0x1B)          /* 索引3，RPL3 */
#define USER_DS    (0x23)          /* 索引4，RPL3 */

/* x86-64控制寄存器 */
static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val) : "memory");
}

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline void write_cr2(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr2" : : "r"(val) : "memory");
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline uint64_t read_cr4(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val) : "memory");
}

/* 标志寄存器 */
static inline uint64_t read_rflags(void)
{
    uint64_t val;
    __asm__ volatile ("pushf; pop %0" : "=r"(val));
    return val;
}

static inline void write_rflags(uint64_t val)
{
    __asm__ volatile ("push %0; popf" : : "r"(val) : "memory");
}

/* 禁用/启用中断 */
static inline void cli(void)
{
    __asm__ volatile ("cli" ::: "memory");
}

static inline void sti(void)
{
    __asm__ volatile ("sti" ::: "memory");
}

/* 暂停 */
static inline void hlt(void)
{
    __asm__ volatile ("hlt");
}

/* 内存屏障 */
static inline void mfence(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

/* 读取时间戳计数器 */
static inline uint64_t rdtsc(void)
{
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

/* 无限循环 */
static inline void hang(void)
{
    while (1) {
        hlt();
    }
}

/* 外部函数声明 */
extern void context_switch(void *prev, void *next);
extern void interrupt_handler_stub(void);

/* 上下文保存和恢复函数声明 */
extern void arch_save_context(void *ctx);
extern void arch_restore_context(void *ctx);

/* GDT加载函数 */
extern void gdt_load(void *gdt_ptr);

/* 延迟函数声明 */
void hal_udelay(u32 us);

#endif /* HIC_ARCH_X86_64_ASM_H */