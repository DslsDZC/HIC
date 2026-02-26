/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL (Compatibility Hardware Abstraction Layer) - 兼容性硬件抽象层实现
 */

#include "include/chal.h"
#include "lib/console.h"
#include <stddef.h>

/* ==================== 错误处理 ==================== */

const char* chal_strerror(chal_error_t err)
{
    switch (err) {
        case CHAL_OK:
            return "Success";
        case CHAL_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case CHAL_ERR_NOT_SUPPORTED:
            return "Not supported";
        case CHAL_ERR_TIMEOUT:
            return "Timeout";
        case CHAL_ERR_BUSY:
            return "Resource busy";
        case CHAL_ERR_NO_MEMORY:
            return "No memory";
        default:
            return "Unknown error";
    }
}

/* ==================== 初始化 ==================== */

/**
 * @brief 初始化CHAL层
 * 
 * @return CHAL_OK 成功，其他值表示错误
 */
chal_error_t chal_init(void)
{
    console_puts("[CHAL] Compatibility HAL v");
    console_puts(CHAL_VERSION_STRING);
    console_puts(" initialized\n");
    
    return CHAL_OK;
}

/**
 * @brief 获取CHAL信息
 * 
 * @return CHAL版本字符串
 */
const char* chal_get_info(void)
{
    return "CHAL (Compatibility Hardware Abstraction Layer) v" CHAL_VERSION_STRING;
}

/* ==================== 架构兼容性检查 ==================== */

/**
 * @brief 检查架构是否支持
 * 
 * @param arch_type 架构类型
 * @return 是否支持
 */
bool chal_is_arch_supported(hal_arch_type_t arch_type)
{
    hal_arch_type_t current = hal_get_arch_type();
    return current == arch_type;
}

/**
 * @brief 获取支持的架构列表
 * 
 * @param arch_list 架构列表输出
 * @param max_count 最大数量
 * @return 实际数量
 */
u32 chal_get_supported_archs(hal_arch_type_t* arch_list, u32 max_count)
{
    if (!arch_list || max_count == 0) {
        return 0;
    }
    
    u32 count = 0;
    
    /* x86_64 */
    if (count < max_count) {
        arch_list[count++] = HAL_ARCH_X86_64;
    }
    
    /* ARM64 */
    if (count < max_count) {
        arch_list[count++] = HAL_ARCH_ARM64;
    }
    
    /* RISC-V64 */
    if (count < max_count) {
        arch_list[count++] = HAL_ARCH_RISCV64;
    }
    
    return count;
}

/* ==================== 功能支持检查 ==================== */

/**
 * @brief 检查是否支持IO端口
 * 
 * @return 是否支持
 */
bool chal_supports_io_ports(void)
{
    return hal_supports_io_ports();
}

/**
 * @brief 检查是否支持原子操作
 * 
 * @return 是否支持
 */
bool chal_supports_atomic_ops(void)
{
    /* 所有支持的架构都支持原子操作 */
    return true;
}

/**
 * @brief 检查是否支持内存屏障
 * 
 * @return 是否支持
 */
bool chal_supports_memory_barriers(void)
{
    /* 所有支持的架构都支持内存屏障 */
    return true;
}

/* ==================== 兼容性包装函数 ==================== */

/**
 * @brief 安全读取32位（带错误检查）
 * 
 * @param addr 地址
 * @param out_val 输出值
 * @return CHAL_OK 成功，其他值表示错误
 */
chal_error_t chal_safe_read32(const volatile void* addr, u32* out_val)
{
    if (!addr || !out_val) {
        return CHAL_ERR_INVALID_PARAM;
    }
    
    *out_val = chal_read32(addr);
    return CHAL_OK;
}

/**
 * @brief 安全写入32位（带错误检查）
 * 
 * @param addr 地址
 * @param val 值
 * @return CHAL_OK 成功，其他值表示错误
 */
chal_error_t chal_safe_write32(volatile void* addr, u32 val)
{
    if (!addr) {
        return CHAL_ERR_INVALID_PARAM;
    }
    
    chal_write32(addr, val);
    return CHAL_OK;
}

/**
 * @brief 等待条件（带超时）
 * 
 * @param condition 条件函数
 * @param timeout_us 超时时间（微秒）
 * @return CHAL_OK 成功，CHAL_ERR_TIMEOUT 超时
 */
