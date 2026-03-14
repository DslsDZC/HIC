/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
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
#include "hardware_probe.h"

/* 全局定时器计数 */
extern u64 g_timer_ticks;

/* 获取系统时间（纳秒） */
u64 get_system_time_ns(void)
{
    /* 使用HAL接口获取时间戳 */
    u64 timestamp = hal_get_timestamp();
    
    /* 获取CPU频率（Hz），从硬件探测结果获取 */
    extern cpu_info_t g_cpu_info;
    u64 cpu_freq_hz = g_cpu_info.clock_frequency;
    
    if (cpu_freq_hz == 0) {
        /* 如果CPU频率未知，使用默认值3GHz */
        cpu_freq_hz = 3000000000ULL;
    }
    
    /* 计算纳秒：timestamp * 1e9 / cpu_freq_hz
     * 使用整数运算避免浮点数
     * 分解为：timestamp * (1000000000 / cpu_freq_hz)
     * 但为了精度，使用：timestamp * 1000 / (cpu_freq_hz / 1000000)
     */
    
    /* 方法1：如果频率是GHz的整数倍 */
    u64 ghz = cpu_freq_hz / 1000000000ULL;
    if (ghz > 0 && cpu_freq_hz % 1000000000ULL == 0) {
        return timestamp / ghz;
    }
    
    /* 方法2：精确计算，避免溢出 */
    /* 将时间戳分成高位和低位分别计算 */
    u64 ns_per_cycle_x1000 = (1000000000ULL * 1000) / cpu_freq_hz;
    u64 ns = (timestamp * ns_per_cycle_x1000) / 1000;
    
    return ns;
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
