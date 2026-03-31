/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL - RISC-V 架构特定实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* ==================== 内存屏障 ==================== */

void arch_memory_barrier(void) {
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

void arch_read_barrier(void) {
    __asm__ volatile("fence ir, ir" ::: "memory");
}

void arch_write_barrier(void) {
    __asm__ volatile("fence ow, ow" ::: "memory");
}

/* ==================== 时间戳 ==================== */

uint64_t arch_get_timestamp(void) {
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

void arch_udelay(uint32_t us) {
    uint64_t ticks = (uint64_t)us * 100;  /* 假设 100MHz timebase */
    uint64_t start = arch_get_timestamp();
    while ((arch_get_timestamp() - start) < ticks) {
        __asm__ volatile("wfi");
    }
}

/* ==================== IO 端口（RISC-V 无 IO 端口，使用 MMIO） ==================== */

uint8_t chal_inb(uint16_t port) { (void)port; return 0xFF; }
uint16_t chal_inw(uint16_t port) { (void)port; return 0xFFFF; }
uint32_t chal_inl(uint16_t port) { (void)port; return 0xFFFFFFFF; }
void chal_outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void chal_outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void chal_outl(uint16_t port, uint32_t value) { (void)port; (void)value; }

/* ==================== 缓存 ==================== */

void arch_cache_flush(void *addr, size_t size) {
    /* RISC-V 缓存操作需要 Zicbom 扩展或 SBI 调用 */
    (void)addr;
    (void)size;
    arch_memory_barrier();
}

void arch_cache_invalidate(void *addr, size_t size) {
    (void)addr;
    (void)size;
    arch_memory_barrier();
}

void arch_cache_prefetch(const void *addr) {
    (void)addr;
}

/* ==================== 串口（UART 16550 兼容） ==================== */

bool arch_uart_init(uint64_t base) {
    (void)base;
    return true;
}

void arch_uart_putc(uint64_t base, char c) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    /* 等待发送缓冲区空 (LSR.THRE = 1) */
    while ((uart[5] & 0x20) == 0) {
        __asm__ volatile("wfi");
    }
    uart[0] = (uint8_t)c;
}

char arch_uart_getc(uint64_t base) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    while ((uart[5] & 0x01) == 0) {
        __asm__ volatile("wfi");
    }
    return (char)uart[0];
}

bool arch_uart_rx_ready(uint64_t base) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    return (uart[5] & 0x01) != 0;
}

/* ==================== CPU ID ==================== */

uint32_t arch_get_cpu_id(void) {
    uint64_t hartid;
    __asm__ volatile("mv %0, tp" : "=r"(hartid));
    return (uint32_t)hartid;
}

/* ==================== 原子操作 ==================== */

bool chal_atomic_cas32(volatile uint32_t *addr, uint32_t expected, uint32_t desired) {
    uint32_t result;
    __asm__ volatile(
        "1: lr.w %0, (%1); "
        "bne %0, %2, 2f; "
        "sc.w %0, %3, (%1); "
        "bnez %0, 1b; "
        "li %0, 1; j 3f; "
        "2: li %0, 0; "
        "3: "
        : "=&r"(result)
        : "r"(addr), "r"(expected), "r"(desired)
        : "memory"
    );
    return result != 0;
}

bool chal_atomic_cas64(volatile uint64_t *addr, uint64_t expected, uint64_t desired) {
    uint64_t result;
    __asm__ volatile(
        "1: lr.d %0, (%1); "
        "bne %0, %2, 2f; "
        "sc.d %0, %3, (%1); "
        "bnez %0, 1b; "
        "li %0, 1; j 3f; "
        "2: li %0, 0; "
        "3: "
        : "=&r"(result)
        : "r"(addr), "r"(expected), "r"(desired)
        : "memory"
    );
    return result != 0;
}

uint32_t chal_atomic_add32(volatile uint32_t *addr, uint32_t value) {
    uint32_t result, tmp;
    __asm__ volatile(
        "1: lr.w %0, (%2); add %0, %0, %3; "
        "sc.w %1, %0, (%2); bnez %1, 1b"
        : "=&r"(result), "=&r"(tmp)
        : "r"(addr), "r"(value)
        : "memory"
    );
    return result - value;
}

uint64_t chal_atomic_add64(volatile uint64_t *addr, uint64_t value) {
    uint64_t result, tmp;
    __asm__ volatile(
        "1: lr.d %0, (%2); add %0, %0, %3; "
        "sc.d %1, %0, (%2); bnez %1, 1b"
        : "=&r"(result), "=&r"(tmp)
        : "r"(addr), "r"(value)
        : "memory"
    );
    return result - value;
}

uint32_t chal_atomic_xchg32(volatile uint32_t *addr, uint32_t value) {
    uint32_t result, tmp;
    __asm__ volatile(
        "1: lr.w %0, (%2); "
        "sc.w %1, %3, (%2); bnez %1, 1b"
        : "=&r"(result), "=&r"(tmp)
        : "r"(addr), "r"(value)
        : "memory"
    );
    return result;
}

uint64_t chal_atomic_xchg64(volatile uint64_t *addr, uint64_t value) {
    uint64_t result, tmp;
    __asm__ volatile(
        "1: lr.d %0, (%2); "
        "sc.d %1, %3, (%2); bnez %1, 1b"
        : "=&r"(result), "=&r"(tmp)
        : "r"(addr), "r"(value)
        : "memory"
    );
    return result;
}
