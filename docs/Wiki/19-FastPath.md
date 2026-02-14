# 快速路径

## 概述

HIK 通过快速路径（Fast Path）优化技术，针对高频操作实现极致性能。快速路径绕过常规的检查流程，直接执行操作，大幅减少延迟。

## 快速路径设计原则

### 1. 优先识别热点
- 系统调用
- 中断处理
- 能力验证
- 域切换

### 2. 简化检查流程
- 预计算常用结果
- 缓存热点数据
- 减少分支

### 3. 内联关键代码
- 编译器内联
- 代码展开
- 避免函数调用

## 系统调用快速路径

### 快速系统调用入口

```c
// x86-64 快速系统调用
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

// 目标: 20-30 ns (60-90 周期 @ 3GHz)
```

### 系统调用快速分发

```c
// 快速系统调用分发
void fast_syscall_dispatch(u64 num, u64 arg1, u64 arg2, u64 arg3, u64 arg4) {
    // 使用跳转表直接分发
    static void (*syscall_table[])(u64, u64, u64, u64) = {
        [SYSCALL_IPC_CALL] = fast_ipc_call,
        [SYSCALL_CAP_TRANSFER] = fast_cap_transfer,
        [SYSCALL_CAP_DERIVE] = fast_cap_derive,
        // ...
    };
    
    if (num < SYSCALL_MAX && syscall_table[num]) {
        syscall_table[num](arg1, arg2, arg3, arg4);
    }
}
```

### IPC 快速路径

```c
// 快速 IPC 调用
hik_status_t fast_ipc_call(ipc_call_params_t *params) {
    // 直接访问，跳过能力检查（在调用方已验证）
    domain_id_t caller = get_current_domain();
    cap_entry_t *endpoint = &g_cap_table[params->endpoint_cap];
    
    // 快速域切换
    return fast_domain_switch(caller, endpoint->endpoint.target_domain, 
                               params->endpoint_cap, 0, NULL, 0);
}

// 目标: 50-100 ns (150-300 周期 @ 3GHz)
```

## 中断快速路径

### 静态 IRQ 路由表

```c
// 静态 IRQ 路由表
static irq_handler_t irq_static_routes[256];

// 快速中断处理
void fast_irq_handler(u64 irq_vector) {
    // 直接查找，无循环
    irq_handler_t handler = irq_static_routes[irq_vector];
    
    if (handler) {
        // 直接调用，无锁
        handler(irq_vector);
    }
}

// 目标: 0.5-1 μs (1500-3000 周期 @ 3GHz)
```

### 中断处理快速路径

```c
// 快速中断处理程序
void fast_timer_irq_handler(u64 vector) {
    // 直接递增计数器
    g_timer_ticks++;
    
    // 检查是否需要调度
    if (g_timer_ticks >= TIMER_QUANTUM) {
        g_timer_ticks = 0;
        g_need_reschedule = true;
    }
}
```

## 能力验证快速路径

### 快速能力验证

```c
// 快速能力验证（内联）
static inline bool fast_capability_exists(cap_id_t cap) {
    // 直接数组访问，无分支
    return cap < CAP_TABLE_SIZE && g_cap_table[cap].cap_id != 0;
}

// 快速能力权限检查
static inline bool fast_capability_check(cap_id_t cap, cap_rights_t required) {
    cap_entry_t *entry = &g_cap_table[cap];
    return (entry->rights & required) == required;
}

// 目标: < 10 ns (< 30 周期 @ 3GHz)
```

### 能力快速转移

```c
// 快速能力转移
hik_status_t fast_cap_transfer(cap_id_t cap, domain_id_t to) {
    cap_entry_t *entry = &g_cap_table[cap];
    
    // 直接修改所有者
    entry->owner = to;
    entry->ref_count++;
    
    return HIK_SUCCESS;
}
```

## 域切换快速路径

### 快速域切换

