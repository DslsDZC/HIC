/**
 * HIK ARM64架构支持
 * 遵循文档第12节：无MMU架构的简化设计
 */

#ifndef HIK_ARCH_ARM64_ASM_H
#define HIK_ARCH_ARM64_ASM_H

#include "../include/types.h"

/* ARM64特权级 */
#define ARM64_EL0  0  /* 用户态 */
#define ARM64_EL1  1  /* 内核态 */
#define ARM64_EL2  2  /* 虚拟化 */
#define ARM64_EL3  3  /* 安全监控 */

/* ARM64系统寄存器 */

/* 当前特权级 */
static inline u32 arm64_get_current_el(void) {
    u32 el;
    __asm__ volatile ("mrs %0, CurrentEL" : "=r"(el));
    return (el >> 2) & 0x3;
}

/* 控制寄存器SCTLR_EL1 */
static inline u64 arm64_get_sctlr_el1(void) {
    u64 val;
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(val));
    return val;
}

static inline void arm64_set_sctlr_el1(u64 val) {
    __asm__ volatile ("msr sctlr_el1, %0" : : "r"(val));
}

/* 内存屏障 */
static inline void arm64_dsb(void) {
    __asm__ volatile ("dsb sy" ::: "memory");
}

static inline void arm64_dmb(void) {
    __asm__ volatile ("dmb sy" ::: "memory");
}

static inline void arm64_isb(void) {
    __asm__ volatile ("isb" ::: "memory");
}

/* 页表基址寄存器 */
static inline u64 arm64_get_ttbr0_el1(void) {
    u64 val;
    __asm__ volatile ("mrs %0, ttbr0_el1" : "=r"(val));
    return val;
}

static inline void arm64_set_ttbr0_el1(u64 val) {
    __asm__ volatile ("msr ttbr0_el1, %0" : : "r"(val));
}

/* 中断控制 */
static inline void arm64_disable_irq(void) {
    __asm__ volatile ("msr daifset, #2" ::: "memory");
}

static inline void arm64_enable_irq(void) {
    __asm__ volatile ("msr daifclr, #2" ::: "memory");
}

/* 获取时间戳 */
static inline u64 arm64_get_timestamp(void) {
    u64 cnt;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(cnt));
    return cnt;
}

/* Halt */
static inline void arm64_halt(void) {
    __asm__ volatile ("wfi");
}

#endif /* HIK_ARCH_ARM64_ASM_H */