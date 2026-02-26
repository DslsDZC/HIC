/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL (Compatibility Hardware Abstraction Layer) - 兼容性硬件抽象层
 * 
 * 本文件提供兼容性保证的硬件抽象接口，遵循以下原则：
 * - 简单：减少参数，简化使用流程
 * - 直观：使用直观的命名和参数
 * - 方便：提供便捷的封装，减少样板代码
 * - 兼容：保证API/ABI的向后兼容性
 * 
 * 版本: 1.0.0
 * 兼容性: HIC Core-0 v1.0+
 */

#ifndef HIC_CHAL_H
#define HIC_CHAL_H

#include "../types.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== CHAL版本信息 ==================== */

#define CHAL_VERSION_MAJOR  1
#define CHAL_VERSION_MINOR  0
#define CHAL_VERSION_PATCH  0

#define CHAL_VERSION_STRING "1.0.0"

/* ==================== CHAL设计原则 ==================== */

/**
 * 设计原则：
 * 1. 简单：函数参数不超过3个（特殊情况除外）
 * 2. 直观：函数名称清晰，参数名称直观
 * 3. 方便：提供便捷的默认值和简化函数
 * 4. 兼容：保证API/ABI的向后兼容性
 */

/* ==================== 错误代码 ==================== */

typedef enum {
    CHAL_OK = 0,                     /* 成功 */
    CHAL_ERR_INVALID_PARAM = 1,      /* 无效参数 */
    CHAL_ERR_NOT_SUPPORTED = 2,      /* 不支持的操作 */
    CHAL_ERR_TIMEOUT = 3,            /* 超时 */
    CHAL_ERR_BUSY = 4,               /* 资源忙 */
    CHAL_ERR_NO_MEMORY = 5,          /* 内存不足 */
} chal_error_t;

/**
 * 错误代码转字符串
 */
const char* chal_strerror(chal_error_t err);

/* ==================== 简化的错误处理 ==================== */

/**
 * 检查操作是否成功
 */
static inline bool chal_ok(chal_error_t err) {
    return err == CHAL_OK;
}

/**
 * 检查操作是否失败
 */
static inline bool chal_fail(chal_error_t err) {
    return err != CHAL_OK;
}

/**
 * 快速成功返回
 */
static inline chal_error_t chal_success(void) {
    return CHAL_OK;
}

/**
 * 快速失败返回
 */
static inline chal_error_t chal_fail_code(chal_error_t code) {
    return code;
}

/* ==================== 简化的内存操作 ==================== */

/**
 * @brief 读取8位值
 * 
 * @param addr 地址
 * @return 读取的值
 */
static inline u8 chal_read8(const volatile void* addr) {
    return *(volatile const u8*)addr;
}

/**
 * @brief 写入8位值
 * 
 * @param addr 地址
 * @param val 值
 */
static inline void chal_write8(volatile void* addr, u8 val) {
    *(volatile u8*)addr = val;
}

/**
 * @brief 读取16位值
 * 
 * @param addr 地址
 * @return 读取的值
 */
static inline u16 chal_read16(const volatile void* addr) {
    return *(volatile const u16*)addr;
}

/**
 * @brief 写入16位值
 * 
 * @param addr 地址
 * @param val 值
 */
static inline void chal_write16(volatile void* addr, u16 val) {
    *(volatile u16*)addr = val;
}

/**
 * @brief 读取32位值
 * 
 * @param addr 地址
 * @return 读取的值
 */
static inline u32 chal_read32(const volatile void* addr) {
    return *(volatile const u32*)addr;
}

/**
 * @brief 写入32位值
 * 
 * @param addr 地址
 * @param val 值
 */
static inline void chal_write32(volatile void* addr, u32 val) {
    *(volatile u32*)addr = val;
}

/**
 * @brief 读取64位值
 * 
 * @param addr 地址
 * @return 读取的值
 */
static inline u64 chal_read64(const volatile void* addr) {
    return *(volatile const u64*)addr;
}

/**
 * @brief 写入64位值
 * 
 * @param addr 地址
 * @param val 值
 */