```c
// 快速域切换
hik_status_t fast_domain_switch(domain_id_t from, domain_id_t to,
                                 cap_id_t endpoint, u64 flags,
                                 void *data, u64 data_len) {
    // 直接切换页表基址
    u64 old_cr3 = read_cr3();
    domain_t *to_domain = &g_domains[to];
    write_cr3(to_domain->page_table);
    
    // 记录切换
    g_current_domain = to;
    
    return HIK_SUCCESS;
}

// 目标: 100-200 ns (300-600 周期 @ 3GHz)
```

## 上下文切换快速路径

### 最小化上下文切换

```c
// 最小化上下文切换（只切换必要寄存器）
void minimal_context_switch(thread_t *prev, thread_t *next) {
    __asm__ volatile (
        // 保存 RSP
        "mov %%rsp, %0\n"
        // 恢复 RSP
        "mov %1, %%rsp\n"
        : "=m"(prev->rsp)
        : "m"(next->rsp)
        : "memory"
    );
}

// 目标: 120-150 ns (360-450 周期 @ 3GHz)
```

## 快速路径优化技巧

### 1. 预计算

```c
// 预计算常用值
static u64 capability_rights_table[CAP_TABLE_SIZE];

// 初始化时预计算
void init_capability_rights_table(void) {
    for (cap_id_t cap = 0; cap < CAP_TABLE_SIZE; cap++) {
        capability_rights_table[cap] = g_cap_table[cap].rights;
    }
}
```

### 2. 缓存热数据

```c
// 缓存当前域
static thread_local domain_id_t g_current_domain_cache;

// 快速获取当前域
static inline domain_id_t fast_get_current_domain(void) {
    return g_current_domain_cache;
}
```

### 3. 分支预测

```c
// 使用 likely/unlikely 提示
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// 快速路径示例
if (likely(fast_capability_exists(cap))) {
    // 快速路径
    return fast_capability_check(cap, required);
} else {
    // 慢速路径
    return full_capability_verify(cap, required);
}
```

### 4. 批量操作

```c
// 批量系统调用
struct syscall_batch {
    u64 syscalls[16];
    u64 args[16][4];
    u64 results[16];
    u32 count;
};

// 批量执行系统调用
void fast_syscall_batch(syscall_batch_t *batch) {
    for (u32 i = 0; i < batch->count; i++) {
        batch->results[i] = fast_syscall(
            batch->syscalls[i],
            batch->args[i][0],
            batch->args[i][1],
            batch->args[i][2]
        );
    }
}
```

## 快速路径监控

### 性能统计

```c
// 快速路径命中率统计
typedef struct fast_path_stats {
    u64 total_calls;
    u64 fast_path_hits;
    u64 fast_path_misses;
    u64 avg_fast_cycles;
    u64 avg_slow_cycles;
} fast_path_stats_t;

// 记录快速路径使用
void record_fast_path_usage(bool hit, u64 cycles) {
    if (hit) {
        g_fast_path_stats.fast_path_hits++;
        g_fast_path_stats.avg_fast_cycles = 
            (g_fast_path_stats.avg_fast_cycles + cycles) / 2;
    } else {
        g_fast_path_stats.fast_path_misses++;
        g_fast_path_stats.avg_slow_cycles = 
            (g_fast_path_stats.avg_slow_cycles + cycles) / 2;
    }
    g_fast_path_stats.total_calls++;
}
```

## 快速路径最佳实践

1. **识别热点**: 使用性能分析工具识别热点
2. **简化流程**: 简化热点代码的检查流程
3. **预计算**: 预计算常用值
4. **缓存数据**: 缓存热数据
5. **测试验证**: 确保快速路径正确性
6. **监控效果**: 监控快速路径命中率

## 相关文档

- [性能指标](./17-PerformanceMetrics.md) - 性能目标
- [优化技术](./18-OptimizationTechniques.md) - 优化技术详解
- [架构设计](./02-Architecture.md) - 架构设计

---

*最后更新: 2026-02-14*