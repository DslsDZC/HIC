# 性能指标

## 概述

HIK 的性能目标是提供极致的系统调用、中断处理和上下文切换性能，同时保持安全性和可靠性。本文档定义了 HIK 的性能指标和测量方法。

## 性能目标

### 核心指标

| 操作类型 | 目标延迟 | 测量方法 | 备注 |
|----------|----------|----------|------|
| 系统调用 | 20-30 ns | TSC 周期计数 | 快速路径 |
| 中断处理 | 0.5-1 μs | TSC 周期计数 | 静态路由 |
| 上下文切换 | 120-150 ns | TSC 周期计数 | 只切换通用寄存器 |
| 能力验证 | < 10 ns | TSC 周期计数 | 直接数组访问 |
| IPC 调用 | 50-100 ns | TSC 周期计数 | 域切换开销 |

### 性能基准

```
假设 CPU 频率: 3 GHz
- 1 周期 ≈ 0.33 ns
- 10 周期 ≈ 3.3 ns
- 100 周期 ≈ 33 ns
- 1000 周期 ≈ 333 ns
```

## 性能测量

### TSC 时间戳

```c
// 获取 TSC 时间戳
static inline u64 rdtsc(void) {
    u32 low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | low;
}

// 测量操作延迟
u64 measure_latency(void (*func)(void)) {
    u64 start = rdtsc();
    func();
    u64 end = rdtsc();
    return end - start;
}
```

### 性能计数器

```c
// 性能计数器
typedef struct perf_counter {
    u64 syscall_count;
    u64 syscall_total_cycles;
    u64 irq_count;
    u64 irq_total_cycles;
    u64 context_switch_count;
    u64 context_switch_total_cycles;
} perf_counter_t;

// 测量系统调用
void perf_measure_syscall(u64 cycles) {
    g_perf_counter.syscall_count++;
    g_perf_counter.syscall_total_cycles += cycles;
}

// 测量中断处理
void perf_measure_irq(u64 cycles) {
    g_perf_counter.irq_count++;
    g_perf_counter.irq_total_cycles += cycles;
}

// 测量上下文切换
void perf_measure_context_switch(u64 cycles) {
    g_perf_counter.context_switch_count++;
    g_perf_counter.context_switch_total_cycles += cycles;
}
```

## 性能分析

### 系统调用性能

```c
// 系统调用入口
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, 
                     u64 arg3, u64 arg4) {
    u64 start = rdtsc();
    
    // 执行系统调用
    hik_status_t status = execute_syscall(syscall_num, arg1, arg2, arg3, arg4);
    
    u64 end = rdtsc();
    u64 cycles = end - start;
    
    // 记录性能
    perf_measure_syscall(cycles);
    
    // 检查是否超过目标
    if (cycles > 90) {  // 30 ns @ 3GHz
        log_warning("Slow syscall: %lu cycles\n", cycles);
    }
}
```

### 中断处理性能

```c
// 中断处理入口
void irq_handler(u64 irq_vector) {
    u64 start = rdtsc();
    
    // 查找 IRQ 处理程序
    irq_handler_t handler = irq_table[irq_vector];
    
    if (handler) {
        // 执行处理程序
        handler(irq_vector);
    }
    
    u64 end = rdtsc();
    u64 cycles = end - start;
    
    // 记录性能
    perf_measure_irq(cycles);
    
    // 检查是否超过目标
    if (cycles > 3000) {  // 1 μs @ 3GHz
        log_warning("Slow IRQ: %lu cycles\n", cycles);
    }
}
```

### 上下文切换性能

```c
// 上下文切换
void schedule(void) {
    thread_t *prev = g_current_thread;
    thread_t *next = select_next_thread();
    
    u64 start = rdtsc();
    
    // 切换上下文
    context_switch(prev, next);
    
    u64 end = rdtsc();
    u64 cycles = end - start;
    
    // 记录性能
    perf_measure_context_switch(cycles);
    
    // 检查是否超过目标
    if (cycles > 450) {  // 150 ns @ 3GHz
        log_warning("Slow context switch: %lu cycles\n", cycles);
    }
}
```

## 性能优化

### 快速路径优化

```c
// 快速系统调用路径
static inline u64 fast_syscall(u64 num, u64 arg1, u64 arg2, u64 arg3) {
    u64 result;
    __asm__ volatile (
        "syscall"
        : "=a"(result)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return result;
}
```

### 中断快速路径

```c
// 静态 IRQ 路由表
static irq_handler_t irq_static_routes[256];

// 快速中断处理
void fast_irq_handler(u64 irq_vector) {
    irq_handler_t handler = irq_static_routes[irq_vector];
    if (handler) {
        handler(irq_vector);
    }
}
```

### 上下文切换优化

