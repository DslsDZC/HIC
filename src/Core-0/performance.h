/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC性能优化指南
 * 遵循文档第9节：性能要求
 * 目标：
 * - 系统调用延迟：20-30纳秒
 * - 中断处理延迟：0.5-1微秒
 * - 线程切换延迟：120-150纳秒
 */

#ifndef HIC_KERNEL_PERFORMANCE_H
#define HIC_KERNEL_PERFORMANCE_H

#include "types.h"

/* 性能优化策略 */

/* 1. 快速系统调用优化 */
#define SYSCALL_FAST_PATH
#ifdef SYSCALL_FAST_PATH
/* 使用syscall/sysret指令（x86-64）减少上下文切换开销 */
static inline u64 fast_syscall_entry(u64 num, u64 arg1, u64 arg2, u64 arg3) {
    u64 result;
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return result;
}
#endif

/* 2. 中断处理优化 */
#define IRQ_FAST_PATH
#ifdef IRQ_FAST_PATH
/* 只保存必要寄存器，延迟保存完整上下文 */
/* 静态路由表快速查找 */
/* 避免页表切换 */
#endif

/* 3. 线程切换优化 */
#define SCHEDULER_FAST_SWITCH
#ifdef SCHEDULER_FAST_SWITCH
/* 只切换通用寄存器 */
/* 避免FPU状态保存（除非使用） */
/* 使用快速上下文切换指令 */
#endif

/* 4. 缓存优化 */
#define CACHE_LINE_SIZE 64
#define __align_cache __attribute__((aligned(CACHE_LINE_SIZE)))

/* 5. 分支预测优化 */
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* 6. 内联关键函数 */
#define FORCE_INLINE __attribute__((always_inline)) static inline

/* 性能计数器 */
typedef struct {
    u64 syscall_count;
    u64 syscall_total_cycles;
    u64 irq_count;
    u64 irq_total_cycles;
    u64 context_switch_count;
    u64 context_switch_total_cycles;
} perf_counter_t;

/* 性能测量接口 */
void perf_init(void);
void perf_measure_syscall(u64 cycles);
void perf_measure_irq(u64 cycles);
void perf_measure_context_switch(u64 cycles);
void perf_measure_timer(u64 ticks);
void perf_update_stats(void);

/* 性能统计输出 */
void perf_print_stats(void);

/* 时间接口 */
u64 get_system_time_ns(void);

/* 定时器接口 */
void timer_update(void);

#endif /* HIC_KERNEL_PERFORMANCE_H */
