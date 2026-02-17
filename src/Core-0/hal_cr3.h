/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK硬件抽象层（HAL）接口 - CR3扩展
 * 提供页表管理相关的硬件抽象
 */

#ifndef HIK_KERNEL_HAL_CR3_H
#define HIK_KERNEL_HAL_CR3_H

#include "types.h"

/* ==================== CR3控制寄存器接口 ==================== */

/**
 * 读取CR3寄存器（页表基址）
 */
static inline u64 hal_get_cr3(void) {
    u64 val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

/**
 * 写入CR3寄存器（页表基址）
 */
static inline void hal_set_cr3(u64 val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val));
}

/**
 * 使单个页失效
 */
static inline void hal_invalidate_page(void* addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif /* HIK_KERNEL_HAL_CR3_H */