/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC ARM64架构特定HAL实现
 * 
 * 本文件提供ARM64架构的HAL接口实现
 * 架构无关代码通过hal.c调用这些函数
 */

#include <stdint.h>
#include <stdbool.h>
#include "hal.h"

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

/* ==================== 系统调用接口 ==================== */

/**
 * 执行系统调用 (ARM64: 使用 SVC 指令)
 */
void arch_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
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
}

/**
 * 系统调用返回 (ARM64: 使用 ERET 指令)
 */
void arch_syscall_return(u64 ret_val)
{
    __asm__ volatile(
        "mov x0, %0\n"
        "eret\n"
        :
        : "r"(ret_val)
        : "x0", "memory"
    );
}

/* ==================== 异常处理接口 ==================== */

/**
 * 触发异常 (ARM64: 使用 BRK 指令)
 */
void arch_trigger_exception(u32 exc_num)
{
    __asm__ volatile("brk %0" : : "I"((u16)exc_num));
}

/* ==================== 多核支持接口 ==================== */

/**
 * 获取当前 CPU ID (ARM64: 通过 MPIDR_EL1 获取)
 */
cpu_id_t arch_get_cpu_id(void)
{
    u64 mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (cpu_id_t)(mpidr & 0xFF);  /* 取 Aff0 字段 */
}

/* ==================== 上下文初始化 ==================== */

/**
 * 获取架构特定的初始标志值
 * ARM64: PSTATE 默认值
 */
u64 arch_context_init_flags(void)
{
    return 0x0;
}