static inline void chal_write64(volatile void* addr, u64 val) {
    *(volatile u64*)addr = val;
}

/**
 * @brief 内存填充（字节）
 * 
 * @param dst 目标地址
 * @param c 填充值
 * @param n 大小
 */
static inline void chal_memset(void* dst, u8 c, u64 n) {
    u8* d = (u8*)dst;
    for (u64 i = 0; i < n; i++) {
        d[i] = c;
    }
}

/**
 * @brief 内存拷贝
 * 
 * @param dst 目标地址
 * @param src 源地址
 * @param n 大小
 */
static inline void chal_memcpy(void* dst, const void* src, u64 n) {
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    for (u64 i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/**
 * @brief 内存比较
 * 
 * @param s1 地址1
 * @param s2 地址2
 * @param n 大小
 * @return 比较结果（0表示相等）
 */
static inline int chal_memcmp(const void* s1, const void* s2, u64 n) {
    const u8* p1 = (const u8*)s1;
    const u8* p2 = (const u8*)s2;
    for (u64 i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

/* ==================== 简化的IO操作 ==================== */

/**
 * @brief 读取IO端口8位
 * 
 * @param port 端口号
 * @return 读取的值
 */
static inline u8 chal_io_read8(u16 port) {
    return hal_inb(port);
}

/**
 * @brief 写入IO端口8位
 * 
 * @param port 端口号
 * @param val 值
 */
static inline void chal_io_write8(u16 port, u8 val) {
    hal_outb(port, val);
}

/**
 * @brief 读取IO端口16位
 * 
 * @param port 端口号
 * @return 读取的值
 */
static inline u16 chal_io_read16(u16 port) {
    return hal_inw(port);
}

/**
 * @brief 写入IO端口16位
 * 
 * @param port 端口号
 * @param val 值
 */
static inline void chal_io_write16(u16 port, u16 val) {
    hal_outw(port, val);
}

/**
 * @brief 读取IO端口32位
 * 
 * @param port 端口号
 * @return 读取的值
 */
static inline u32 chal_io_read32(u16 port) {
    return hal_inl(port);
}

/**
 * @brief 写入IO端口32位
 * 
 * @param port 端口号
 * @param val 值
 */
static inline void chal_io_write32(u16 port, u32 val) {
    hal_outl(port, val);
}

/* ==================== 简化的中断控制 ==================== */

/**
 * @brief 禁用中断（简化版）
 * 
 * @return 之前的中断状态
 */
static inline bool chal_irq_disable(void) {
    return hal_disable_interrupts();
}

/**
 * @brief 启用中断
 */
static inline void chal_irq_enable(void) {
    hal_enable_interrupts();
}

/**
 * @brief 恢复中断状态
 * 
 * @param state 之前的中断状态
 */
static inline void chal_irq_restore(bool state) {
    hal_restore_interrupts(state);
}

/**
 * @brief 中断作用域宏（RAII模式）
 */
#define CHAL_IRQ_SCOPE() \
    bool __irq_state = chal_irq_disable()

#define CHAL_IRQ_SCOPE_END() \
    chal_irq_restore(__irq_state)

/* ==================== 简化的内存屏障 ==================== */

/**
 * @brief 完整内存屏障
 */
static inline void chal_barrier(void) {
    hal_memory_barrier();
}

/**
 * @brief 读屏障
 */
static inline void chal_rbarrier(void) {
    hal_read_barrier();
}

/**
 * @brief 写屏障
 */
static inline void chal_wbarrier(void) {
    hal_write_barrier();
}

/* ==================== 简化的时间操作 ==================== */

/**
 * @brief 获取时间戳（纳秒）
 * 
 * @return 时间戳
 */
static inline u64 chal_time_now(void) {
    return hal_get_timestamp();
}

/**
 * @brief 延迟微秒
 * 
 * @param us 微秒数
 */
static inline void chal_time_delay_us(u32 us) {
    hal_udelay(us);
}

/**
 * @brief 延迟毫秒
 * 
 * @param ms 毫秒数
 */
static inline void chal_time_delay_ms(u32 ms) {
    hal_udelay(ms * 1000);
}

/**
 * @brief 延迟秒
 * 
 * @param s 秒数
 */
static inline void chal_time_delay_s(u32 s) {
    hal_udelay(s * 1000000);
}

/* ==================== 简化的页操作 ==================== */

#define CHAL_PAGE_SIZE    4096
#define CHAL_PAGE_SHIFT   12
#define CHAL_PAGE_MASK    (CHAL_PAGE_SIZE - 1)

/**
 * @brief 页对齐
 * 
 * @param addr 地址
 * @return 对齐后的地址
 */
static inline u64 chal_page_align(u64 addr) {
    return (addr + CHAL_PAGE_SIZE - 1) & ~(u64)(CHAL_PAGE_SIZE - 1);
}

/**
 * @brief 页下对齐
 * 
 * @param addr 地址
 * @return 下对齐的地址
 */
static inline u64 chal_page_floor(u64 addr) {
    return addr & ~(u64)(CHAL_PAGE_SIZE - 1);
}

/**
 * @brief 检查是否页对齐
 * 
 * @param addr 地址
 * @return 是否对齐
 */
static inline bool chal_page_is_aligned(u64 addr) {
    return (addr & CHAL_PAGE_MASK) == 0;
}

/**
 * @brief 计算页号
 * 
 * @param addr 地址
 * @return 页号
 */
static inline u64 chal_page_number(u64 addr) {
    return addr >> CHAL_PAGE_SHIFT;
}

/**
 * @brief 页号转地址
 * 
 * @param page_num 页号
 * @return 地址
 */
static inline u64 chal_page_address(u64 page_num) {
    return page_num << CHAL_PAGE_SHIFT;
}

/**
 * @brief 计算页数
 * 
 * @param size 大小
 * @return 页数
 */
static inline u64 chal_page_count(u64 size) {
    return (size + CHAL_PAGE_SIZE - 1) >> CHAL_PAGE_SHIFT;
}

/* ==================== 简化的位操作 ==================== */

/**
 * @brief 设置位
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 */
static inline void chal_bit_set(volatile u32* reg, u32 bit) {
    *reg |= (1U << bit);
}

/**
 * @brief 清除位
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 */
static inline void chal_bit_clear(volatile u32* reg, u32 bit) {
    *reg &= ~(1U << bit);
}

/**
 * @brief 翻转位
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 */
static inline void chal_bit_toggle(volatile u32* reg, u32 bit) {
    *reg ^= (1U << bit);
}

/**
 * @brief 测试位
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 * @return 位值
 */
static inline bool chal_bit_test(const volatile u32* reg, u32 bit) {
    return (*reg & (1U << bit)) != 0;
}

/**
 * @brief 设置多个位
 * 
 * @param reg 寄存器地址
 * @param mask 位掩码
 */
static inline void chal_bits_set(volatile u32* reg, u32 mask) {
    *reg |= mask;
}

/**
 * @brief 清除多个位
 * 
 * @param reg 寄存器地址
 * @param mask 位掩码
 */
static inline void chal_bits_clear(volatile u32* reg, u32 mask) {
    *reg &= ~mask;
}

/**
 * @brief 测试多个位
 * 
 * @param reg 寄存器地址
 * @param mask 位掩码
 * @return 是否所有位都设置
 */
static inline bool chal_bits_test_all(const volatile u32* reg, u32 mask) {
    return (*reg & mask) == mask;
}

/**
 * @brief 测试多个位（任意一个）
 * 
 * @param reg 寄存器地址
 * @param mask 位掩码
 * @return 是否任意一个位设置
 */
static inline bool chal_bits_test_any(const volatile u32* reg, u32 mask) {
    return (*reg & mask) != 0;
}

/* ==================== 简化的原子操作 ==================== */

/**
 * @brief 原子读取32位
 * 
 * @param addr 地址
 * @return 值
 */
static inline u32 chal_atomic_read32(volatile u32* addr) {
    return *addr;
}

/**
 * @brief 原子写入32位
 * 
 * @param addr 地址
 * @param val 值
 */
static inline void chal_atomic_write32(volatile u32* addr, u32 val) {
    *addr = val;
}

/**
 * @brief 原子加
 * 
 * @param addr 地址
 * @param val 增加值
 * @return 新值
 */
static inline u32 chal_atomic_add32(volatile u32* addr, u32 val) {
    u32 old = *addr;
    *addr = old + val;
    return old + val;
}

/**
 * @brief 原子减
 * 
 * @param addr 地址
 * @param val 减少值
 * @return 新值
 */
static inline u32 chal_atomic_sub32(volatile u32* addr, u32 val) {
    u32 old = *addr;
    *addr = old - val;
    return old - val;
}

/**
 * @brief 原子设置位
 * 
 * @param addr 地址
 * @param bit 位号
 * @return 旧值
 */
static inline u32 chal_atomic_set_bit32(volatile u32* addr, u32 bit) {
    u32 old = *addr;
    *addr = old | (1U << bit);
    return old;
}

/**
 * @brief 原子清除位
 * 
 * @param addr 地址
 * @param bit 位号
 * @return 旧值
 */
static inline u32 chal_atomic_clear_bit32(volatile u32* addr, u32 bit) {
    u32 old = *addr;
    *addr = old & ~(1U << bit);
    return old;
}

/**
 * @brief 原子比较交换
 * 
 * @param addr 地址
 * @param expected 期望值
 * @param desired 新值
 * @return 是否成功
 */
static inline bool chal_atomic_cas32(volatile u32* addr, u32 expected, u32 desired) {
    if (*addr == expected) {
        *addr = desired;
        return true;
    }
    return false;
}

/* ==================== 简化的CPU操作 ==================== */

/**
 * @brief 停止CPU
 */
static inline void chal_cpu_halt(void) {
    hal_halt();
}

/**
 * @brief 空转等待
 */
static inline void chal_cpu_idle(void) {
    hal_idle();
}

/**
 * @brief 断点
 */
static inline void chal_cpu_breakpoint(void) {
    hal_breakpoint();
}

/**
 * @brief CPU指令屏障
 */
static inline void chal_cpu_fence(void) {
    __asm__ __volatile__("" ::: "memory");
}

/* ==================== 简化的内存映射 ==================== */

/**
 * @brief 物理地址转虚拟地址
 * 
 * @param phys 物理地址
 * @return 虚拟地址
 */
static inline void* chal_mem_phys_to_virt(phys_addr_t phys) {
    return hal_phys_to_virt(phys);
}

/**
 * @brief 虚拟地址转物理地址
 * 
 * @param virt 虚拟地址
 * @return 物理地址
 */
static inline phys_addr_t chal_mem_virt_to_phys(void* virt) {
    return hal_virt_to_phys(virt);
}

/* ==================== 简化的版本管理 ==================== */

/**
 * @brief 获取CHAL版本
 * 
 * @return 版本号（主版本<<16 | 次版本<<8 | 补丁版本）
 */
static inline u32 chal_get_version(void) {
    return (CHAL_VERSION_MAJOR << 16) | (CHAL_VERSION_MINOR << 8) | CHAL_VERSION_PATCH;
}

/**
 * @brief 检查版本兼容性
 * 
 * @param required_major 需要的主版本
 * @param required_minor 需要的次版本
 * @return 是否兼容
 */
static inline bool chal_is_version_compatible(u32 required_major, u32 required_minor) {
    if (CHAL_VERSION_MAJOR > required_major) {
        return true;  /* 新主版本通常向后兼容 */
    }
    if (CHAL_VERSION_MAJOR == required_major && CHAL_VERSION_MINOR >= required_minor) {
        return true;  /* 同主版本，次版本更高兼容 */
    }
    return false;
}

/* ==================== 简化的断言和调试 ==================== */

/**
 * @brief 断言宏
 */
#ifdef CHAL_DEBUG
#define CHAL_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            chal_cpu_breakpoint(); \
        } \
    } while(0)
#else
#define CHAL_ASSERT(condition) ((void)0)
#endif

/**
 * @brief 不可达宏
 */
#define CHAL_UNREACHABLE() \
    do { \
        CHAL_ASSERT(0); \
        chal_cpu_halt(); \
    } while(0)

/**
 * @brief 必须成功宏
 */
#define CHAL_MUST_SUCCESS(expr) \
    do { \
        chal_error_t _err = (expr); \
        if (chal_fail(_err)) { \
            CHAL_ASSERT(0); \
            chal_cpu_halt(); \
        } \
    } while(0)

/* ==================== C++便利包装 ==================== */

#ifdef __cplusplus
namespace chal {
    // 内存操作
    inline u8 read8(const volatile void* addr) { return chal_read8(addr); }
    inline void write8(volatile void* addr, u8 val) { chal_write8(addr, val); }
    inline u16 read16(const volatile void* addr) { return chal_read16(addr); }
    inline void write16(volatile void* addr, u16 val) { chal_write16(addr, val); }
    inline u32 read32(const volatile void* addr) { return chal_read32(addr); }
    inline void write32(volatile void* addr, u32 val) { chal_write32(addr, val); }
    inline u64 read64(const volatile void* addr) { return chal_read64(addr); }
    inline void write64(volatile void* addr, u64 val) { chal_write64(addr, val); }
    
    // IO操作
    inline u8 io_read8(u16 port) { return chal_io_read8(port); }
    inline void io_write8(u16 port, u8 val) { chal_io_write8(port, val); }
    inline u16 io_read16(u16 port) { return chal_io_read16(port); }
    inline void io_write16(u16 port, u16 val) { chal_io_write16(port, val); }
    inline u32 io_read32(u16 port) { return chal_io_read32(port); }
    inline void io_write32(u16 port, u32 val) { chal_io_write32(port, val); }
    
    // 中断控制
    inline bool irq_disable(void) { return chal_irq_disable(); }
    inline void irq_enable(void) { chal_irq_enable(); }
    inline void irq_restore(bool state) { chal_irq_restore(state); }
    
    // 内存屏障
    inline void barrier(void) { chal_barrier(); }
    inline void rbarrier(void) { chal_rbarrier(); }
    inline void wbarrier(void) { chal_wbarrier(); }
    
    // 时间操作
    inline u64 time_now(void) { return chal_time_now(); }
    inline void time_delay_us(u32 us) { chal_time_delay_us(us); }
    inline void time_delay_ms(u32 ms) { chal_time_delay_ms(ms); }
    inline void time_delay_s(u32 s) { chal_time_delay_s(s); }
    
    // 页操作
    inline u64 page_align(u64 addr) { return chal_page_align(addr); }
    inline u64 page_floor(u64 addr) { return chal_page_floor(addr); }
    inline bool page_is_aligned(u64 addr) { return chal_page_is_aligned(addr); }
    inline u64 page_number(u64 addr) { return chal_page_number(addr); }
    inline u64 page_address(u64 page_num) { return chal_page_address(page_num); }
    inline u64 page_count(u64 size) { return chal_page_count(size); }
    
    // 位操作
    inline void bit_set(volatile u32* reg, u32 bit) { chal_bit_set(reg, bit); }
    inline void bit_clear(volatile u32* reg, u32 bit) { chal_bit_clear(reg, bit); }
    inline void bit_toggle(volatile u32* reg, u32 bit) { chal_bit_toggle(reg, bit); }
    inline bool bit_test(const volatile u32* reg, u32 bit) { return chal_bit_test(reg, bit); }
    
    // CPU操作
    inline void cpu_halt(void) { chal_cpu_halt(); }
    inline void cpu_idle(void) { chal_cpu_idle(); }
    inline void cpu_breakpoint(void) { chal_cpu_breakpoint(); }
    inline void cpu_fence(void) { chal_cpu_fence(); }
    
    // 错误检查
    inline bool ok(chal_error_t err) { return chal_ok(err); }
    inline bool fail(chal_error_t err) { return chal_fail(err); }
    inline chal_error_t success(void) { return chal_success(); }
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* HIC_CHAL_H */