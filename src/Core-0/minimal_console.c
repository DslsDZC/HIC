/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 简单的控制台实现
 */

void console_puts(const char *str) {
    /* 使用内联汇编写入串口 */
    const char *p = str;
    while (*p) {
        __asm__ volatile (
            "outb %0, $0xE9"  /* 写入 COM1 端口 */
            :
            : "a"(*p)
        );
        p++;
    }
}