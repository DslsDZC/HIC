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

// 串口是否已初始化的标志
static bool g_serial_initialized = false;

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
    // 只初始化一次串口（避免重复初始化导致的问题）
    if (!g_serial_initialized) {
        serial_init(0x3F8);  // COM1, 115200 baud
    }
    
    // 暂时禁用UEFI ConOut操作，避免可能的重复输出
    // if (gST && gST->con_out) {
    //     gST->con_out->ClearScreen(gST->con_out);
    // }
}

/**
 * 输出字符到控制台（仅UEFI，不输出到串口）
 * 注意：串口输出由serial_putchar单独处理
 */
static void __attribute__((unused)) console_putchar_uefi(char c)
{
    // 仅使用UEFI ConOut协议输出到屏幕
    if (gST && gST->con_out) {
        CHAR16 str[2];
        str[0] = (CHAR16)c;
        str[1] = 0;
        gST->con_out->OutputString(gST->con_out, str);
    }
}

/**
 * 输出字符到控制台（串口和屏幕）
 */
void console_putchar(char c)
{
    // 仅输出到串口
    serial_putchar(g_serial_port, c);
    
    // 暂时禁用UEFI屏幕输出，避免字符重复问题
    // console_putchar_uefi(c);
}

/**
 * 输出字符串
 * 同时输出到串口和UEFI屏幕
 */
void console_puts(const char *str)
{
    // 仅输出到串口
    serial_puts(g_serial_port, str);
    
    // 暂时禁用UEFI屏幕输出，避免字符重复问题
    /*
    if (gST && gST->con_out) {
        // 将ASCII字符串转换为CHAR16字符串
        size_t len = 0;
        const char *p = str;
        while (*p++) len++;
        
        // 使用栈上缓冲区（限制长度以避免栈溢出）
        CHAR16 buf[256];
        size_t copy_len = (len < 255) ? len : 255;
        
        for (size_t i = 0; i < copy_len; i++) {
            buf[i] = (CHAR16)str[i];
        }
        buf[copy_len] = 0;
        gST->con_out->OutputString(gST->con_out, buf);
    }
    */
}

/**
 * 简化的printf实现
 */
void console_printf(const char *fmt, ...)
{
    va_list args;
    char buffer[64];  // 增加缓冲区大小以支持64位指针
    
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
                        // %llx 或 %llu
                        if (*fmt == 'x') {
                            fmt++;
                            uint64_t value = va_arg(args, uint64_t);
                            uint64_to_str(value, buffer, 16);
                            console_puts(buffer);
                        } else if (*fmt == 'u') {
                            fmt++;
                            uint64_t value = va_arg(args, uint64_t);
                            uint64_to_str(value, buffer, 10);
                            console_puts(buffer);
                        } else {
                            // %ll 不完整，当作 %lu 处理
                            unsigned long value = va_arg(args, unsigned long);
                            uint_to_str(value, buffer, 10);
                            console_puts(buffer);
                        }
                    } else if (*fmt == 'x') {
                        fmt++;
                        uint64_t value = va_arg(args, uint64_t);
                        uint64_to_str(value, buffer, 16);
                        console_puts(buffer);
                    } else if (*fmt == 'u') {
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
                    char c = (char)va_arg(args, int);
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
    g_current_attr = (uint8_t)((bg << 4) | (fg & 0x0F));
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
        gST->con_out->SetCursorPosition(gST->con_out, (UINTN)x, (UINTN)y);
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
    // 如果已经初始化过，直接返回
    if (g_serial_initialized) {
        return;
    }
    
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
    
    // 禁用FIFO，清除缓冲区
    outb(port + 2, 0x00);
    
    // 禁用RTS和DTR
    outb(port + 4, 0x00);
    
    // 标记串口已初始化
    g_serial_initialized = true;
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
    
    outb(port, (uint8_t)c);
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
    char buffer[256];  // 保持大缓冲区
    
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
    char temp[64];  // 增加缓冲区大小以支持64位整数
    int i = 0;
    
    // 清零temp缓冲区
    for (int j = 0; j < 64; j++) {
        temp[j] = 0;
    }
    
    if (value == 0) {
        *buffer++ = '0';
        *buffer = '\0';
        return;
    }
    
    while (value > 0) {
        temp[i++] = digits[value % (uint64_t)base];
        value /= (uint64_t)base;
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
    char temp[64];  // 增加缓冲区大小以支持64位整数
    
    // 清零temp缓冲区
    for (int i = 0; i < 64; i++) {
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
                    char c = (char)va_arg(args, int);
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
    return (int)written;
}
