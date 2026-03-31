/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL (Core Hardware Abstraction Layer) - 服务侧共享库接口
 * 
 * CHAL 作为共享库（.hiclib）实现，服务通过 lib_manager 加载后直接调用。
 * 
 * 使用流程：
 * 1. 服务启动时调用 lib_lookup("libchal") 获取库能力
 * 2. 映射库代码段到服务地址空间
 * 3. 通过符号表获取函数指针
 * 4. 直接调用函数（同地址空间，零开销）
 * 
 * 兼容层设计原则：
 * - 共享库方式：有 MMU 时，多服务共享同一物理代码段
 * - 静态链接方式：无 MMU 时，兼容层代码静态链接到服务
 */

#ifndef HIC_SERVICE_CHAL_H
#define HIC_SERVICE_CHAL_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "hiclib.h"

/* ==================== 库标识 ==================== */

/* CHAL 库 UUID (生成: 2026-03-31) */
#define CHAL_LIB_UUID \
    { 0xC1, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
      0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#define CHAL_LIB_NAME    "libchal"
#define CHAL_LIB_VERSION HICLIB_PACK_VERSION(1, 0, 0)

/* ==================== 内存屏障接口 ==================== */

/**
 * 完整内存屏障
 * 确保所有读写操作都完成
 */
void chal_memory_barrier(void);

/**
 * 读屏障
 * 确保所有读操作完成
 */
void chal_read_barrier(void);

/**
 * 写屏障
 * 确保所有写操作完成
 */
void chal_write_barrier(void);

/* ==================== 原子操作接口 ==================== */

/**
 * 原子比较并交换（32位）
 * @param addr 目标地址
 * @param expected 期望值
 * @param desired 新值
 * @return true 如果交换成功
 */
bool chal_atomic_cas32(volatile uint32_t *addr, uint32_t expected, uint32_t desired);

/**
 * 原子比较并交换（64位）
 */
bool chal_atomic_cas64(volatile uint64_t *addr, uint64_t expected, uint64_t desired);

/**
 * 原子加法
 * @param addr 目标地址
 * @param value 加数值
 * @return 加之前的值
 */
uint32_t chal_atomic_add32(volatile uint32_t *addr, uint32_t value);
uint64_t chal_atomic_add64(volatile uint64_t *addr, uint64_t value);

/**
 * 原子交换
 * @param addr 目标地址
 * @param value 新值
 * @return 交换前的值
 */
uint32_t chal_atomic_xchg32(volatile uint32_t *addr, uint32_t value);
uint64_t chal_atomic_xchg64(volatile uint64_t *addr, uint64_t value);

/* ==================== 内存操作接口 ==================== */

/**
 * 内存拷贝
 */
void chal_memcpy(void *dst, const void *src, size_t size);

/**
 * 内存设置
 */
void chal_memset(void *dst, int value, size_t size);

/**
 * 内存比较
 */
int chal_memcmp(const void *s1, const void *s2, size_t size);

/**
 * 内存清零
 */
void chal_memzero(void *dst, size_t size);

/* ==================== MMIO 接口 ==================== */

/**
 * MMIO 读取（8位）
 */
uint8_t chal_read8(const volatile void *addr);

/**
 * MMIO 读取（16位）
 */
uint16_t chal_read16(const volatile void *addr);

/**
 * MMIO 读取（32位）
 */
uint32_t chal_read32(const volatile void *addr);

/**
 * MMIO 读取（64位）
 */
uint64_t chal_read64(const volatile void *addr);

/**
 * MMIO 写入（8位）
 */
void chal_write8(volatile void *addr, uint8_t value);

/**
 * MMIO 写入（16位）
 */
void chal_write16(volatile void *addr, uint16_t value);

/**
 * MMIO 写入（32位）
 */
void chal_write32(volatile void *addr, uint32_t value);

/**
 * MMIO 写入（64位）
 */
void chal_write64(volatile void *addr, uint64_t value);

/* ==================== IO 端口接口（x86_64） ==================== */

/**
 * 读取 IO 端口（8位）
 * @param port 端口号
 * @return 读取值
 */
uint8_t chal_inb(uint16_t port);

/**
 * 读取 IO 端口（16位）
 */
uint16_t chal_inw(uint16_t port);

/**
 * 读取 IO 端口（32位）
 */
uint32_t chal_inl(uint16_t port);

/**
 * 写入 IO 端口（8位）
 */
void chal_outb(uint16_t port, uint8_t value);

/**
 * 写入 IO 端口（16位）
 */
void chal_outw(uint16_t port, uint16_t value);

/**
 * 写入 IO 端口（32位）
 */
void chal_outl(uint16_t port, uint32_t value);

/* ==================== 中断控制接口 ==================== */

/**
 * 禁用中断
 * @return 之前的中断状态
 */
bool chal_disable_interrupts(void);

/**
 * 启用中断
 */
void chal_enable_interrupts(void);

/**
 * 恢复中断状态
 */
void chal_restore_interrupts(bool state);

/* ==================== 时间接口 ==================== */

/**
 * 获取时间戳（纳秒）
 */
uint64_t chal_get_timestamp(void);

/**
 * 微秒延迟
 */
void chal_udelay(uint32_t us);

/**
 * 毫秒延迟
 */
void chal_mdelay(uint32_t ms);

/* ==================== 缓存操作接口 ==================== */

/**
 * 刷新数据缓存
 */
void chal_cache_flush(void *addr, size_t size);

/**
 * 使缓存无效
 */
void chal_cache_invalidate(void *addr, size_t size);

/**
 * 预取缓存
 */
void chal_cache_prefetch(const void *addr);

/* ==================== 串口接口 ==================== */

/**
 * 串口输出字符
 */
void chal_uart_putc(char c);

/**
 * 串口输出字符串
 */
void chal_uart_puts(const char *str);

/**
 * 串口接收字符（阻塞）
 */
char chal_uart_getc(void);

/**
 * 检查串口是否有数据
 */
bool chal_uart_rx_ready(void);

/* ==================== 系统信息接口 ==================== */

/**
 * 获取当前 CPU ID
 */
uint32_t chal_get_cpu_id(void);

/**
 * 获取页大小
 */
uint32_t chal_get_page_size(void);

/* ==================== 错误码 ==================== */

typedef enum {
    CHAL_OK = 0,
    CHAL_ERR_INVALID_PARAM = 1,
    CHAL_ERR_NOT_SUPPORTED = 2,
    CHAL_ERR_PERMISSION = 3,
    CHAL_ERR_IO_ERROR = 4,
} chal_error_t;

/* ==================== 辅助宏 ==================== */

#define CHAL_ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define CHAL_ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define CHAL_IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

/* ==================== 库加载辅助函数 ==================== */

/**
 * CHAL 库句柄（不透明类型）
 */
typedef struct chal_lib_handle chal_lib_handle_t;

/**
 * 加载 CHAL 共享库
 * @return 库句柄，失败返回 NULL
 * 
 * 内部调用 lib_manager 获取库能力并映射到地址空间
 */
chal_lib_handle_t *chal_lib_load(void);

/**
 * 卸载 CHAL 共享库
 */
void chal_lib_unload(chal_lib_handle_t *handle);

/**
 * 获取库版本
 */
uint32_t chal_lib_get_version(chal_lib_handle_t *handle);

#endif /* HIC_SERVICE_CHAL_H */
