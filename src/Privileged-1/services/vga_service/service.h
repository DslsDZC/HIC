/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef VGA_SERVICE_H
#define VGA_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/* 基础类型定义 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* VGA 显存地址 */
#define VGA_MEMORY 0xB8000

/* VGA 屏幕尺寸 */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA 颜色属性 */
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GREY    0x7
#define VGA_COLOR_DARK_GREY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

#define VGA_COLOR(bg, fg) (((bg) << 4) | (fg))

/* 服务接口 */
void vga_service_init(void);
void vga_service_putchar(char c);
void vga_service_puts(const char *str);
void vga_service_clear(void);
void vga_service_set_color(uint8_t color);
void vga_service_set_cursor(uint8_t x, uint8_t y);
void vga_service_get_cursor(uint8_t *x, uint8_t *y);

/* 滚屏 */
void vga_service_scroll_up(void);

#endif /* VGA_SERVICE_H */