chal_error_t chal_wait_until(bool (*condition)(void), u32 timeout_us)
{
    u64 start = chal_time_now();
    
    while (!condition()) {
        u64 elapsed = chal_time_now() - start;
        if (elapsed >= timeout_us) {
            return CHAL_ERR_TIMEOUT;
        }
        chal_cpu_idle();
    }
    
    return CHAL_OK;
}

/**
 * @brief 等待位被设置
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 * @param timeout_us 超时时间（微秒）
 * @return CHAL_OK 成功，CHAL_ERR_TIMEOUT 超时
 */
chal_error_t chal_wait_for_bit_set(volatile u32* reg, u32 bit, u32 timeout_us)
{
    u64 start = chal_time_now();
    
    while (!chal_bit_test(reg, bit)) {
        u64 elapsed = chal_time_now() - start;
        if (elapsed >= timeout_us) {
            return CHAL_ERR_TIMEOUT;
        }
        chal_cpu_idle();
    }
    
    return CHAL_OK;
}

/**
 * @brief 等待位被清除
 * 
 * @param reg 寄存器地址
 * @param bit 位号
 * @param timeout_us 超时时间（微秒）
 * @return CHAL_OK 成功，CHAL_ERR_TIMEOUT 超时
 */
chal_error_t chal_wait_for_bit_clear(volatile u32* reg, u32 bit, u32 timeout_us)
{
    u64 start = chal_time_now();
    
    while (chal_bit_test(reg, bit)) {
        u64 elapsed = chal_time_now() - start;
        if (elapsed >= timeout_us) {
            return CHAL_ERR_TIMEOUT;
        }
        chal_cpu_idle();
    }
    
    return CHAL_OK;
}

/* ==================== 高级内存操作 ==================== */

/**
 * @brief 检查地址范围是否有效
 * 
 * @param base 基地址
 * @param size 大小
 * @return 是否有效
 */
bool chal_is_valid_range(void* base, u64 size)
{
    /* 简单检查：非空且不为零大小 */
    return base != NULL && size > 0;
}

/**
 * @brief 检查地址范围是否重叠
 * 
 * @param base1 地址1
 * @param size1 大小1
 * @param base2 地址2
 * @param size2 大小2
 * @return 是否重叠
 */
bool chal_ranges_overlap(void* base1, u64 size1, void* base2, u64 size2)
{
    if (!base1 || !base2 || size1 == 0 || size2 == 0) {
        return false;
    }
    
    u64 addr1 = (u64)base1;
    u64 addr2 = (u64)base2;
    
    u64 end1 = addr1 + size1 - 1;
    u64 end2 = addr2 + size2 - 1;
    
    return !(end1 < addr2 || end2 < addr1);
}

/**
 * @brief 对齐地址
 * 
 * @param addr 地址
 * @param alignment 对齐字节数
 * @return 对齐后的地址
 */
