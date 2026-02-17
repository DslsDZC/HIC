<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# RISC-V 架构

## 概述

HIK 支持 RISC-V 架构，利用其简洁的指令集和模块化设计。本文档介绍了 HIK 在 RISC-V 架构上的实现。

## 架构特性

### CPU 特性

- **特权级**: M/S 模式 (Core-0), U 模式 (Application-3)
- **64位地址空间**: 支持 64 位物理和虚拟地址
- **mtime**: 机器定时器用于性能测量
- **ecall**: 环境调用指令用于系统调用

### 寄存器上下文

```c
// RISC-V 寄存器上下文
typedef struct arch_context {
    // 通用寄存器 x0-x31
    u64 x[32];
    
    // 特殊寄存器
    u64 pc;               // 程序计数器
    u64 ra;               // 返回地址
    
    // 系统寄存器
    u64 mstatus;          // 机器状态寄存器
    u64 mepc;             // 机器异常PC
    u64 mcause;           // 机器异常原因
    u64 mtval;            // 机器异常值
    
    // 保存的栈指针
    u64 sp;               // 栈指针
    u64 tp;               // 线程指针
} arch_context_t;
```

## 系统调用

### ecall 系统调用

```c
// RISC-V 快速系统调用
static inline u64 fast_syscall(u64 num, u64 arg1, u64 arg2, u64 arg3) {
    u64 result;
    __asm__ volatile (
        "ecall\n"
        "mv a0, %1\n"
        "mv a1, %2\n"
        "mv a2, %3\n"
        "mv a3, %4\n"
        : "=r"(result)
        : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3)
        : "memory"
    );
    return result;
}

// 目标: 20-30 ns (60-90 周期 @ 3GHz)
```

## 性能优化

### 机器定时器

```c
// 读取机器定时器
static inline u64 read_mtime(void) {
    u64 time;
    __asm__ volatile("csrr %0, mtime" : "=r"(time));
    return time;
}

// 测量操作延迟
u64 measure_cycles(void (*func)(void)) {
    u64 start = read_mtime();
    func();
    u64 end = read_mtime();
    return end - start;
}
```

## 相关文档

- [x86_64 架构](./24-x86_64.md) - x86_64 架构
- [ARM64 架构](./25-ARM64.md) - ARM64 架构
- [快速路径](./19-FastPath.md) - 快速路径优化

---

*最后更新: 2026-02-14*