/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL - ARM64 架构特定实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* ==================== 内存屏障 ==================== */

void arch_memory_barrier(void) {
    __asm__ volatile("dmb ish" ::: "memory");
}

void arch_read_barrier(void) {
    __asm__ volatile("dmb ishld" ::: "memory");
}

void arch_write_barrier(void) {
    __asm__ volatile("dmb ishst" ::: "memory");
}

/* ==================== 时间戳 ==================== */

uint64_t arch_get_timestamp(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
}

void arch_udelay(uint32_t us) {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    uint64_t ticks = ((uint64_t)us * freq) / 1000000;
    uint64_t start = arch_get_timestamp();
    while ((arch_get_timestamp() - start) < ticks) {
        __asm__ volatile("yield");
    }
}

/* ==================== IO 端口（ARM64 无 IO 端口） ==================== */

uint8_t chal_inb(uint16_t port) { (void)port; return 0xFF; }
uint16_t chal_inw(uint16_t port) { (void)port; return 0xFFFF; }
uint32_t chal_inl(uint16_t port) { (void)port; return 0xFFFFFFFF; }
void chal_outb(uint16_t port, uint8_t value) { (void)port; (void)value; }
void chal_outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void chal_outl(uint16_t port, uint32_t value) { (void)port; (void)value; }

/* ==================== 缓存 ==================== */

void arch_cache_flush(void *addr, size_t size) {
    uintptr_t ptr = (uintptr_t)addr;
    uintptr_t end = ptr + size;
    for (; ptr < end; ptr += 64) {
        __asm__ volatile("dc cvac, %0" : : "r"(ptr) : "memory");
    }
    __asm__ volatile("dsb ish" ::: "memory");
}

void arch_cache_invalidate(void *addr, size_t size) {
    uintptr_t ptr = (uintptr_t)addr;
    uintptr_t end = ptr + size;
    for (; ptr < end; ptr += 64) {
        __asm__ volatile("dc ivac, %0" : : "r"(ptr) : "memory");
    }
    __asm__ volatile("dsb ish" ::: "memory");
}

void arch_cache_prefetch(const void *addr) {
    __asm__ volatile("prfm pldl1keep, [%0]" : : "r"(addr) : "memory");
}

/* ==================== 串口（PL011） ==================== */

bool arch_uart_init(uint64_t base) {
    (void)base;
    return true;
}

void arch_uart_putc(uint64_t base, char c) {
    volatile uint32_t *uart = (volatile uint32_t *)base;
    /* 等待发送缓冲区空 (PL011 FR.TXFF = 0) */
    while (uart[0x18/4] & (1 << 5)) {
        __asm__ volatile("yield");
    }
    uart[0] = (uint32_t)c;
}

char arch_uart_getc(uint64_t base) {
    volatile uint32_t *uart = (volatile uint32_t *)base;
    /* 等待接收缓冲区有数据 (PL011 FR.RXFE = 0) */
    while (uart[0x18/4] & (1 << 4)) {
        __asm__ volatile("yield");
    }
    return (char)uart[0];
}

bool arch_uart_rx_ready(uint64_t base) {
    volatile uint32_t *uart = (volatile uint32_t *)base;
    return (uart[0x18/4] & (1 << 4)) == 0;
}

/* ==================== CPU ID ==================== */

uint32_t arch_get_cpu_id(void) {
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xFF);
}

/* ==================== 原子操作 ==================== */

bool chal_atomic_cas32(volatile uint32_t *addr, uint32_t expected, uint32_t desired) {
    uint32_t result;
    __asm__ volatile(
        "1: ldaxr %w0, [%1]; "
        "cmp %w0, %w2; b.ne 2f; "
        "stlxr %w0, %w3, [%1]; "
        "cbnz %w0, 1b; "
        "mov %w0, #1; b 3f; "
        "2: mov %w0, #0; "
        "3: "
        : "=&r"(result)
        : "r"(addr), "r"(expected), "r"(desired)
        : "memory", "cc"
    );
    return result != 0;
}

bool chal_atomic_cas64(volatile uint64_t *addr, uint64_t expected, uint64_t desired) {
    uint64_t result;
    __asm__ volatile(
        "1: ldaxr %0, [%1]; "
        "cmp %0, %2; b.ne 2f; "
        "stlxr %w0, %3, [%1]; "
        "cbnz %w0, 1b; "
        "mov %0, #1; b 3f; "
        "2: mov %0, #0; "
        "3: "
        : "=&r"(result)
        : "r"(addr), "r"(expected), "r"(desired)
        : "memory", "cc"
    );
    return result != 0;
}

uint32_t chal_atomic_add32(volatile uint32_t *addr, uint32_t value) {
    uint32_t result;
    __asm__ volatile(
        "1: ldaxr %w0, [%1]; add %w0, %w0, %w2; "
        "stlxr w3, %w0, [%1]; cbnz w3, 1b"
        : "=&r"(result)
        : "r"(addr), "r"(value)
        : "w3", "memory", "cc"
    );
    return result - value;
}

uint64_t chal_atomic_add64(volatile uint64_t *addr, uint64_t value) {
    uint64_t result;
    __asm__ volatile(
        "1: ldaxr %0, [%1]; add %0, %0, %2; "
        "stlxr w3, %0, [%1]; cbnz w3, 1b"
        : "=&r"(result)
        : "r"(addr), "r"(value)
        : "w3", "memory", "cc"
    );
    return result - value;
}

uint32_t chal_atomic_xchg32(volatile uint32_t *addr, uint32_t value) {
    uint32_t result;
    __asm__ volatile(
        "1: ldaxr %w0, [%1]; "
        "stlxr w2, %w2, [%1]; cbnz w2, 1b"
        : "=&r"(result)
        : "r"(addr), "r"(value)
        : "w2", "memory"
    );
    return result;
}

uint64_t chal_atomic_xchg64(volatile uint64_t *addr, uint64_t value) {
    uint64_t result;
    __asm__ volatile(
        "1: ldaxr %0, [%1]; "
        "stlxr w2, %2, [%1]; cbnz w2, 1b"
        : "=&r"(result)
        : "r"(addr), "r"(value)
        : "w2", "memory"
    );
    return result;
}