u64 chal_align_address(u64 addr, u64 alignment)
{
    if (alignment == 0) {
        return addr;
    }
    
    return (addr + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief 下对齐地址
 * 
 * @param addr 地址
 * @param alignment 对齐字节数
 * @return 下对齐的地址
 */
u64 chal_floor_address(u64 addr, u64 alignment)
{
    if (alignment == 0) {
        return addr;
    }
    
    return addr & ~(alignment - 1);
}

/**
 * @brief 检查地址是否对齐
 * 
 * @param addr 地址
 * @param alignment 对齐字节数
 * @return 是否对齐
 */
bool chal_is_address_aligned(u64 addr, u64 alignment)
{
    if (alignment == 0) {
        return true;
    }
    
    return (addr & (alignment - 1)) == 0;
}

/* ==================== 调试辅助函数 ==================== */

/**
 * @brief 打印寄存器值（调试用）
 * 
 * @param name 寄存器名称
 * @param reg 寄存器地址
 */
void chal_debug_print_reg(const char* name, volatile u32* reg)
{
    if (!name || !reg) {
        return;
    }
    
    console_puts("[CHAL] DEBUG: ");
    console_puts(name);
    console_puts(" = 0x");
    
    u32 val = *reg;
    
    /* 简单的十六进制打印 */
    for (int i = 28; i >= 0; i -= 4) {
        u32 digit = (val >> i) & 0xF;
        if (digit < 10) {
            console_putchar((char)('0' + digit));
        } else {
            console_putchar((char)('A' + digit - 10));
        }
    }
    
    console_puts("\n");
}

/**
 * @brief 打印内存范围（调试用）
 * 
 * @param base 基地址
 * @param size 大小
 */
void chal_debug_print_memory(void* base, u64 size)
{
    if (!base || size == 0) {
        return;
    }
    
    console_puts("[CHAL] DEBUG: Memory dump at 0x");
    
    u64 addr = (u64)base;
    
    /* 打印地址 */
    for (int i = 60; i >= 0; i -= 4) {
        u32 digit = (addr >> i) & 0xF;
        if (digit < 10) {
            console_putchar((char)('0' + digit));
        } else {
            console_putchar((char)('A' + digit - 10));
        }
    }
    
    console_puts(", size = ");
    console_putu64(size);
    console_puts("\n");
}

/* ==================== 性能统计 ==================== */

typedef struct {
    u64 total_calls;
    u64 total_time_ns;
    u64 max_time_ns;
    u64 min_time_ns;
} chal_perf_counter_t;

static chal_perf_counter_t g_perf_counters[32];

/**
 * @brief 性能测量开始
 * 
 * @param counter_id 计数器ID
 */
static inline void chal_perf_start(u32 counter_id)
{
    if (counter_id >= 32) {
        return;
    }
    
    /* 简化实现：只调用开始时间 */
    (void)counter_id;
    chal_time_now();
}

/**
 * @brief 性能测量结束
 * 
 * @param counter_id 计数器ID
 */
static inline void chal_perf_end(u32 counter_id)
{
    if (counter_id >= 32) {
        return;
    }
    
    /* 简化实现：只调用结束时间 */
    (void)counter_id;
    chal_time_now();
}

/**
 * @brief 重置性能计数器
 * 
 * @param counter_id 计数器ID
 */
void chal_perf_reset(u32 counter_id)
{
    if (counter_id >= 32) {
        return;
    }
    
    chal_perf_counter_t* counter = &g_perf_counters[counter_id];
    counter->total_calls = 0;
    counter->total_time_ns = 0;
    counter->max_time_ns = 0;
    counter->min_time_ns = 0xFFFFFFFFFFFFFFFFULL;
}

/**
 * @brief 获取性能统计
 * 
 * @param counter_id 计数器ID
 * @param out_calls 输出调用次数
 * @param out_avg_time 输出平均时间（纳秒）
 * @return CHAL_OK 成功，其他值表示错误
 */
chal_error_t chal_perf_get_stats(u32 counter_id, u64* out_calls, u64* out_avg_time)
{
    if (counter_id >= 32 || !out_calls || !out_avg_time) {
        return CHAL_ERR_INVALID_PARAM;
    }
    
    chal_perf_counter_t* counter = &g_perf_counters[counter_id];
    
    *out_calls = counter->total_calls;
    *out_avg_time = (counter->total_calls > 0) ? 
                    (counter->total_time_ns / counter->total_calls) : 0;
    
    return CHAL_OK;
}

/* ==================== 版本兼容性 ==================== */

/**
 * @brief 获取API版本
 * 
 * @return API版本号
 */
u32 chal_get_api_version(void)
{
    return chal_get_version();
}

/**
 * @brief 获取ABI版本
 * 
 * @return ABI版本号
 */
u32 chal_get_abi_version(void)
{
    /* ABI版本与API版本相同 */
    return chal_get_version();
}

/**
 * @brief 检查API兼容性
 * 
 * @param required_version 需要的版本
 * @return 是否兼容
 */
bool chal_check_api_compatibility(u32 required_version)
{
    u32 current_version = chal_get_api_version();
    u32 required_major = (required_version >> 16) & 0xFF;
    u32 required_minor = (required_version >> 8) & 0xFF;
    
    return chal_is_version_compatible(required_major, required_minor);
}

/**
 * @brief 检查ABI兼容性
 * 
 * @param required_version 需要的版本
 * @return 是否兼容
 */
bool chal_check_abi_compatibility(u32 required_version)
{
    u32 current_version = chal_get_abi_version();
    u32 required_major = (required_version >> 16) & 0xFF;
    
    /* ABI版本要求主版本完全相同 */
    return (current_version >> 16) == required_major;
}
