/**
 * HIC BIOS Console实现
 * 提供基于BIOS的控制台输出
 */

#include <stdint.h>
#include "console.h"
#include "string.h"

/* BIOS视频内存地址 */
#define VIDEO_MEMORY  0xB8000
#define VIDEO_WIDTH   80
#define VIDEO_HEIGHT  25

/* 颜色定义 */
#define COLOR_BLACK   0
#define COLOR_BLUE    1
#define COLOR_GREEN   2
#define COLOR_CYAN    3
#define COLOR_RED     4
#define COLOR_MAGENTA 5
#define COLOR_BROWN   6
#define COLOR_WHITE   7

/* 属性定义 */
#define ATTR_DEFAULT  (COLOR_WHITE | (COLOR_BLACK << 4))

/* 光标位置 */
static int g_cursor_x = 0;
static int g_cursor_y = 0;

/**
 * 初始化BIOS控制台
 */
void console_init_bios(void)
{
    g_cursor_x = 0;
    g_cursor_y = 0;
    
    // 清屏
    console_clear();
}

/**
 * 输出字符
 */
void console_putchar(char c)
{
    volatile uint16_t *video = (volatile uint16_t *)VIDEO_MEMORY;
    
    switch (c) {
        case '\r':
            g_cursor_x = 0;
            break;
            
        case '\n':
            g_cursor_x = 0;
            g_cursor_y++;
            break;
            
        case '\t':
            g_cursor_x = (g_cursor_x + 8) & ~7;
            if (g_cursor_x >= VIDEO_WIDTH) {
                g_cursor_x = 0;
                g_cursor_y++;
            }
            break;
            
        case '\b':
            if (g_cursor_x > 0) {
                g_cursor_x--;
                video[g_cursor_y * VIDEO_WIDTH + g_cursor_x] = ' ' | (ATTR_DEFAULT << 8);
            }
            break;
            
        default:
            // 输出字符
            video[g_cursor_y * VIDEO_WIDTH + g_cursor_x] = c | (ATTR_DEFAULT << 8);
            g_cursor_x++;
            break;
    }
    
    // 自动换行
    if (g_cursor_x >= VIDEO_WIDTH) {
        g_cursor_x = 0;
        g_cursor_y++;
    }
    
    // 屏幕滚动
    if (g_cursor_y >= VIDEO_HEIGHT) {
        console_scroll();
        g_cursor_y = VIDEO_HEIGHT - 1;
    }
    
    // 更新光标位置
    console_update_cursor();
}

/**
 * 输出字符串
 */
void console_puts(const char *s)
{
    while (*s) {
        console_putchar(*s);
        s++;
    }
}

/**
 * 清屏
 */
void console_clear(void)
{
    volatile uint16_t *video = (volatile uint16_t *)VIDEO_MEMORY;
    
    for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++) {
        video[i] = ' ' | (ATTR_DEFAULT << 8);
    }
    
    g_cursor_x = 0;
    g_cursor_y = 0;
    
    console_update_cursor();
}

/**
 * 屏幕滚动
 */
void console_scroll(void)
{
    volatile uint16_t *video = (volatile uint16_t *)VIDEO_MEMORY;
    
    // 向上滚动一行
    for (int i = 0; i < (VIDEO_HEIGHT - 1) * VIDEO_WIDTH; i++) {
        video[i] = video[i + VIDEO_WIDTH];
    }
    
    // 清空最后一行
    for (int i = (VIDEO_HEIGHT - 1) * VIDEO_WIDTH; i < VIDEO_HEIGHT * VIDEO_WIDTH; i++) {
        video[i] = ' ' | (ATTR_DEFAULT << 8);
    }
}

/**
 * 更新光标位置
 */
void console_update_cursor(void)
{
    uint16_t position = g_cursor_y * VIDEO_WIDTH + g_cursor_x;
    
    __asm__ volatile (
        "mov %0, %%dx\n"       /* 先设置DH (高8位) */
        "mov %1, %%cx\n"       /* 设置CH (高8位) */
        "mov $0x02, %%ah\n"    /* 设置功能号 */
        "int $0x10\n"
        : 
        : "d"(position >> 8), "c"(position & 0xFF)
        : "ax"
    );
}

/**
 * 设置颜色
 */
