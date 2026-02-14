/**
 * HIK内核控制台实现
 */

#include "console.h"
#include "mem.h"

static uint16_t *vga_buffer = (uint16_t *)VGA_BUFFER;
static int cursor_row = 0;
static int cursor_col = 0;
static vga_color_t fg_color = VGA_COLOR_WHITE;
static vga_color_t bg_color = VGA_COLOR_BLACK;

/* VGA颜色属性 */
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg)
{
    return fg | (bg << 4);
}

/* VGA字符属性 */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)uc | (uint16_t)color << 8;
}

/* 初始化控制台 */
void console_init(void)
{
    console_clear();
}

/* 清屏 */
void console_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_entry_color(fg_color, bg_color));
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
        vga_buffer[index] = vga_entry((unsigned char)c, 
                                       vga_entry_color(fg_color, bg_color));
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
            vga_buffer[i] = vga_entry(' ', vga_entry_color(fg_color, bg_color));
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

/* 设置光标位置 */
void console_set_cursor(int row, int col)
{
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        cursor_row = row;
        cursor_col = col;
    }
}
