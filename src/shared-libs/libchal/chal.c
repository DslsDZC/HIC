/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL (Core Hardware Abstraction Layer) - 架构无关实现
 * 
 * 此文件仅包含架构无关的代码。
 * 架构特定实现在 arch/<arch>/chal_arch.c 中。
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* ==================== 架构特定函数声明 ==================== */

/* 由 arch/<arch>/chal_arch.c 实现 */
extern void arch_memory_barrier(void);
extern void arch_read_barrier(void);
extern void arch_write_barrier(void);
extern uint64_t arch_get_timestamp(void);
extern void arch_udelay(uint32_t us);
extern void arch_cache_flush(void *addr, size_t size);
extern void arch_cache_invalidate(void *addr, size_t size);
extern void arch_cache_prefetch(const void *addr);
extern uint32_t arch_get_cpu_id(void);
extern bool arch_uart_init(uint64_t base);
extern void arch_uart_putc(uint64_t base, char c);
extern char arch_uart_getc(uint64_t base);
extern bool arch_uart_rx_ready(uint64_t base);

/* ==================== 内存屏障（转发到架构实现） ==================== */

void chal_memory_barrier(void) {
    arch_memory_barrier();
}

void chal_read_barrier(void) {
    arch_read_barrier();
}

void chal_write_barrier(void) {
    arch_write_barrier();
}

/* ==================== 内存操作（架构无关） ==================== */

void chal_memcpy(void *dst, const void *src, size_t size) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (size--) {
        *d++ = *s++;
    }
}

void chal_memset(void *dst, int value, size_t size) {
    uint8_t *d = (uint8_t *)dst;
    while (size--) {
        *d++ = (uint8_t)value;
    }
}

int chal_memcmp(const void *s1, const void *s2, size_t size) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

void chal_memzero(void *dst, size_t size) {
    uint8_t *d = (uint8_t *)dst;
    while (size--) {
        *d++ = 0;
    }
}

/* ==================== MMIO（架构无关，直接内存访问） ==================== */

uint8_t chal_read8(const volatile void *addr) {
    return *(const volatile uint8_t *)addr;
}

uint16_t chal_read16(const volatile void *addr) {
    return *(const volatile uint16_t *)addr;
}

uint32_t chal_read32(const volatile void *addr) {
    return *(const volatile uint32_t *)addr;
}

uint64_t chal_read64(const volatile void *addr) {
    return *(const volatile uint64_t *)addr;
}

void chal_write8(volatile void *addr, uint8_t value) {
    *(volatile uint8_t *)addr = value;
    arch_memory_barrier();
}

void chal_write16(volatile void *addr, uint16_t value) {
    *(volatile uint16_t *)addr = value;
    arch_memory_barrier();
}

void chal_write32(volatile void *addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
    arch_memory_barrier();
}

void chal_write64(volatile void *addr, uint64_t value) {
    *(volatile uint64_t *)addr = value;
    arch_memory_barrier();
}

/* ==================== 时间（转发到架构实现） ==================== */

uint64_t chal_get_timestamp(void) {
    return arch_get_timestamp();
}

void chal_udelay(uint32_t us) {
    arch_udelay(us);
}

void chal_mdelay(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        arch_udelay(1000);
    }
}

/* ==================== 缓存（转发到架构实现） ==================== */

void chal_cache_flush(void *addr, size_t size) {
    arch_cache_flush(addr, size);
}

void chal_cache_invalidate(void *addr, size_t size) {
    arch_cache_invalidate(addr, size);
}

void chal_cache_prefetch(const void *addr) {
    arch_cache_prefetch(addr);
}

/* ==================== 串口 ==================== */

/* 默认串口基地址 */
static uint64_t g_uart_base = 0x3F8;  /* COM1 */

void chal_uart_set_base(uint64_t base) {
    g_uart_base = base;
    arch_uart_init(base);
}

void chal_uart_putc(char c) {
    arch_uart_putc(g_uart_base, c);
}

void chal_uart_puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            arch_uart_putc(g_uart_base, '\r');
        }
        arch_uart_putc(g_uart_base, *str++);
    }
}

char chal_uart_getc(void) {
    return arch_uart_getc(g_uart_base);
}

bool chal_uart_rx_ready(void) {
    return arch_uart_rx_ready(g_uart_base);
}

/* ==================== 系统信息 ==================== */

uint32_t chal_get_cpu_id(void) {
    return arch_get_cpu_id();
}

uint32_t chal_get_page_size(void) {
    return 4096;  /* 4KB */
}

/* ==================== 中断控制（需要 syscall） ==================== */

extern long syscall0(long num);
extern long syscall1(long num, long a1);

#define SYSCALL_CHAL_DISABLE_IRQ  0x1000
#define SYSCALL_CHAL_ENABLE_IRQ   0x1001
#define SYSCALL_CHAL_RESTORE_IRQ  0x1002

bool chal_disable_interrupts(void) {
    return syscall0(SYSCALL_CHAL_DISABLE_IRQ) != 0;
}

void chal_enable_interrupts(void) {
    syscall0(SYSCALL_CHAL_ENABLE_IRQ);
}

void chal_restore_interrupts(bool state) {
    syscall1(SYSCALL_CHAL_RESTORE_IRQ, (long)state);
}
