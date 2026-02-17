<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 监控服务

## 概述

HIK 监控服务提供系统监控、性能分析和调试功能。通过监控服务，可以查看系统状态、性能指标和调试信息。

## 监控功能

### 系统状态监控

```c
// 获取系统状态
hik_status_t monitor_get_system_status(system_status_t *status) {
    // 获取域状态
    status->domain_count = g_domain_count;
    
    // 获取线程状态
    status->thread_count = g_thread_count;
    status->active_threads = count_active_threads();
    
    // 获取内存状态
    status->total_memory = g_total_memory;
    status->free_memory = g_free_memory;
    status->used_memory = g_used_memory;
    
    return HIK_SUCCESS;
}
```

### 性能监控

```c
// 获取性能指标
hik_status_t monitor_get_performance_metrics(performance_metrics_t *metrics) {
    // 获取系统调用延迟
    metrics->syscall_latency_ns = g_syscall_latency_ns;
    
    // 获取中断延迟
    metrics->interrupt_latency_ns = g_interrupt_latency_ns;
    
    // 获取上下文切换延迟
    metrics->context_switch_latency_ns = g_context_switch_latency_ns;
    
    return HIK_SUCCESS;
}
```

### 审计日志

```c
// 获取审计日志
hik_status_t monitor_get_audit_logs(audit_log_t *logs, u32 count) {
    // 从审计日志中读取
    u32 log_count = min(count, g_audit_log_count);
    
    for (u32 i = 0; i < log_count; i++) {
        logs[i] = g_audit_logs[g_audit_log_index - log_count + i];
    }
    
    return HIK_SUCCESS;
}
```

## 调试功能

### 调试输出

```c
// 调试输出
void monitor_debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    // 写入调试日志
    console_vprintf("[DEBUG] ");
    console_vprintf(format, args);
    console_vprintf("\n");
    
    // 写入日志文件
    log_debug(format, args);
    
    va_end(args);
}
```

### 断点

```c
// 设置断点
void monitor_set_breakpoint(u64 addr) {
    // 保存原始指令
    u8 original = *(u8*)addr;
    
    // 写入断点指令 (INT3)
    *(u8*)addr = 0xCC;
    
    // 记录断点
    breakpoint_t *bp = &g_breakpoints[g_breakpoint_count++];
    bp->addr = addr;
    bp->original = original;
}
```

## 性能分析

### 性能统计

```c
// 性能统计
void monitor_dump_performance_stats(void) {
    // 系统调用
    console_printf("System calls: %llu, avg latency: %llu ns\n",
                   g_syscall_count, g_syscall_latency_ns);
    
    // 中断
    console_printf("Interrupts: %llu, avg latency: %llu ns\n",
                   g_interrupt_count, g_interrupt_latency_ns);
    
    // 上下文切换
    console_printf("Context switches: %llu, avg latency: %llu ns\n",
                   g_context_switch_count, g_context_switch_latency_ns);
}
```

## 最佳实践

1. **最小化开销**: 监控代码应该最小化性能开销
2. **异步日志**: 使用异步日志记录
3. **采样统计**: 使用采样统计而不是完全统计
4. **资源限制**: 限制监控资源使用

## 相关文档

- [审计日志](./14-AuditLogging.md) - 审计日志
- [性能指标](./17-PerformanceMetrics.md) - 性能指标
- [故障排除](./38-Troubleshooting.md) - 故障排除

---

*最后更新: 2026-02-14*