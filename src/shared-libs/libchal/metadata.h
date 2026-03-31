/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL 库元数据定义
 * 用于构建 hiclib 格式
 */

#ifndef CHAL_METADATA_H
#define CHAL_METADATA_H

/* 库标识 */
#define CHAL_LIB_NAME       "libchal"
#define CHAL_LIB_DISPLAY    "Core Hardware Abstraction Layer"

/* 版本号 */
#define CHAL_VERSION_MAJOR  1
#define CHAL_VERSION_MINOR  0
#define CHAL_VERSION_PATCH  0

/* 库 UUID (v4 variant) */
#define CHAL_LIB_UUID_BYTES \
    0xC1, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01

/* 架构标识 */
#if defined(__x86_64__)
#define CHAL_LIB_ARCH 1
#elif defined(__aarch64__)
#define CHAL_LIB_ARCH 2
#elif defined(__riscv) && (__riscv_xlen == 64)
#define CHAL_LIB_ARCH 3
#else
#define CHAL_LIB_ARCH 0
#endif

/* 导出符号列表 */
#define CHAL_EXPORT_SYMBOLS \
    EXPORT_SYM(chal_memory_barrier) \
    EXPORT_SYM(chal_read_barrier) \
    EXPORT_SYM(chal_write_barrier) \
    EXPORT_SYM(chal_memcpy) \
    EXPORT_SYM(chal_memset) \
    EXPORT_SYM(chal_memcmp) \
    EXPORT_SYM(chal_memzero) \
    EXPORT_SYM(chal_read8) \
    EXPORT_SYM(chal_read16) \
    EXPORT_SYM(chal_read32) \
    EXPORT_SYM(chal_read64) \
    EXPORT_SYM(chal_write8) \
    EXPORT_SYM(chal_write16) \
    EXPORT_SYM(chal_write32) \
    EXPORT_SYM(chal_write64) \
    EXPORT_SYM(chal_inb) \
    EXPORT_SYM(chal_inw) \
    EXPORT_SYM(chal_inl) \
    EXPORT_SYM(chal_outb) \
    EXPORT_SYM(chal_outw) \
    EXPORT_SYM(chal_outl) \
    EXPORT_SYM(chal_disable_interrupts) \
    EXPORT_SYM(chal_enable_interrupts) \
    EXPORT_SYM(chal_restore_interrupts) \
    EXPORT_SYM(chal_get_timestamp) \
    EXPORT_SYM(chal_udelay) \
    EXPORT_SYM(chal_mdelay) \
    EXPORT_SYM(chal_cache_flush) \
    EXPORT_SYM(chal_cache_invalidate) \
    EXPORT_SYM(chal_cache_prefetch) \
    EXPORT_SYM(chal_uart_putc) \
    EXPORT_SYM(chal_uart_puts) \
    EXPORT_SYM(chal_uart_getc) \
    EXPORT_SYM(chal_uart_rx_ready) \
    EXPORT_SYM(chal_get_cpu_id) \
    EXPORT_SYM(chal_get_page_size) \
    EXPORT_SYM(chal_atomic_cas32) \
    EXPORT_SYM(chal_atomic_cas64) \
    EXPORT_SYM(chal_atomic_add32) \
    EXPORT_SYM(chal_atomic_add64) \
    EXPORT_SYM(chal_atomic_xchg32) \
    EXPORT_SYM(chal_atomic_xchg64)

/* 无依赖 */
#define CHAL_DEPENDENCIES /* none */

#endif /* CHAL_METADATA_H */
