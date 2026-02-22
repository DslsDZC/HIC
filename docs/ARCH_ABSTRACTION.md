<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC内核架构抽象层文档

## 概述

HIC内核通过引入架构抽象层(Architecture Abstraction Layer)，减少了对特定架构(如x86_64)的依赖，使得内核核心代码可以在不同架构之间更容易移植。

## 架构抽象层设计

### 1. 统一的架构抽象 (`kernel/include/arch.h`)

提供了跨架构的统一接口，包括：

#### 架构类型定义
```c
typedef enum {
    ARCH_UNKNOWN = 0,
    ARCH_X86_64,
    ARCH_ARM64,
    ARCH_RISCV64,
    ARCH_MAX
} arch_type_t;
```

#### 寄存器上下文（架构无关）
```c
typedef struct {
    u64 r[16];           /* 通用寄存器 */
    u64 sp;              /* 栈指针 */
    u64 pc;              /* 程序计数器 */
    u64 fp;              /* 帧指针 */
    u64 flags;           /* 标志寄存器 */
    u64 kernel_sp;       /* 内核栈指针 */
    u64 user_sp;         /* 用户栈指针 */
    u64 reserved[8];
} arch_context_t;
```

#### 内存屏障
- `arch_memory_barrier()` - 完整内存屏障
- `arch_read_barrier()` - 读屏障
- `arch_write_barrier()` - 写屏障

#### 中断控制
- `arch_disable_interrupts()` - 禁用中断
- `arch_enable_interrupts()` - 启用中断

#### 时间获取
- `arch_get_timestamp()` - 获取时间戳

#### 特权级
- `arch_get_privilege_level()` - 获取当前特权级

#### 控制寄存器
- `arch_read_cr()` - 读取控制寄存器
- `arch_write_cr()` - 写入控制寄存器

#### IO端口（仅x86）
- `arch_inb()` - 读取IO端口
- `arch_outb()` - 写入IO端口

### 2. 统一的中断向量定义

将架构特定的中断向量映射为统一的异常类型：
```c
#define EXC_DIVIDE_ERROR        0
#define EXC_DEBUG               1
#define EXC_NMI                 2
#define EXC_PAGE_FAULT          14
#define IRQ_VECTOR_SYSCALL      128
#define IRQ_VECTOR_IRQ_BASE     32
```

### 3. 硬件探测接口架构无关化

#### CPU信息结构
```c
typedef struct cpu_info {
    u32 vendor_id[3];
    u32 version;
    u32 feature_flags[4];
    u32 logical_cores;
    u32 physical_cores;
    u64 clock_frequency;
    arch_type_t arch;  /* 架构类型 */
    ...
} cpu_info_t;
```

#### 设备信息结构（架构无关）
```c
typedef struct device {
    device_type_t type;
    u16 vendor_id;
    u16 device_id;
    u64 base_address;
    u8 irq;
    
    union {
        struct { /* PCI特定 */ } pci;
        struct { /* 平台设备特定 */ } platform;
    };
} device_t;
```

#### 中断控制器信息（架构无关）
```c
typedef struct interrupt_controller {
    u64 base_address;
    u32 irq_base;
    u32 num_irqs;
    u8 enabled;
    char name[32];
} interrupt_controller_t;
```

### 4. 线程上下文架构无关化

将线程控制块中的上下文从x86_64特定改为使用`arch_context_t`：
```c
typedef struct thread {
    ...
    arch_context_t arch_ctx;  /* 架构无关的上下文 */
    ...
} thread_t;
```

### 5. 控制台接口架构无关化

支持多种控制台类型：
```c
typedef enum {
    CONSOLE_TYPE_SERIAL,
    CONSOLE_TYPE_VGA,         /* x86特有 */
    CONSOLE_TYPE_FRAMEBUFFER,
    CONSOLE_TYPE_NONE
} console_type_t;
```

## 架构特定代码隔离

架构特定代码被隔离在以下目录中：
- `kernel/arch/x86_64/` - x86_64特定实现
- 未来可扩展：`kernel/arch/arm64/`, `kernel/arch/riscv64/`

### 架构特定文件
- `arch/x86_64/entry.S` - 内核入口点
- `arch/x86_64/context.S` - 上下文切换
- `arch/x86_64/gdt.c/h` - 全局描述符表
- `arch/x86_64/idt.c/h` - 中断描述符表
- `arch/x86_64/asm.h` - 汇编接口

## 移植指南

要为新的架构添加支持，需要：

1. **创建架构目录**
   ```
   kernel/arch/<arch_name>/
   ```

2. **实现架构特定接口**
   - `entry.S` - 内核入口
   - `context.S` - 上下文切换
   - 中断控制器初始化
   - 内存管理单元初始化

3. **更新架构抽象层**
   - 在`arch.h`中添加架构特定的宏和内联函数
   - 实现架构特定的CPUID/MSR操作

4. **更新硬件探测**
   - 实现架构特定的CPU探测
   - 实现架构特定的设备探测

## 改进效果

### 减少的架构依赖

| 文件 | 改进前 | 改进后 |
|------|--------|--------|
| `thread.h` | x86_64寄存器上下文 | `arch_context_t` |
| `hardware_probe.h` | APIC/IOAPIC特定 | `interrupt_controller_t` |
| `boot_info.c` | `__asm__ volatile("hlt")` | `arch_halt()` |
| `syscall.c` | `mov %0, %%rax` | 架构条件编译 |
| `console.h` | 仅VGA | 支持多种控制台类型 |

### 代码复用性提升

- 核心调度器代码可以在不同架构间共享
- 内存管理器代码架构无关
- 能力系统代码架构无关
- IPC系统代码架构无关

### 可维护性提升

- 架构特定代码集中管理
- 减少了核心代码中的条件编译
- 更容易添加新架构支持

## 未来扩展

1. **添加ARM64支持**
   - 实现ARM64特定中断处理
   - 实现ARM64上下文切换
   - 实现ARM64内存管理

2. **添加RISC-V支持**
   - 实现RISC-V异常处理
   - 实现RISC-V上下文切换
   - 实现RISC-V SBI接口

3. **统一设备模型**
   - 进一步抽象PCI/平台设备
   - 实现设备树解析
   - 统一DMA接口

## 注意事项

1. **性能考虑**
   - 架构抽象层使用内联函数，编译器会优化
   - 关键路径仍可使用架构特定优化

2. **兼容性**
   - 保持向后兼容现有x86_64代码
   - 渐进式迁移，避免大规模重构

3. **测试**
   - 每个架构都需要单独测试
   - 确保架构抽象层的正确性

## 总结

通过引入架构抽象层，HIC内核显著减少了对x86_64架构的依赖，为未来支持ARM64、RISC-V等架构奠定了基础。核心代码的架构无关性提升了代码的可移植性、可维护性和可扩展性。