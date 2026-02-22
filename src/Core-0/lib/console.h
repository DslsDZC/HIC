/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef HIC_LIB_CONSOLE_H
#define HIC_LIB_CONSOLE_H

#include <stdarg.h>
#include "types.h"
#include "hal.h"

/* 控制台类型 */
typedef enum {
    CONSOLE_TYPE_SERIAL
} console_type_t;

/* 控制台信息 */
typedef struct {
    console_type_t type;
    u16 serial_port;
    u32 serial_baud;
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

#endif /* HIC_LIB_CONSOLE_H */