/**
 * 控制台输出实现
 */

#include <stdint.h>
#include <stdarg.h>
#include "console.h"
#include "string.h"
#include "efi.h"

// 外部全局变量（在main.c中定义）
extern EFI_SYSTEM_TABLE *gST;

// VGA文本模式显示
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA颜色属性
static uint8_t g_current_attr = 0x0F;  // 白字黑底

// 当前光标位置
static int g_cursor_x = 0;
static int g_cursor_y = 0;

// 日志级别
static log_level_t g_log_level = LOG_LEVEL_INFO;

// 串口配置
static uint16_t g_serial_port = 0x3F8;  // COM1

// 静态函数前置声明
static void outb(uint16_t port, uint8_t value);
static uint8_t inb(uint16_t port);
static void int_to_str(int64_t value, char *buffer, int base);
static void uint_to_str(uint64_t value, char *buffer, int base);
static void uint64_to_str(uint64_t value, char *buffer, int base);
static int vsnprintf(char *buffer, size_t size, const char *fmt, va_list args);

/**
 * 初始化控制台
 */
void console_init(void)
{
    // 直接使用UEFI ConOut，不依赖其他初始化
    if (gST && gST->con_out) {
        gST->con_out->ClearScreen(gST->con_out);
    }
}

/**
 * 输出字符到控制台
 * 使用UEFI ConOut协议
 */
void console_putchar(char c)
{
    // 使用UEFI ConOut协议
    if (gST && gST->con_out) {
        CHAR16 str[2];
        str[0] = (CHAR16)c;
        str[1] = 0;
        gST->con_out->OutputString(gST->con_out, str);
    }
}

/**
 * 输出字符串
 * 使用UEFI ConOut协议
 */
void console_puts(const char *str)
{
    // 使用UEFI ConOut协议
    if (gST && gST->con_out) {
        // 转换为CHAR16字符串
        CHAR16 utf16[256];
        int i = 0;
        while (*str && i < 255) {
            utf16[i++] = (CHAR16)*str++;
        }
        utf16[i] = 0;
        gST->con_out->OutputString(gST->con_out, utf16);
    }
}

/**
 * 简化的printf实现
 */
void console_printf(const char *fmt, ...)
{
    va_list args;
    char buffer[32];
    
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int value = va_arg(args, int);
                    int_to_str(value, buffer, 10);
                    console_puts(buffer);
                    break;
                }
                
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_str(value, buffer, 10);
                    console_puts(buffer);
                    break;
                }
                
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_str(value, buffer, 16);
                    console_puts(buffer);
                    break;
                }
                
                case 'X': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_str(value, buffer, 16);
                    // 转换为大写
                    for (char *p = buffer; *p; p++) {
                        if (*p >= 'a' && *p <= 'f') {
                            *p -= 32;
                        }
                    }
                    console_puts(buffer);
                    break;
                }
                
                case 'p': {
                    uint64_t value = va_arg(args, uint64_t);
                    console_putchar('0');
                    console_putchar('x');
                    uint64_to_str(value, buffer, 16);
                    console_puts(buffer);
                    break;
                }
                
                case 'l': {
                    fmt++;
                    if (*fmt == 'l') {
                        fmt++;
                        uint64_t value = va_arg(args, uint64_t);
                        uint64_to_str(value, buffer, 10);
                        console_puts(buffer);
                    } else {
                        long value = va_arg(args, long);
                        int_to_str(value, buffer, 10);
                        console_puts(buffer);
                    }
                    break;
                }
                
                case 's': {
                    char *str = va_arg(args, char *);
                    if (str) {
                        console_puts(str);
                    } else {
                        console_puts("(null)");
                    }
                    break;
                }
                
                case 'c': {
                    char c = va_arg(args, int);
                    console_putchar(c);
                    break;
                }
                
                case '%':
                    console_putchar('%');
                    break;
                
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
    
    va_end(args);
}

/**
 * 设置颜色
 */
void console_set_color(console_color_t fg, console_color_t bg)
{
    g_current_attr = (bg << 4) | (fg & 0x0F);
}