```c
// 最小化上下文切换
void minimal_context_switch(thread_t *prev, thread_t *next) {
    // 只保存/恢复必要寄存器
    __asm__ volatile (
        "mov %%rsp, %0\n"
        "mov %1, %%rsp\n"
        : "=m"(prev->rsp)
        : "m"(next->rsp)
        : "memory"
    );
}
```

## 性能监控

### 实时监控

```c
// 性能监控服务
void performance_monitor_service(void) {
    while (1) {
        // 获取性能统计
        perf_stats_t stats;
        perf_get_stats(&stats);
        
        // 输出统计
        console_puts("=== Performance Stats ===\n");
        console_printf("Syscall: %llu calls, %llu cycles avg\n",
                      stats.syscall_count,
                      stats.syscall_total_cycles / stats.syscall_count);
        console_printf("IRQ: %llu calls, %llu cycles avg\n",
                      stats.irq_count,
                      stats.irq_total_cycles / stats.irq_count);
        console_printf("Context Switch: %llu calls, %llu cycles avg\n",
                      stats.context_switch_count,
                      stats.context_switch_total_cycles / stats.context_switch_count);
        
        // 延迟
        hal_udelay(1000000);  // 1秒
    }
}
```

### 性能报告

```c
// 生成性能报告
void perf_print_stats(void) {
    console_puts("\n========== 性能统计 ==========\n");
    
    // 系统调用统计
    if (g_perf_counter.syscall_count > 0) {
        u64 avg = g_perf_counter.syscall_total_cycles / g_perf_counter.syscall_count;
        console_puts("系统调用:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.syscall_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期 (");
        console_putu64(avg / 3);
        console_puts(" ns @ 3GHz)\n");
    }
    
    // 中断处理统计
    if (g_perf_counter.irq_count > 0) {
        u64 avg = g_perf_counter.irq_total_cycles / g_perf_counter.irq_count;
        console_puts("\n中断处理:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.irq_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期 (");
        console_putu64(avg / 3);
        console_puts(" ns @ 3GHz)\n");
    }
    
    // 上下文切换统计
    if (g_perf_counter.context_switch_count > 0) {
        u64 avg = g_perf_counter.context_switch_total_cycles / g_perf_counter.context_switch_count;
        console_puts("\n上下文切换:\n");
        console_puts("  次数: ");
        console_putu64(g_perf_counter.context_switch_count);
        console_puts("\n");
        console_puts("  平均延迟: ");
        console_putu64(avg);
        console_puts(" 周期 (");
        console_putu64(avg / 3);
        console_puts(" ns @ 3GHz)\n");
    }
    
    console_puts("==============================\n\n");
}
```

## 性能测试

### 基准测试

```c
// 系统调用基准测试
void benchmark_syscall(void) {
    const u64 iterations = 1000000;
    u64 total_cycles = 0;
    
    for (u64 i = 0; i < iterations; i++) {
        u64 start = rdtsc();
        sys_ipc_call(NULL);  // 示例系统调用
        u64 end = rdtsc();
        total_cycles += (end - start);
    }
    
    u64 avg_cycles = total_cycles / iterations;
    console_printf("Syscall avg: %llu cycles (%llu ns @ 3GHz)\n",
                  avg_cycles, avg_cycles / 3);
}

// 中断处理基准测试
void benchmark_irq(void) {
    const u64 iterations = 100000;
    u64 total_cycles = 0;
    
    for (u64 i = 0; i < iterations; i++) {
        u64 start = rdtsc();
        irq_handler(32);  // 示例中断
        u64 end = rdtsc();
        total_cycles += (end - start);
    }
    
    u64 avg_cycles = total_cycles / iterations;
    console_printf("IRQ avg: %llu cycles (%llu ns @ 3GHz)\n",
                  avg_cycles, avg_cycles / 3);
}
```

## 性能调优

### 识别瓶颈

1. **分析热点**: 使用性能分析工具识别热点代码
2. **检查缓存**: 检查缓存未命中率
3. **分析分支**: 检查分支预测准确率
4. **测量延迟**: 测量内存访问延迟

### 优化策略

1. **缓存优化**: 优化数据布局提高缓存命中率
2. **分支优化**: 减少分支和预测失败
3. **内存优化**: 减少内存访问次数
4. **算法优化**: 使用更高效的算法

## 最佳实践

1. **基准测试**: 定期运行基准测试
2. **性能监控**: 实时监控系统性能
3. **瓶颈分析**: 及时识别和解决性能瓶颈
4. **优化迭代**: 持续优化性能
5. **目标跟踪**: 追踪性能目标的达成情况

## 相关文档

- [优化技术](./18-OptimizationTechniques.md) - 优化技术详解
- [快速路径](./19-FastPath.md) - 快速路径优化
- [架构设计](./02-Architecture.md) - 架构设计

---

*最后更新: 2026-02-14*