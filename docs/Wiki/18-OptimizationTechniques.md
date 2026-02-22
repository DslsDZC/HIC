<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 优化技术

## 概述

HIC 通过多种优化技术实现高性能目标。本文档详细介绍了 HIC 使用的各种优化策略和技术。

## 快速路径优化

### 系统调用快速路径

```c
// 使用 syscall/sysret 指令（x86-64）
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

// 目标: 20-30 ns (60-90 周期 @ 3GHz)
```

### 中断快速路径

```c
// 静态路由表
static irq_handler_t irq_static_routes[256];

// 快速中断处理
void fast_irq_handler(u64 irq_vector) {
    irq_handler_t handler = irq_static_routes[irq_vector];
    if (handler) {
        handler(irq_vector);
    }
}

// 目标: 0.5-1 μs (1500-3000 周期 @ 3GHz)
```

### 上下文切换快速路径

```c
// 只切换通用寄存器
void minimal_context_switch(thread_t *prev, thread_t *next) {
    __asm__ volatile (
        "mov %%rsp, %0\n"
        "mov %1, %%rsp\n"
        : "=m"(prev->rsp)
        : "m"(next->rsp)
        : "memory"
    );
}

// 目标: 120-150 ns (360-450 周期 @ 3GHz)
```

## 缓存优化

### 缓存行对齐

```c
#define CACHE_LINE_SIZE 64

// 数据结构缓存行对齐
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) aligned_data {
    u64 data1;
    u64 data2;
    // ...
} aligned_data_t;
```

### 热数据聚集

```c
// 将频繁访问的数据聚集在一起
typedef struct {
    // 热路径数据
    thread_t *current_thread;
    u64 syscall_count;
    u64 irq_count;
    
    // 冷数据
    // ...
} perf_hot_data_t;
```

### 预取优化

```c
// 数据预取
static inline void prefetch_data(void *addr) {
    __builtin_prefetch(addr, 0, 3);  // 预取到 L1
}
```

## 分支预测优化

### 分支预测提示

```c
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// 使用示例
if (likely(capability_exists(cap))) {
    // 快速路径
} else {
    // 慢速路径
}
```

### 分支减少

```c
// 避免分支
u64 get_capability_fast(cap_id_t cap) {
    // 直接数组访问，无分支
    return g_cap_table[cap].rights;
}
```

## 内存优化

### 直接内存访问

```c
// 物理内存直接映射，无地址转换
void *phys_to_virt(phys_addr_t phys) {
    return (void *)phys;  // 恒等映射
}
```

### 零拷贝

```c
// 共享内存实现零拷贝
void zero_copy_transfer(domain_id_t from, domain_id_t to, 
                        phys_addr_t addr, size_t size) {
    // 直接访问共享内存，无需拷贝
}
```

## 内联优化

### 关键函数内联

```c
// 强制内联关键函数
#define FORCE_INLINE __attribute__((always_inline)) static inline

FORCE_INLINE bool capability_exists_fast(cap_id_t cap) {
    return cap < CAP_TABLE_SIZE && g_cap_table[cap].cap_id != 0;
}
```

## 并行优化

### SIMD 指令

```c
// 使用 SIMD 加速内存操作
void memcpy_simd(void *dst, const void *src, size_t size) {
    __m256i *d = (__m256i *)dst;
    const __m256i *s = (const __m256i *)src;
    size_t vec_count = size / 32;
    
    for (size_t i = 0; i < vec_count; i++) {
        _mm256_store_si256(&d[i], _mm256_load_si256(&s[i]));
    }
}
```

## 优化策略总结

| 优化技术 | 性能提升 | 复杂度 | 应用场景 |
|----------|----------|--------|----------|
| 快速路径 | 30-50% | 低 | 系统调用、中断 |
| 缓存优化 | 20-30% | 中 | 数据访问 |
| 分支优化 | 10-20% | 低 | 条件判断 |
| 内联优化 | 5-15% | 低 | 小函数 |
| SIMD 加速 | 50-100% | 高 | 批量操作 |

## 最佳实践

1. **测量优先**: 优化前先测量性能
2. **热点优化**: 优先优化热点代码
3. **保持简洁**: 避免过度优化
4. **测试验证**: 优化后测试正确性
5. **文档记录**: 记录优化决策

## 相关文档

- [性能指标](./17-PerformanceMetrics.md) - 性能目标
- [快速路径](./19-FastPath.md) - 快速路径详解
- [架构设计](./02-Architecture.md) - 架构设计

---

*最后更新: 2026-02-14*