/**
 * 清屏
 * 使用UEFI ConOut协议
 */
void console_clear(void)
{
    // 使用UEFI ConOut清屏
    if (gST && gST->con_out) {
        gST->con_out->ClearScreen(gST->con_out);
    }
}

/**
 * 设置光标位置
 * 在UEFI环境中，光标位置由ConOut协议管理
 */
void console_set_cursor(int x, int y)
{
    if (gST && gST->con_out) {
        // 使用UEFI ConOut协议设置光标位置
        gST->con_out->SetCursorPosition(gST->con_out, x, y);
    }
    
    // 保存局部变量用于其他功能
    if (x >= 0 && x < VGA_WIDTH) {
        g_cursor_x = x;
    }
    if (y >= 0 && y < VGA_HEIGHT) {
        g_cursor_y = y;
    }
}

/**
 * 获取光标位置
 */
void console_get_cursor(int *x, int *y)
{
    if (x) *x = g_cursor_x;
    if (y) *y = g_cursor_y;
}

/**
 * 设置日志级别
 */
void log_set_level(log_level_t level)
{
    g_log_level = level;
}

/**
 * 日志输出辅助函数
 */
static void log_output(log_level_t level, const char *prefix, const char *fmt, va_list args)
{
    if (level > g_log_level) {
        return;
    }
    
    // 设置颜色
    switch (level) {
        case LOG_LEVEL_ERROR:
            console_set_color(CON_COLOR_LIGHTRED, CON_COLOR_BLACK);
            break;
        case LOG_LEVEL_WARN:
            console_set_color(CON_COLOR_YELLOW, CON_COLOR_BLACK);
            break;
        case LOG_LEVEL_INFO:
            console_set_color(CON_COLOR_LIGHTGREEN, CON_COLOR_BLACK);
            break;
        case LOG_LEVEL_DEBUG:
            console_set_color(CON_COLOR_LIGHTCYAN, CON_COLOR_BLACK);
            break;
        case LOG_LEVEL_TRACE:
            console_set_color(CON_COLOR_LIGHTGRAY, CON_COLOR_BLACK);
            break;
    }
    
    console_puts(prefix);
    console_set_color(CON_COLOR_WHITE, CON_COLOR_BLACK);
    
    // 输出格式化字符串（使用静态缓冲区避免栈溢出）
    static char log_buffer[256];
    vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
    console_puts(log_buffer);
    console_putchar('\n');
}

/**
 * 错误日志
 */
void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_output(LOG_LEVEL_ERROR, "[ERROR] ", fmt, args);
    va_end(args);
}

/**
 * 警告日志
 */
void log_warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_output(LOG_LEVEL_WARN, "[WARN] ", fmt, args);
    va_end(args);
}

/**
 * 信息日志
 */
void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_output(LOG_LEVEL_INFO, "[INFO] ", fmt, args);
    va_end(args);
}

/**
 * 调试日志
 */
void log_debug(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_output(LOG_LEVEL_DEBUG, "[DEBUG] ", fmt, args);
    va_end(args);
}

/**
 * 跟踪日志
 */
void log_trace(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_output(LOG_LEVEL_TRACE, "[TRACE] ", fmt, args);
    va_end(args);
}

/**
 * 初始化串口
 */
void serial_init(uint16_t port)
{
    g_serial_port = port;
    
    // 禁用中断
    outb(port + 1, 0x00);
    
    // 启用DLAB（设置波特率）
    outb(port + 3, 0x80);
    
    // 设置波特率为115200
    outb(port + 0, 0x01);  // 低字节
    outb(port + 1, 0x00);  // 高字节
    
    // 8位数据，无校验，1停止位
    outb(port + 3, 0x03);
    
    // 启用FIFO，清除缓冲区
    outb(port + 2, 0xC7);
    
    // 启用RTS和DTR
    outb(port + 4, 0x0B);
}

/**
 * 串口输出字符
 */
