/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

#ifndef HIK_LIB_CONSOLE_H
#define HIK_LIB_CONSOLE_H

#include <stdarg.h>
#include "types.h"
#include "hal.h"

/* VGA控制台（x86特有） */
#if defined(__x86_64__)
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_BUFFER  (volatile u16*)0xB8000
#define VGA_COLOR   0x0F  /* 白色 */
#endif

/* 控制台类型 */
typedef enum {
    CONSOLE_TYPE_SERIAL,
    CONSOLE_TYPE_VGA,
    CONSOLE_TYPE_FRAMEBUFFER,
    CONSOLE_TYPE_NONE
} console_type_t;

/* 控制台信息 */
typedef struct {
    console_type_t type;
    u16 serial_port;
    u32 serial_baud;
    u64 framebuffer_base;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 framebuffer_pitch;
    u32 framebuffer_bpp;
} console_info_t;

/* 控制台初始化 */
void console_init(console_type_t type);

/* 控制台输出 */
void console_putchar(char c);
void console_puts(const char *str);
void console_printf(const char *fmt, ...);
void console_vprintf(const char *fmt, va_list args);

/* 控制台控制 */
void console_clear(void);
void console_set_color(u8 foreground, u8 background);
void console_get_cursor(int *x, int *y);
void console_set_cursor(int x, int y);

/* 格式化输出辅助 */
void console_puthex64(u64 value);
void console_puthex32(u32 value);
void console_putu64(u64 value);
void console_putu32(u32 value);
void console_puti64(s64 value);
void console_puti32(s32 value);

/* 日志输出 */
void log_info(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* HIK_LIB_CONSOLE_H */