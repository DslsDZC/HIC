/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件抽象层 (HAL) - 架构无关接口
 * 
 * 本文件提供架构无关的接口，用于核心代码
 * 核心代码应该只包含此文件，不应直接包含arch.h
 * 
 * 架构隔离原则：
 * 1. 架构无关代码只包含此文件
 * 2. 此文件不包含arch.h
 * 3. 所有架构特定操作通过函数调用
 * 4. 架构检测在hal.c中完成
 */

#ifndef HIC_HAL_H
#define HIC_HAL_H

#include "../types.h"

#include "types.h"

/* Clang Static Analyzer兼容性：确保bool类型可用 */
/* 注意：GCC使用c23标准，bool已经是关键字，不需要定义 */
#ifndef __cplusplus
#if !defined(__bool_true_false_are_defined) && !defined(bool) && !defined(__GNUC__)
typedef unsigned char bool;
#define true 1
#define false 0
#endif
#endif

/* 架构类型枚举 */
typedef enum {
    HAL_ARCH_UNKNOWN = 0,
    HAL_ARCH_X86_64,
    HAL_ARCH_ARM64,
    HAL_ARCH_RISCV64,
    HAL_ARCH_MAX
} hal_arch_type_t;

/* 架构无关的上下文结构（完整实现） */
typedef struct hal_context {
    /* 通用寄存器 */
    u64 general_regs[16];

    /* 特殊寄存器 */
    u64 pc;                  /* 程序计数器 */
    u64 sp;                  /* 栈指针 */
    u64 flags;               /* 标志寄存器 */

    /* 架构特定扩展 */
    u64 arch_specific[32];

    /* 状态信息 */
    u32 state;
    u32 privilege_level;
} hal_context_t;

/* 获取当前架构类型 */
hal_arch_type_t hal_get_arch_type(void);

/* ==================== 内存屏障接口 ==================== */

/**
 * 完整内存屏障
 * 确保所有读写操作都完成
 */
void hal_memory_barrier(void);

/**
 * 读屏障
 * 确保所有读操作完成
 */
void hal_read_barrier(void);

/**
 * 写屏障
 * 确保所有写操作完成
 */
void hal_write_barrier(void);

/* ==================== 中断控制接口 ==================== */

/**
 * 禁用中断
 * 返回之前的中断状态
 */
bool hal_disable_interrupts(void);

/**
 * 启用中断
 */
void hal_enable_interrupts(void);

/**
 * 恢复中断状态
 */
void hal_restore_interrupts(bool state);

/* ==================== 时间接口 ==================== */

/**
 * 返回纳秒级时间戳
 */
u64 hal_get_timestamp(void);

/**
 * 延迟微秒
 */
void hal_udelay(u32 us);

/* ==================== 特权级接口 ==================== */

/**
 * 检查是否在内核态
 */
bool hal_is_kernel_mode(void);

/**
 * 获取当前特权级
 */
u32 hal_get_privilege_level(void);

/* ==================== 页大小接口 ==================== */

#define HAL_PAGE_SIZE       4096
#define HAL_PAGE_SHIFT      12
#define HAL_PAGE_MASK       (HAL_PAGE_SIZE - 1)

/**
 * 页对齐
 */
static inline u64 hal_page_align(u64 addr) {
    return (addr + HAL_PAGE_SIZE - 1) & ~(u64)(HAL_PAGE_SIZE - 1);
}

/**
 * 检查是否页对齐
 */
static inline bool hal_is_page_aligned(u64 addr) {
    return (addr & HAL_PAGE_MASK) == 0;
}

/* ==================== 内存接口 ==================== */

/**
 * 物理地址到虚拟地址映射（恒等映射）
 */
void* hal_phys_to_virt(phys_addr_t phys);

/**
 * 虚拟地址到物理地址映射（恒等映射）
 */
phys_addr_t hal_virt_to_phys(void* virt);

/* ==================== 上下文接口 ==================== */

/**
 * 切换到线程上下文
 */
void hal_context_switch(void* prev, void* next);

/**
 * 初始化线程上下文
 */
void hal_context_init(void* context, void* entry_point, void* stack_top);

/**
 * 保存上下文
 */
void hal_save_context(void* context);

/**
 * 恢复上下文
 */
void hal_restore_context(void* context);

/* ==================== 系统调用接口 ==================== */

/**
 * 执行系统调用
 */
void hal_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

/**
 * 系统调用返回
 */
void hal_syscall_return(u64 ret_val);

/* ==================== 设备接口 ==================== */

/**
 * 读取IO端口（如果支持）
 */
u8 hal_inb(u16 port);

/**
 * 写入IO端口（如果支持）
 */
void hal_outb(u16 port, u8 value);

/**
 * 读取IO端口（16位）
 */
u16 hal_inw(u16 port);

/**
 * 写入IO端口（16位）
 */
void hal_outw(u16 port, u16 value);

/**
 * 读取IO端口（32位）
 */
u32 hal_inl(u16 port);

/**
 * 写入IO端口（32位）
 */
void hal_outl(u16 port, u32 value);

/* ==================== 时间接口 ==================== */

/**
 * 延迟微秒
 */
void hal_udelay(u32 us);

/* ==================== 错误处理接口 ==================== */

/**
 * 触发异常
 */
void hal_trigger_exception(u32 exc_num);

/**
 * 停止CPU
 */
void hal_halt(void);

/**
 * 空转等待
 */
void hal_idle(void);

/* ==================== 调试接口 ==================== */

/**
 * 断点
 */
void hal_breakpoint(void);

/**
 * 栈回溯（调试用）
 */
void hal_stack_trace(void);

/* ==================== 架构检测和初始化 ==================== */

/**
 * 初始化HAL层
 * 在内核启动时调用，检测架构并初始化相关功能
 */
void hal_init(void);

/**
 * 获取架构名称
 */
const char* hal_get_arch_name(void);

/**
 * 获取页大小
 */
u64 hal_get_page_size(void);

/**
 * 检查当前架构是否支持IO端口
 */
bool hal_supports_io_ports(void);

#endif /* HIC_HAL_H */