void serial_putchar(uint16_t port, char c)
{
    // 等待发送缓冲区为空
    while ((inb(port + 5) & 0x20) == 0) {
        ;
    }
    
    outb(port, c);
}

/**
 * 串口输出字符串
 */
void serial_puts(uint16_t port, const char *str)
{
    while (*str) {
        serial_putchar(port, *str++);
    }
}

/**
 * 串口格式化输出
 */
void serial_printf(uint16_t port, const char *fmt, ...)
{
    va_list args;
    char buffer[256];
    
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    serial_puts(port, buffer);
}

/**
 * 端口输出
 */
static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * 端口输入
 */
static uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * 整数转字符串（有符号）
 */
static void int_to_str(int64_t value, char *buffer, int base)
{
    if (value < 0) {
        *buffer++ = '-';
        value = -value;
    }
    uint_to_str((uint64_t)value, buffer, base);
}

/**
 * 整数转字符串（无符号）
 */
static void uint_to_str(uint64_t value, char *buffer, int base)
{
    char digits[] = "0123456789abcdef";
    char temp[32];
    int i = 0;
    
    if (value == 0) {
        *buffer++ = '0';
        *buffer = '\0';
        return;
    }
    
    while (value > 0) {
        temp[i++] = digits[value % base];
        value /= base;
    }
    
    while (i > 0) {
        *buffer++ = temp[--i];
    }
    *buffer = '\0';
}

/**
 * 64位整数转字符串
 */
static void uint64_to_str(uint64_t value, char *buffer, int base)
{
    uint_to_str(value, buffer, base);
}

/**
 * 简化的vsnprintf实现
 */
static int vsnprintf(char *buffer, size_t size, const char *fmt, va_list args)
{
    size_t written = 0;
    char temp[32];
    
    // 清零temp缓冲区
    for (int i = 0; i < 32; i++) {
        temp[i] = 0;
    }
    
    while (*fmt && written < size - 1) {
        if (*fmt == '%') {
            fmt++;
            
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int value = va_arg(args, int);
                    int_to_str(value, temp, 10);
                    {
                        const char *p = temp;
                        while (*p && written < size - 1) {
                            buffer[written++] = *p++;
                        }
                    }
                    break;
                }
                
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_str(value, temp, 10);
                    {
                        const char *p = temp;
                        while (*p && written < size - 1) {
                            buffer[written++] = *p++;
                        }
                    }
                    break;
                }
                
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    uint_to_str(value, temp, 16);
                    {
                        const char *p = temp;
                        while (*p && written < size - 1) {
                            buffer[written++] = *p++;
                        }
                    }
                    break;
                }
                
                case 'l':
                    fmt++;
                    if (*fmt == 'l') {
                        fmt++;
                        uint64_t value = va_arg(args, uint64_t);
                        uint64_to_str(value, temp, 10);
                        {
                            const char *p = temp;
                            while (*p && written < size - 1) {
                                buffer[written++] = *p++;
                            }
                        }
                    }
                    break;
                
                case 's': {
                    char *str = va_arg(args, char *);
                    if (str) {
                        const char *p = str;
                        while (*p && written < size - 1) {
                            buffer[written++] = *p++;
                        }
                    } else {
                        const char *null_str = "(null)";
                        const char *p = null_str;
                        while (*p && written < size - 1) {
                            buffer[written++] = *p++;
                        }
                    }
                    break;
                }
                
                case 'c': {
                    char c = va_arg(args, int);
                    if (written < size - 1) {
                        buffer[written++] = c;
                    }
                    break;
                }
                
                case '%':
                    if (written < size - 1) {
                        buffer[written++] = '%';
                    }
                    break;
                
                default:
                    if (written < size - 1) {
                        buffer[written++] = '%';
                    }
                    if (written < size - 1) {
                        buffer[written++] = *fmt;
                    }
                    break;
            }
        } else {
            if (written < size - 1) {
                buffer[written++] = *fmt;
            }
        }
        
        fmt++;
    }
    
    buffer[written] = '\0';
    return written;
}
