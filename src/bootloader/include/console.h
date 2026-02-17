#ifndef HIC_BOOTLOADER_CONSOLE_H
#define HIC_BOOTLOADER_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

// 控制台颜色定义
typedef enum {
    CON_COLOR_BLACK = 0,
    CON_COLOR_BLUE,
    CON_COLOR_GREEN,
    CON_COLOR_CYAN,
    CON_COLOR_RED,
    CON_COLOR_MAGENTA,
    CON_COLOR_BROWN,
    CON_COLOR_LIGHTGRAY,
    CON_COLOR_DARKGRAY,
    CON_COLOR_LIGHTBLUE,
    CON_COLOR_LIGHTGREEN,
    CON_COLOR_LIGHTCYAN,
    CON_COLOR_LIGHTRED,
    CON_COLOR_LIGHTMAGENTA,
    CON_COLOR_YELLOW,
    CON_COLOR_WHITE
} console_color_t;

// 控制台初始化
void console_init(void);

// 输出字符
void console_putchar(char c);

// 输出字符串
void console_puts(const char *str);

// 格式化输出
void console_printf(const char *fmt, ...);

// 设置颜色
void console_set_color(console_color_t fg, console_color_t bg);

// 清屏
void console_clear(void);

// 设置光标位置
void console_set_cursor(int x, int y);

// 获取光标位置
void console_get_cursor(int *x, int *y);

// 日志级别
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;

// 设置日志级别
void log_set_level(log_level_t level);

// 日志输出函数
void log_error(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
void log_trace(const char *fmt, ...);

// 串口调试输出
void serial_init(uint16_t port);
void serial_putchar(uint16_t port, char c);
void serial_puts(uint16_t port, const char *str);
void serial_printf(uint16_t port, const char *fmt, ...);

#endif // HIC_BOOTLOADER_CONSOLE_H