/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核控制台实现
 * 仅支持串口输出
 */

#include <stdarg.h>
#include "console.h"
#include "minimal_uart.h"
#include "mem.h"
#include "hal.h"

/* 初始化控制台 */
void console_init(console_type_t type)
{
    (void)type;  /* 暂时忽略类型参数 */
    
    /* 引导程序已经初始化了串口，这里不再重新初始化
       避免与引导程序的串口配置冲突 */
}

/* 清屏 */
void console_clear(void)
{
    /* 串口清屏：发送ANSI清屏序列 */
    const char *clear_seq = "\033[2J\033[H";
    while (*clear_seq) {
        minimal_uart_putc(*clear_seq++);
    }
}

/* 输出字符 */
void console_putchar(char c)
{
    /* 仅通过串口输出 */
    minimal_uart_putc(c);
}

/* 输出字符串 */
void console_puts(const char *str)
{
    while (*str) {
        minimal_uart_putc(*str++);
    }
}

/* 输出数字（十进制） */
void console_putu64(uint64_t value)
{
    if (value == 0) {
        console_putchar('0');
        return;
    }
    
    char buffer[21];
    int pos = 20;
    buffer[pos] = '\0';
    
    while (value > 0 && pos > 0) {
        buffer[--pos] = (char)('0' + (value % 10));
        value /= 10;
    }
    
    console_puts(&buffer[pos]);
}

/* 输出有符号整数（64位） */
void console_puti64(s64 value)
{
    if (value < 0) {
        console_putchar('-');
        value = -value;
    }
    console_putu64((u64)value);
}

/* 格式化输出 */
void console_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_vprintf(fmt, args);
    va_end(args);
}

/* 输出数字（十六进制） */
void console_puthex64(uint64_t value)
{
    const char hex_chars[] = "0123456789ABCDEF";
    console_puts("0x");
    
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        console_putchar(hex_chars[nibble]);
    }
}

void console_puthex32(uint32_t value)
{
    console_puthex64(value);
}

/* 输出32位无符号整数 */
void console_putu32(uint32_t value)
{
    console_putu64(value);
}

/* 输出32位有符号整数 */
void console_puti32(s32 value)
{
    console_puti64(value);
}

/* 格式化输出（va_list版本） */
void console_vprintf(const char *fmt, va_list args)
{
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                    console_puti64(va_arg(args, s64));
                    break;
                case 'u':
                    console_putu64(va_arg(args, u64));
                    break;
                case 'x':
                    console_puthex64(va_arg(args, u64));
                    break;
                case 'p':
                    console_puts("0x");
                    console_puthex64(va_arg(args, u64));
                    break;
                case 'c':
                    console_putchar((char)va_arg(args, int));
                    break;
                case 's':
                    console_puts(va_arg(args, const char*));
                    break;
                case 'l':
                    fmt++;
                    if (*fmt == 'u') {
                        console_putu64(va_arg(args, u64));
                    } else if (*fmt == 'd') {
                        console_puti64(va_arg(args, s64));
                    } else if (*fmt == 'x') {
                        console_puthex64(va_arg(args, u64));
                    } else {
                        console_putchar('%');
                        console_putchar('l');
                        console_putchar(*fmt);
                    }
                    break;
                case '\0':
                    console_putchar('%');
                    goto done;
                default:
                    console_putchar('%');
                    console_putchar(*fmt);
                    break;
            }
        } else {
            console_putchar(*fmt);
        }
        fmt++;
    }
done:
    return;
}

/* 日志输出函数 */
void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_vprintf(fmt, args);
    va_end(args);
}

void log_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_puts("[WARN] ");
    console_vprintf(fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_puts("[ERROR] ");
    console_vprintf(fmt, args);
    va_end(args);
}