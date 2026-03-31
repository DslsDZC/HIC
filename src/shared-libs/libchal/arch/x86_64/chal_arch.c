/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * CHAL - x86_64 架构特定实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* ==================== 内存屏障 ==================== */

void arch_memory_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

void arch_read_barrier(void) {
    __asm__ volatile("lfence" ::: "memory");
}

void arch_write_barrier(void) {
    __asm__ volatile("sfence" ::: "memory");
}

/* ==================== 时间戳 ==================== */

uint64_t arch_get_timestamp(void) {
    uint64_t value;
    __asm__ volatile("rdtsc" : "=A"(value));
    return value;
}

void arch_udelay(uint32_t us) {
    uint64_t start = arch_get_timestamp();
    /* 假设 1GHz 时钟，1us = 1000 cycles */
    uint64_t target = (uint64_t)us * 1000;
    while ((arch_get_timestamp() - start) < target) {
        __asm__ volatile("pause");
    }
}

/* ==================== IO 端口 ==================== */

uint8_t chal_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t chal_inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t chal_inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void chal_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void chal_outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void chal_outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* ==================== 缓存 ==================== */

void arch_cache_flush(void *addr, size_t size) {
    uintptr_t ptr = (uintptr_t)addr;
    uintptr_t end = ptr + size;
    for (; ptr < end; ptr += 64) {
        __asm__ volatile("clflush (%0)" : : "r"(ptr) : "memory");
    }
}

void arch_cache_invalidate(void *addr, size_t size) {
    (void)addr;
    (void)size;
    arch_memory_barrier();
}

void arch_cache_prefetch(const void *addr) {
    __asm__ volatile("prefetcht0 (%0)" : : "r"(addr) : "memory");
}

/* ==================== 串口 ==================== */

bool arch_uart_init(uint64_t base) {
    (void)base;
    return true;
}

void arch_uart_putc(uint64_t base, char c) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    /* 等待发送缓冲区空 */
    while ((uart[5] & 0x20) == 0) {
        __asm__ volatile("pause");
    }
    uart[0] = (uint8_t)c;
}

char arch_uart_getc(uint64_t base) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    while ((uart[5] & 0x01) == 0) {
        __asm__ volatile("pause");
    }
    return (char)uart[0];
}

bool arch_uart_rx_ready(uint64_t base) {
    volatile uint8_t *uart = (volatile uint8_t *)base;
    return (uart[5] & 0x01) != 0;
}

/* ==================== CPU ID ==================== */

uint32_t arch_get_cpu_id(void) {
    uint32_t id;
    __asm__ volatile(
        "cpuid"
        : "=b"(id)
        : "a"(1)
        : "ecx", "edx"
    );
    return id >> 24;  /* APIC ID */
}

/* ==================== 原子操作 ==================== */

bool chal_atomic_cas32(volatile uint32_t *addr, uint32_t expected, uint32_t desired) {
    uint8_t result;
    __asm__ volatile(
        "lock cmpxchgl %2, %1; sete %0"
        : "=r"(result), "+m"(*addr)
        : "r"(desired), "a"(expected)
        : "memory", "cc"
    );
    return result != 0;
}

bool chal_atomic_cas64(volatile uint64_t *addr, uint64_t expected, uint64_t desired) {
    uint8_t result;
    __asm__ volatile(
        "lock cmpxchgq %2, %1; sete %0"
        : "=r"(result), "+m"(*addr)
        : "r"(desired), "a"(expected)
        : "memory", "cc"
    );
    return result != 0;
}

uint32_t chal_atomic_add32(volatile uint32_t *addr, uint32_t value) {
    __asm__ volatile("lock xaddl %0, %1" : "+r"(value), "+m"(*addr) : : "memory", "cc");
    return value;
}

uint64_t chal_atomic_add64(volatile uint64_t *addr, uint64_t value) {
    __asm__ volatile("lock xaddq %0, %1" : "+r"(value), "+m"(*addr) : : "memory", "cc");
    return value;
}

uint32_t chal_atomic_xchg32(volatile uint32_t *addr, uint32_t value) {
    __asm__ volatile("lock xchgl %0, %1" : "+r"(value), "+m"(*addr) : : "memory");
    return value;
}

uint64_t chal_atomic_xchg64(volatile uint64_t *addr, uint64_t value) {
    __asm__ volatile("lock xchgq %0, %1" : "+r"(value), "+m"(*addr) : : "memory");
    return value;
}
