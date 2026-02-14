/**
 * HIK硬件抽象层 (HAL) - 架构无关接口
 * 
 * 本文件提供架构无关的接口，用于核心代码
 * 核心代码应该只包含此文件，不应直接包含arch.h
 */

#ifndef HIK_HAL_H
#define HIK_HAL_H

#include "types.h"

/* ==================== 内存屏障接口 ==================== */

/**
 * 完整内存屏障
 * 确保所有读写操作都完成
 */
static inline void hal_memory_barrier(void) {
    __asm__ volatile("" ::: "memory");  /* 编译器屏障 */
    /* 架构特定屏障由arch.h提供 */
}

/**
 * 读屏障
 * 确保所有读操作完成
 */
static inline void hal_read_barrier(void) {
    __asm__ volatile("" ::: "memory");
}

/**
 * 写屏障
 * 确保所有写操作完成
 */
static inline void hal_write_barrier(void) {
    __asm__ volatile("" ::: "memory");
}

/* ==================== 中断控制接口 ==================== */

/**
 * 禁用中断
 * 返回之前的中断状态
 */
static inline bool hal_disable_interrupts(void) {
    extern bool arch_disable_interrupts_save(void);
    return arch_disable_interrupts_save();
}

/**
 * 启用中断
 */
static inline void hal_enable_interrupts(void) {
    extern void arch_enable_interrupts_restore(bool state);
    arch_enable_interrupts_restore(false);
}

/**
 * 恢复中断状态
 */
static inline void hal_restore_interrupts(bool state) {
    extern void arch_enable_interrupts_restore(bool state);
    arch_enable_interrupts_restore(state);
}

/* ==================== 时间接口 ==================== */

/**
 * 获取当前时间戳
 * 返回纳秒级时间戳
 */
static inline u64 hal_get_timestamp(void) {
    extern u64 arch_get_timestamp(void);
    return arch_get_timestamp();
}

/**
 * 延迟微秒
 */
void hal_udelay(u32 us);

/* ==================== 特权级接口 ==================== */

/**
 * 检查是否在内核态
 */
static inline bool hal_is_kernel_mode(void) {
    extern u32 arch_get_privilege_level(void);
    return arch_get_privilege_level() == 0;
}

/* ==================== 页大小接口 ==================== */

#define HAL_PAGE_SIZE       4096
#define HAL_PAGE_SHIFT      12
#define HAL_PAGE_MASK       (HAL_PAGE_SIZE - 1)

/**
 * 页对齐
 */
static inline u64 hal_page_align(u64 addr) {
    return (addr + HAL_PAGE_SIZE - 1) & ~(HAL_PAGE_SIZE - 1);
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
static inline void* hal_phys_to_virt(phys_addr_t phys) {
    return (void*)phys;
}

/**
 * 虚拟地址到物理地址映射（恒等映射）
 */
static inline phys_addr_t hal_virt_to_phys(void* virt) {
    return (phys_addr_t)virt;
}

/* ==================== 上下文接口 ==================== */

/**
 * 切换到线程上下文
 */
void hal_context_switch(void* prev, void* next);

/**
 * 初始化线程上下文
 */
void hal_context_init(void* context, void* entry_point, void* stack_top);

/* ==================== 系统调用接口 ==================== */

/**
 * 执行系统调用
 */
void hal_syscall_invoke(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

/* ==================== 设备接口 ==================== */

/**
 * 读取IO端口（如果支持）
 */
u8 hal_inb(u16 port);

/**
 * 写入IO端口（如果支持）
 */
void hal_outb(u16 port, u8 value);

/* ==================== 错误处理接口 ==================== */

/**
 * 触发异常
 */
void hal_trigger_exception(u32 exc_num);

/**
 * 停机
 */
static inline void hal_halt(void) {
    extern void arch_halt(void);
    arch_halt();
}

/* ==================== 调试接口 ==================== */

/**
 * 断点
 */
static inline void hal_breakpoint(void) {
#if defined(__x86_64__)
    __asm__ volatile("int3");
#elif defined(__aarch64__)
    __asm__ volatile("brk 0");
#elif defined(__riscv)
    __asm__ volatile("ebreak");
#endif
}

/**
 * 栈回溯（调试用）
 */
void hal_stack_trace(void);

#endif /* HIK_HAL_H */