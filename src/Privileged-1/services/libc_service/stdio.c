/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "../include/stdio.h"
#include "../include/stdarg.h"
#include "../include/string.h"
#include "../include/stdint.h"
#include "../include/stddef.h"

/* 外部控制台函数（需要由服务提供） */
extern void console_putchar(char c);
extern void console_puts(const char *s);

/* 格式化输出 */
static int print_int(int value, int base, int is_signed, int is_uppercase) {
    char buffer[32];
    int i = 0;
    int negative = 0;
    unsigned int unsigned_value;
    
    if (is_signed && value < 0) {
        negative = 1;
        unsigned_value = -value;
    } else {
        unsigned_value = (unsigned int)value;
    }
    
    /* 转换为字符串 */
    if (unsigned_value == 0) {
        buffer[i++] = '0';
    } else {
        while (unsigned_value > 0) {
            int digit = unsigned_value % base;
            if (digit < 10) {
                buffer[i++] = '0' + digit;
            } else {
                buffer[i++] = (is_uppercase ? 'A' : 'a') + (digit - 10);
            }
            unsigned_value /= base;
        }
    }
    
    /* 添加符号 */
    if (negative) {
        buffer[i++] = '-';
    }
    
    /* 反转并输出 */
    int count = 0;
    while (i > 0) {
        console_putchar(buffer[--i]);
        count++;
    }
    
    return count;
}

static int print_long(long value, int base, int is_signed, int is_uppercase) {
    char buffer[64];
    int i = 0;
    int negative = 0;
    unsigned long unsigned_value;
    
    if (is_signed && value < 0) {
        negative = 1;
        unsigned_value = -value;
    } else {
        unsigned_value = (unsigned long)value;
    }
    
    /* 转换为字符串 */
    if (unsigned_value == 0) {
        buffer[i++] = '0';
    } else {
        while (unsigned_value > 0) {
            int digit = unsigned_value % base;
            if (digit < 10) {
                buffer[i++] = '0' + digit;
            } else {
                buffer[i++] = (is_uppercase ? 'A' : 'a') + (digit - 10);
            }
            unsigned_value /= base;
        }
    }
    
    /* 添加符号 */
    if (negative) {
        buffer[i++] = '-';
    }
    
    /* 反转并输出 */
    int count = 0;
    while (i > 0) {
        console_putchar(buffer[--i]);
        count++;
    }
    
    return count;
}

static int print_pointer(void *ptr) {
    uintptr_t value = (uintptr_t)ptr;
    char buffer[32];
    int i = 0;
    
    if (value == 0) {
        console_puts("(nil)");
        return 5;
    }
    
    /* 转换为十六进制字符串 */
    while (value > 0) {
        int digit = value % 16;
        if (digit < 10) {
            buffer[i++] = '0' + digit;
        } else {
            buffer[i++] = 'a' + (digit - 10);
        }
        value /= 16;
    }
    
    /* 输出 0x 前缀 */
    console_puts("0x");
    int count = 2;
    
    /* 反转并输出 */
    while (i > 0) {
        console_putchar(buffer[--i]);
        count++;
    }
    
    return count;
}

static int print_string(const char *str) {
    if (str == NULL) {
        console_puts("(null)");
        return 6;
    }
    
    int count = 0;
    while (*str) {
        console_putchar(*str++);
        count++;
    }
    return count;
}

int printf(const char *format, ...) {
    va_list args;
    int count = 0;
    
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            
            /* 处理格式说明符 */
            int is_long = 0;
            int is_uppercase = 0;
            
            /* 检查长度修饰符 */
            if (*format == 'l') {
                is_long = 1;
                format++;
            }
            
            /* 检查大写 */
            if (*format == 'X') {
                is_uppercase = 1;
            }
            
            switch (*format) {
                case '%':
                    console_putchar('%');
                    count++;
                    break;
                    
                case 'c':
                    console_putchar((char)va_arg(args, int));
                    count++;
                    break;
                    
                case 's':
                    count += print_string(va_arg(args, const char *));
                    break;
                    
                case 'd':
                case 'i':
                    if (is_long) {
                        count += print_long(va_arg(args, long), 10, 1, 0);
                    } else {
                        count += print_int(va_arg(args, int), 10, 1, 0);
                    }
                    break;
                    
                case 'u':
                    if (is_long) {
                        count += print_long(va_arg(args, long), 10, 0, 0);
                    } else {
                        count += print_int(va_arg(args, int), 10, 0, 0);
                    }
                    break;
                    
                case 'x':
                    if (is_long) {
                        count += print_long(va_arg(args, long), 16, 0, 0);
                    } else {
                        count += print_int(va_arg(args, int), 16, 0, 0);
                    }
                    break;
                    
                case 'X':
                    if (is_long) {
                        count += print_long(va_arg(args, long), 16, 0, 1);
                    } else {
                        count += print_int(va_arg(args, int), 16, 0, 1);
                    }
                    break;
                    
                case 'p':
                    count += print_pointer(va_arg(args, void *));
                    break;
                    
                case 'o':
                    if (is_long) {
                        count += print_long(va_arg(args, long), 8, 0, 0);
                    } else {
                        count += print_int(va_arg(args, int), 8, 0, 0);
                    }
                    break;
                    
                default:
                    console_putchar('%');
                    console_putchar(*format);
                    count += 2;
                    break;
            }
        } else {
            console_putchar(*format);
            count++;
        }
        
        format++;
    }
    
    va_end(args);
    return count;
}

int sprintf(char *str, const char *format, ...) {
    /* 简化实现：先格式化到字符串，然后返回长度 */
    /* 完整实现需要更复杂的逻辑 */
    va_list args;
    va_start(args, format);
    
    /* TODO: 实现完整的 sprintf */
    /* 这是一个简化版本，需要完善 */
    
    va_end(args);
    return 0;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    /* 简化实现 */
    /* TODO: 实现完整的 snprintf */
    
    va_list args;
    va_start(args, format);
    
    /* TODO: 实现完整的 snprintf */
    
    va_end(args);
    return 0;
}

int puts(const char *s) {
    if (s == NULL) {
        return -1;
    }
    
    console_puts(s);
    console_putchar('\n');
    
    return (int)strlen(s) + 1;
}

int putchar(int c) {
    console_putchar((char)c);
    return c;
}

int fputs(const char *s, void *stream) {
    /* 简化实现：忽略 stream 参数 */
    if (s == NULL) {
        return -1;
    }
    
    console_puts(s);
    return (int)strlen(s);
}

int fputc(int c, void *stream) {
    /* 简化实现：忽略 stream 参数 */
    console_putchar((char)c);
    return c;
}