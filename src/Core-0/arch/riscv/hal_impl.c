/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC RISC-V架构特定HAL实现
 * 
 * 本文件提供RISC-V架构的HAL接口实现
 * 架构无关代码通过hal.c调用这些函数
 */

#include <stdint.h>

/* ==================== 上下文保存和恢复 ==================== */

/**
 * 保存RISC-V上下文
 * 这个函数由汇编代码提供
 */
void riscv64_save_context(void *ctx);

/**
 * 恢复RISC-V上下文
 * 这个函数由汇编代码提供
 */
void riscv64_restore_context(void *ctx);

/* ==================== CPU控制 ==================== */

/**
 * 停止CPU（WFI指令）
 */
void riscv64_halt(void)
{
    __asm__ volatile("wfi");
}

/**
 * 空转等待（NOP指令）
 */
void riscv64_idle(void)
{
    __asm__ volatile("nop");
}

/* ==================== 时间戳 ==================== */

/**
 * 获取时间（RDTIME）
 */
uint64_t riscv64_get_timestamp(void)
{
    uint64_t time;
    __asm__ volatile("rdtime %0" : "=r"(time));
    return time;
}

/* ==================== 内存屏障 ==================== */

/**
 * 完整内存屏障（FENCE IORW, IORW）
 */
void riscv64_memory_barrier(void)
{
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

/**
 * 读屏障（FENCE IR, IR）
 */
void riscv64_read_barrier(void)
{
    __asm__ volatile("fence ir, ir" ::: "memory");
}

/**
 * 写屏障（FENCE OW, OW）
 */
void riscv64_write_barrier(void)
{
    __asm__ volatile("fence ow, ow" ::: "memory");
}

/* ==================== 中断控制 ==================== */

/**
 * 禁用中断并返回之前的状态
 */
bool riscv64_disable_interrupts(void)
{
    uint64_t status;
    __asm__ volatile("csrr %0, mstatus" : "=r"(status));
    __asm__ volatile("csrci mstatus, 8");
    return (status & 8) != 0;
}

/**
 * 启用中断
 */
void riscv64_enable_interrupts(void)
{
    __asm__ volatile("csrsi mstatus, 8");
}

/**
 * 恢复中断状态
 */
void riscv64_restore_interrupts(bool state)
{
    if (state) {
        __asm__ volatile("csrsi mstatus, 8");
    }
}

/* ==================== 特权级查询 ==================== */

/**
 * 获取当前特权级（MSTATUS）
 */
uint32_t riscv64_get_privilege_level(void)
{
    uint64_t status;
    __asm__ volatile("csrr %0, mstatus" : "=r"(status));
    return (status >> 11) & 3;
}

/* ==================== 调试 ==================== */

/**
 * 触发断点（EBREAK）
 */
void riscv64_breakpoint(void)
{
    __asm__ volatile("ebreak");
}