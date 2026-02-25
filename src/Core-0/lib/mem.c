/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核内存操作实现
 */

#include "mem.h"

/* 内存填充为零 */
void *memzero(void *ptr, size_t len)
{
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
    return ptr;
}

/* 内存填充 */
void *memset(void *ptr, int value, size_t len)
{
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = (uint8_t)value;
    }
    return ptr;
}

/* 内存复制 */
void *memcopy(void *dest, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return dest;
}

/* 内存移动（处理重叠） */
void *memmove(void *dest, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    if (d < s) {
        /* 从前向后复制 */
        for (size_t i = 0; i < len; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        /* 从后向前复制 */
        for (size_t i = len; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

/* 内存比较 */
int memcmp(const void *ptr1, const void *ptr2, size_t len)
{
    const uint8_t *p1 = (const uint8_t *)ptr1;
    const uint8_t *p2 = (const uint8_t *)ptr2;
    
    for (size_t i = 0; i < len; i++) {
        if (p1[i] < p2[i]) return -1;
        if (p1[i] > p2[i]) return 1;
    }
    return 0;
}

/* 字节交换 */
uint16_t swap16(uint16_t value)
{
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

uint32_t swap32(uint32_t value)
{
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value >> 8) & 0xFF00) |
           ((value >> 24) & 0xFF);
}

uint64_t swap64(uint64_t value)
{
    return ((value & 0xFF) << 56) |
           ((value & 0xFF00) << 40) |
           ((value & 0xFF0000) << 24) |
           ((value & 0xFF000000) << 8) |
           ((value >> 8) & 0xFF000000) |
           ((value >> 24) & 0xFF0000) |
           ((value >> 40) & 0xFF00) |
           ((value >> 56) & 0xFF);
}

/* 内存对齐 */
uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
}