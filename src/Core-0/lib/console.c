/**
 * HIK内核控制台实现
 */

#include <stdarg.h>
#include "console.h"
#include "mem.h"

static uint16_t *vga_buffer = (uint16_t *)VGA_BUFFER;
static int cursor_row = 0;
static int cursor_col = 0;
static u8 fg_color = 0x0F;
static u8 bg_color = 0x00;

/* 初始化控制台 */
void console_init(void)
{
    console_clear();
}

/* 清屏 */
void console_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (uint16_t)' ' | ((uint16_t)(fg_color | (bg_color << 4)) << 8);
    }
    cursor_row = 0;
    cursor_col = 0;
}

/* 输出字符 */
void console_putchar(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 4) & ~3;
    } else if (c >= ' ') {
        const int index = cursor_row * VGA_WIDTH + cursor_col;
        vga_buffer[index] = (uint16_t)c | ((uint16_t)(fg_color | (bg_color << 4)) << 8);
        cursor_col++;
    }
    
    /* 滚屏 */
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    
    if (cursor_row >= VGA_HEIGHT) {
        /* 向上滚动一行 */
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        
        /* 清空最后一行 */
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (uint16_t)' ' | ((uint16_t)(fg_color | (bg_color << 4)) << 8);
        }
        
        cursor_row = VGA_HEIGHT - 1;
    }
}

/* 输出字符串 */
void console_puts(const char *str)
{
    while (*str) {
        console_putchar(*str++);
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
        buffer[--pos] = '0' + (value % 10);
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

/* 设置光标位置 */
void console_set_cursor(int row, int col)
{
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        cursor_row = row;
        cursor_col = col;
    }
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
                    console_putchar(va_arg(args, int));
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
