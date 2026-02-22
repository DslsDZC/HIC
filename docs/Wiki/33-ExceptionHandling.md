<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 异常处理

## 概述

HIC 异常处理机制负责处理硬件异常、系统调用和外部中断。通过异常向量表和异常处理程序，确保系统稳定性和安全性。

## 异常类型

### 硬件异常

- **缺页异常**: 页表项无效
- **保护异常**: 权限不足
- **对齐异常**: 访问未对齐内存
- **设备不可用**: FPU/SSE未启用

### 系统调用

- **系统调用请求**: 用户态进入内核态
- **快速系统调用**: 优化的系统调用路径

### 外部中断

- **定时器中断**: 时钟中断
- **设备中断**: 外部设备请求

## 异常处理流程

### 异常向量

```c
// 异常向量表
void exception_vectors(void) {
    __asm__ volatile (
        ".align 4096\n"
        ".global exception_vectors\n"
        "exception_vectors:\n"
        
        // 0-31: 异常
        "b divide_error\n"
        "b debug_exception\n"
        "b nmi_interrupt\n"
        "b breakpoint\n"
        // ...
        
        // 32-255: 中断
        "b irq_handler\n"
        "b irq_handler\n"
        // ...
    );
}
```

### 异常处理入口

```c
// 异常处理入口
void exception_handler(exception_frame_t *frame) {
    // 记录异常信息
    exception_info_t info = {
        .type = frame->vector,
        .error_code = frame->error_code,
        .rip = frame->rip,
        .rsp = frame->rsp
    };
    
    // 调用异常处理程序
    exception_handler_t handler = exception_handlers[frame->vector];
    if (handler) {
        handler(&info);
    } else {
        // 默认处理
        default_exception_handler(&info);
    }
}
```

## 缺页异常处理

### 缺页异常

```c
// 缺页异常处理
void page_fault_handler(exception_info_t *info) {
    // 检查地址
    u64 fault_addr = read_cr2();
    
    // 检查权限
    if (info->error_code & PF_ERR_WRITE) {
        // 写访问
        handle_write_fault(fault_addr);
    } else {
        // 读访问
        handle_read_fault(fault_addr);
    }
}
```

## 中断处理

### 中断处理

```c
// 中断处理
void irq_handler(exception_frame_t *frame) {
    // 获取中断向量
    u64 irq_vector = frame->vector - IRQ_VECTOR_BASE;
    
    // 查找处理程序
    irq_handler_t handler = irq_handlers[irq_vector];
    if (handler) {
        handler(irq_vector);
    }
    
    // 发送 EOI
    send_eoi(irq_vector);
}
```

## 最佳实践

1. **快速处理**: 异常处理程序应该快速
2. **日志记录**: 记录所有异常信息
3. **恢复机制**: 提供异常恢复机制
4. **资源清理**: 正确清理资源

## 相关文档

- [Core-0](./08-Core0.md) - Core-0 层
- [物理内存](./12-PhysicalMemory.md) - 物理内存管理
- [监控服务](./34-MonitorService.md) - 监控服务

---

*最后更新: 2026-02-14*