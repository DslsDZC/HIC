/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核控制台实现
 * 仅支持串口输出
 */

#include <stdarg.h>
#include "console.h"
#include "mem.h"
#include "hal.h"
#include "boot_info.h"

/* 默认串口端口（x86_64 COM1） */
#define DEFAULT_SERIAL_PORT 0x3F8

/* 控制台状态 */
static console_info_t g_console_info = {
    .type = CONSOLE_TYPE_SERIAL,
    .serial_port = DEFAULT_SERIAL_PORT,
    .serial_baud = 115200
};

/* 串口输出函数（使用HAL接口，支持多架构） */
static void serial_putc(char c) {
    /* 使用HAL接口输出，支持多架构 */
    hal_outb(g_console_info.serial_port, (u8)c);
}

/* 等待串口发送完成（读取LSR寄存器的bit 5） */
static void serial_wait_tx(void) {
#if defined(__x86_64__)
    /* x86_64架构：读取Line Status Register（LSR，偏移5）的bit 5（THRE） */
    volatile u8 lsr;
    do {
        lsr = hal_inb(g_console_info.serial_port + 5);
    } while ((lsr & 0x20) == 0);  /* 等待Transmitter Holding Register Empty */
#else
    /* 其他架构：简单的延迟 */
    hal_udelay(1);
#endif
}

/* 计算波特率除数（仅x86_64需要） */
#if defined(__x86_64__)
static u16 serial_calc_divisor(u32 baud) {
    /* 基准频率 = 115200 Hz（标准PC串口时钟） */
    if (baud == 0) return 1;  /* 防止除零 */
    return (u16)(115200 / baud);
}
#endif

/* 初始化控制台 */
void console_init(console_type_t type)
{
    g_console_info.type = type;
    
#if defined(__x86_64__)
    /* x86_64架构：需要初始化串口硬件
     * 使用最简化的配置，逐步初始化
     */
    u16 port = g_console_info.serial_port;
    
    /* 最简配置：只设置8N1，不改变波特率（假设bootloader已设置） */
    hal_outb(port + 3, 0x03);  /* 8N1, DLAB disable */
    
    /* 稍微延迟 */
    hal_udelay(10);
#else
    /* 其他架构：假设bootloader已经初始化串口 */
    (void)port;
#endif
}

/* 设置串口配置 */
void console_set_serial_config(u16 port, u32 baud) {
    g_console_info.serial_port = (port != 0) ? port : DEFAULT_SERIAL_PORT;
    g_console_info.serial_baud = (baud != 0) ? baud : 115200;
    
    /* 重新初始化串口 */
    console_init(g_console_info.type);
}

/* 获取控制台信息 */
console_info_t* console_get_info(void) {
    return &g_console_info;
}

/* 清屏 */
void console_clear(void)
{
    /* 串口清屏：发送ANSI清屏序列 */
    const char *clear_seq = "\033[2J\033[H";
    while (*clear_seq) {
        serial_putc(*clear_seq++);
    }
}

/* 输出字符 */
void console_putchar(char c)
{
    /* 仅通过串口输出 */
    serial_putc(c);
}

/* 输出字符串 */
void console_puts(const char *str)
{
    while (*str) {
        serial_putc(*str++);
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