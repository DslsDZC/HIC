/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC性能测量实现
 */

#include "performance.h"
#include "lib/console.h"
#include "lib/mem.h"
#include "hal.h"

/* 全局定时器计数 */
extern u64 g_timer_ticks;

/* 获取系统时间（纳秒） */
u64 get_system_time_ns(void)
{
    /* 使用HAL接口获取时间戳 */
    u64 timestamp = hal_get_timestamp();
    
    /* 假设时间戳是纳秒级，如果是其他单位需要转换 */
    /* 这里假设TSC频率约为3GHz，转换为纳秒 */
    return timestamp / 3;  /* 简化：TSC周期/3 ≈ 纳秒 */
}

/* 定时器更新函数 */
void timer_update(void)
{
    /* 更新定时器状态 */
    static u64 timer_ticks = 0;
    timer_ticks++;
    
    /* 触发调度器tick */
    extern void scheduler_tick(void);
    scheduler_tick();
    
    /* 检查是否有超时的线程需要唤醒 */
    extern void thread_check_timeouts(void);
    thread_check_timeouts();
    
    /* 更新性能统计 */
    extern void perf_measure_timer(u64);
    perf_measure_timer(timer_ticks);
}

/* 全局性能计数器 */
static perf_counter_t g_perf_counter;

/* 初始化性能测量 */
void perf_init(void)
{
    memzero(&g_perf_counter, sizeof(perf_counter_t));
    console_puts("[PERF] Performance measurement initialized\n");
}

/* 测量系统调用 */
void perf_measure_syscall(u64 cycles)
{
    g_perf_counter.syscall_count++;
    g_perf_counter.syscall_total_cycles += cycles;
}

/* 测量中断处理 */
void perf_measure_irq(u64 cycles)
{
    g_perf_counter.irq_count++;
    g_perf_counter.irq_total_cycles += cycles;
}

/* 测量上下文切换 */
void perf_measure_context_switch(u64 cycles)
{
    g_perf_counter.context_switch_count++;
    g_perf_counter.context_switch_total_cycles += cycles;
}

/* 测量定时器 */
void perf_measure_timer(u64 ticks)
{
    g_timer_ticks += ticks;
}

/* 更新性能统计 */
void perf_update_stats(void)
{
    /* 定期更新统计信息 */
    /* 可以在这里添加周期性的统计更新逻辑 */
}

/* 性能统计输出 */
void perf_print_stats(void)
{
    console_puts("\n========== 性能统计 ==========\n");
    
    /* 系统调用统计 */
    if (g_perf_counter.syscall_count > 0) {
        u64 avg = g_perf_counter.syscall_total_cycles / g_perf_counter.syscall_count;
        console_puts("系统调用:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.syscall_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期\n");
        console_puts("  目标: 20-30 纳秒 (~60-90 周期 @ 3GHz)\n");
    }
    
    /* 中断处理统计 */
    if (g_perf_counter.irq_count > 0) {
        u64 avg = g_perf_counter.irq_total_cycles / g_perf_counter.irq_count;
        console_puts("\n中断处理:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.irq_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期\n");
        console_puts("  目标: 0.5-1 微秒 (~1500-3000 周期 @ 3GHz)\n");
    }
    
    /* 上下文切换统计 */
    if (g_perf_counter.context_switch_count > 0) {
        u64 avg = g_perf_counter.context_switch_total_cycles / g_perf_counter.context_switch_count;
        console_puts("\n上下文切换:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.context_switch_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期\n");
        console_puts("  目标: 120-150 纳秒 (~360-450 周期 @ 3GHz)\n");
    }
    
    console_puts("==============================\n\n");
}
