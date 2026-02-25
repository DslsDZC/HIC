/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核最小化入口
 * 用于验证构建系统
 */

#include "types.h"
#include "lib/console.h"

/* 简单的内核入口点 */
void kernel_main(void *info) {
    (void)info;  /* 消除未使用参数警告 */
    console_puts("HIC Kernel Minimal Version\n");
    console_puts("Build verification successful!\n");
    
    /* 简单的无限循环 */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
