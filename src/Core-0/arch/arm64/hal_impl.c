/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK ARM64架构特定HAL实现
 * 
 * 本文件提供ARM64架构的HAL接口实现
 * 架构无关代码通过hal.c调用这些函数
 */

#include <stdint.h>

/* ==================== 上下文保存和恢复 ==================== */

/**
 * 保存ARM64上下文
 * 这个函数由汇编代码提供
 */
void arm64_save_context(void *ctx);

/**
 * 恢复ARM64上下文
 * 这个函数由汇编代码提供
 */
void arm64_restore_context(void *ctx);

/* ==================== CPU控制 ==================== */

/**
 * 停止CPU（WFI指令）
 */
void arm64_halt(void)
{
    __asm__ volatile("wfi");
}

/**
 * 空转等待（YIELD指令）
 */
void arm64_idle(void)
{
    __asm__ volatile("yield");
}

/* ==================== 时间戳 ==================== */

/**
 * 获取系统计数器（CNTVCT_EL0）
 */
uint64_t arm64_get_timestamp(void)
{
    uint64_t cnt;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(cnt));
    return cnt;
}

/* ==================== 内存屏障 ==================== */

/**
 * 完整内存屏障（DMB ISH）
 */
void arm64_memory_barrier(void)
{
    __asm__ volatile("dmb ish" ::: "memory");
}

/**
 * 读屏障（DMB ISHLD）
 */
void arm64_read_barrier(void)
{
    __asm__ volatile("dmb ishld" ::: "memory");
}

/**
 * 写屏障（DMB ISH）
 */
void arm64_write_barrier(void)
{
    __asm__ volatile("dmb ish" ::: "memory");
}

/* ==================== 中断控制 ==================== */

/**
 * 禁用中断并返回之前的状态
 */
bool arm64_disable_interrupts(void)
{
    uint64_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("msr daifset, #2");
    return (daif & (1 << 7)) == 0;
}

/**
 * 启用中断
 */
void arm64_enable_interrupts(void)
{
    __asm__ volatile("msr daifclr, #2");
}

/**
 * 恢复中断状态
 */
void arm64_restore_interrupts(bool state)
{
    if (state) {
        __asm__ volatile("msr daifclr, #2");
    }
}

/* ==================== 特权级查询 ==================== */

/**
 * 获取当前特权级（CurrentEL）
 */
uint32_t arm64_get_privilege_level(void)
{
    uint64_t currentel;
    __asm__ volatile("mrs %0, currentel" : "=r"(currentel));
    return (currentel >> 2) & 3;
}

/* ==================== 调试 ==================== */

/**
 * 触发断点（BRK 0）
 */
void arm64_breakpoint(void)
{
    __asm__ volatile("brk 0");
}