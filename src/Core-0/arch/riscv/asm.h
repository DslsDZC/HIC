/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC RISC-V架构支持
 * 遵循文档第12节：无MMU架构的简化设计
 */

#ifndef HIC_ARCH_RISCV_ASM_H
#define HIC_ARCH_RISCV_ASM_H

#include "types.h"

/* RISC-V特权级 */
#define RISCV_U_MODE  0  /* 用户态 */
#define RISCV_S_MODE  1  /* 监管态 */
#define RISCV_M_MODE  3  /* 机器态 */

/* RISC-V CSR寄存器 */

/* 当前特权级 */
static inline u32 riscv_get_current_priv(void) {
    u32 priv;
    __asm__ volatile ("csrr %0, mstatus" : "=r"(priv));
    return (priv >> 11) & 0x3;
}

/* 控制寄存器 */
static inline u64 riscv_get_sstatus(void) {
    u64 val;
    __asm__ volatile ("csrr %0, sstatus" : "=r"(val));
    return val;
}

static inline void riscv_set_sstatus(u64 val) {
    __asm__ volatile ("csrw sstatus, %0" : : "r"(val));
}

/* 内存屏障 */
static inline void riscv_fence(void) {
    __asm__ volatile ("fence" ::: "memory");
}

/* 页表基址寄存器 */
static inline u64 riscv_get_satp(void) {
    u64 val;
    __asm__ volatile ("csrr %0, satp" : "=r"(val));
    return val;
}

static inline void riscv_set_satp(u64 val) {
    __asm__ volatile ("csrw satp, %0" : : "r"(val));
}

/* 中断控制 */
static inline void riscv_disable_irq(void) {
    __asm__ volatile ("csrci sstatus, 2" ::: "memory");
}

static inline void riscv_enable_irq(void) {
    __asm__ volatile ("csrsi sstatus, 2" ::: "memory");
}

/* 获取时间戳 */
static inline u64 riscv_get_timestamp(void) {
    u64 time;
    __asm__ volatile ("rdtime %0" : "=r"(time));
    return time;
}

/* Halt */
static inline void riscv_halt(void) {
    __asm__ volatile ("wfi");
}

#endif /* HIC_ARCH_RISCV_ASM_H */