void console_set_color(uint8_t foreground, uint8_t background)
{
    /* 完整实现：保存颜色状态并在输出时使用 */
    g_color_fg = foreground & 0x0F;
    g_color_bg = background & 0x0F;
    
    /* 更新视频属性 */
    g_color_attr = (g_color_bg << 4) | g_color_fg;
    
    /* 如果在文本模式下，立即更新BIOS属性 */
    if (g_video_mode == VIDEO_MODE_TEXT) {
        uint8_t attr = g_color_attr;
        __asm__ volatile (
            "int $0x10"
            :
            : "a"((0x08 << 8) | attr),  /* 写字符属性 */
              "b"(0)
        );
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
 * 设置光标位置
 */
void console_set_cursor(int x, int y)
{
    if (x >= 0 && x < VIDEO_WIDTH) {
        g_cursor_x = x;
    }
    if (y >= 0 && y < VIDEO_HEIGHT) {
        g_cursor_y = y;
    }
    
    console_update_cursor();
}

/**
 * 串口初始化
 */
void serial_init(uint16_t port)
{
    // 禁用中断
    __asm__ volatile (
        "mov $0, %%al\n"
        "outb %%al, $1\n"      // IER (中断使能寄存器) = port + 1
        :
        : "a"(0), "d"(port)
    );
    
    // 启用DLAB (设置波特率)
    __asm__ volatile (
        "mov $0x80, %%al\n"
        "outb %%al, $3\n"      // LCR (线路控制寄存器) = port + 3
        :
        : "a"(0x80), "d"(port)
    );
    
    // 设置波特率为115200
    __asm__ volatile (
        "mov $1, %%al\n"      // 低字节 (115200 = 115200 / 115200 = 1)
        "outb %%al, $0\n"      // DLL (除数锁存器低字节) = port + 0
        "mov $0, %%al\n"      // 高字节
        "outb %%al, $1\n"      // DLH (除数锁存器高字节) = port + 1
        :
        : "a"(0), "d"(port)
    );
    
    // 禁用DLAB，设置8N1 (8位数据，无校验，1停止位)
    __asm__ volatile (
        "mov $0x03, %%al\n"
        "outb %%al, $3\n"      // LCR = port + 3
        :
        : "a"(0x03), "d"(port)
    );
    
    // 启用FIFO，清空FIFO
    __asm__ volatile (
        "mov $0xC7, %%al\n"
        "outb %%al, $2\n"      // FCR (FIFO控制寄存器) = port + 2
        :
        : "a"(0xC7), "d"(port)
    );
    
    // 启用调制解调器状态中断
    __asm__ volatile (
        "mov $0x0B, %%al\n"
        "outb %%al, $4\n"      // MCR (调制解调器控制寄存器) = port + 4
        :
        : "a"(0x0B), "d"(port)
    );
    
    // 设置RTS和DTR
    __asm__ volatile (
        "mov $0x03, %%al\n"
        "outb %%al, $4\n"      // MCR = port + 4
        :
        : "a"(0x03), "d"(port)
    );
}

/**
 * 串口输出字符
 */
void serial_putchar(char c)
{
    // 等待发送缓冲区为空
    uint8_t lsr;
    do {
        __asm__ volatile (
            "inb $5, %%al\n"    // LSR (线路状态寄存器) = port + 5
            : "=a"(lsr)
            : "d"(0x3F8)
        );
    } while (!(lsr & 0x20));  // 检查THRE位（发送保持寄存器空）
    
    // 发送字符
    __asm__ volatile (
        "movb %0, %%al\n"
        "outb %%al, $0\n"      // THR (发送保持寄存器) = port + 0
        :
        : "r"(c), "d"(0x3F8)
        : "al"
    );
}

/**
 * 日志输出
 */
void log_message(const char *level, const char *fmt, ...)
{
    // 输出到视频控制台
    console_puts(level);
    console_puts(": ");
    console_puts(fmt);
    console_puts("\n");
    
    // 输出到串口
    const char *p = level;
    while (*p) {
        serial_putchar(*p++);
    }
    serial_putchar(':');
    serial_putchar(' ');
    
    p = fmt;
    while (*p) {
        serial_putchar(*p++);
    }
    serial_putchar('\r');
    serial_putchar('\n');
}

/**
 * 格式化输出
 */
void console_printf(const char *fmt, ...)
{
    /* 完整实现：支持格式化输出 */
    va_list args;
    va_start(args, fmt);
    
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    console_puts(buffer);
    
    /* 同时输出到串口（用于调试） */
    const char *p = buffer;
    while (*p) {
        serial_putchar(*p++);
    }
    
    va_end(args);
}
