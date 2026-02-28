/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
#include <string.h>

/* VGA 显存指针（物理地址） */
volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_MEMORY;

/* 光标位置 */
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

/* 当前颜色 */
static uint8_t current_color = VGA_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

/* 内存写函数 */
static inline void write_mem16(void *addr, uint16_t value) {
    __asm__ volatile("movw %0, (%1)" : : "r"(value), "r"(addr));
}

/* 初始化 VGA 服务 */
void vga_service_init(void) {
    /* 清空屏幕 */
    vga_service_clear();
    
    /* 重置光标和颜色 */
    cursor_x = 0;
    cursor_y = 0;
    current_color = VGA_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
}

/* 设置颜色 */
void vga_service_set_color(uint8_t color) {
    current_color = color;
}

/* 设置光标位置 */
void vga_service_set_cursor(uint8_t x, uint8_t y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
    }
}

/* 获取光标位置 */
void vga_service_get_cursor(uint8_t *x, uint8_t *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

/* 清空屏幕 */
void vga_service_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        write_mem16((void *)&vga_buffer[i], (uint16_t)((current_color << 8) | ' '));
    }
    cursor_x = 0;
    cursor_y = 0;
}

/* 向上滚动一行 */
void vga_service_scroll_up(void) {
    /* 将所有行向上移动一行 */
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        write_mem16((void *)&vga_buffer[i], vga_buffer[i + VGA_WIDTH]);
    }
    
    /* 清空最后一行 */
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        write_mem16((void *)&vga_buffer[i], (uint16_t)((current_color << 8) | ' '));
    }
    
    /* 光标移到最后一行开头 */
    cursor_y = VGA_HEIGHT - 1;
    cursor_x = 0;
}

/* 输出一个字符 */
void vga_service_putchar(char c) {
    switch (c) {
        case '\n':
            /* 换行 */
            cursor_x = 0;
            cursor_y++;
            break;
            
        case '\r':
            /* 回车 */
            cursor_x = 0;
            break;
            
        case '\t':
            /* 制表符（4个空格） */
            cursor_x = (cursor_x + 4) & ~3;
            break;
            
        case '\b':
            /* 退格 */
            if (cursor_x > 0) {
                cursor_x--;
                write_mem16((void *)&vga_buffer[cursor_y * VGA_WIDTH + cursor_x], 
                           (uint16_t)((current_color << 8) | ' '));
            }
            break;
            
        default:
            /* 可打印字符 */
            if (c >= ' ' && c <= '~') {
                uint16_t attr = (current_color << 8) | (uint16_t)c;
                write_mem16((void *)&vga_buffer[cursor_y * VGA_WIDTH + cursor_x], attr);
                cursor_x++;
            }
            break;
    }
    
    /* 检查是否需要换行或滚屏 */
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        vga_service_scroll_up();
    }
}

/* 输出字符串 */
void vga_service_puts(const char *str) {
    if (!str) return;
    
    while (*str) {
        vga_service_putchar(*str++);
    }
}