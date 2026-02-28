/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
#include <string.h>

/* 声明 VGA 服务接口 */
extern void vga_service_putchar(char c);

/* 魔数 */
#define SERIAL_BUFFER_MAGIC 0x53455249  /* "SERI" */

/* 全局缓冲区 */
static serial_buffer_t g_serial_buffer = {
    .magic = SERIAL_BUFFER_MAGIC,
    .write_pos = 0,
    .read_pos = 0,
    .wrap_count = 0
};

/* 端口 I/O 函数 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* 检查串口是否有数据可读 */
static bool serial_has_data(void) {
    return (inb(SERIAL_PORT + 5) & 0x01) != 0;
}

/* 从串口读取一个字符 */
static char serial_read_char(void) {
    return inb(SERIAL_PORT);
}

/* 初始化串口服务 */
void serial_service_init(void) {
    /* 清空缓冲区 */
    memset(&g_serial_buffer, 0, sizeof(serial_buffer_t));
    g_serial_buffer.magic = SERIAL_BUFFER_MAGIC;
    g_serial_buffer.write_pos = 0;
    g_serial_buffer.read_pos = 0;
    g_serial_buffer.wrap_count = 0;
}

/* 轮询串口并读取数据到缓冲区 */
void serial_service_poll(void) {
    while (serial_has_data()) {
        char c = serial_read_char();
        
        /* 同时输出到 VGA 显示 */
        vga_service_putchar(c);
        
        /* 写入缓冲区 */
        uint32_t pos = g_serial_buffer.write_pos;
        g_serial_buffer.buffer[pos] = c;
        
        /* 更新写位置（环形缓冲区） */
        g_serial_buffer.write_pos = (pos + 1) % SERIAL_BUFFER_SIZE;
        
        /* 检测环绕 */
        if (g_serial_buffer.write_pos == 0) {
            g_serial_buffer.wrap_count++;
        }
        
        /* 如果缓冲区满了，丢弃最旧的数据 */
        if (g_serial_buffer.write_pos == g_serial_buffer.read_pos) {
            g_serial_buffer.read_pos = (g_serial_buffer.read_pos + 1) % SERIAL_BUFFER_SIZE;
        }
    }
}

/* 读取缓冲区数据 */
const char* serial_service_read_buffer(uint32_t *size) {
    if (size) {
        /* 计算可读数据大小 */
        if (g_serial_buffer.write_pos >= g_serial_buffer.read_pos) {
            *size = g_serial_buffer.write_pos - g_serial_buffer.read_pos;
        } else {
            *size = SERIAL_BUFFER_SIZE - g_serial_buffer.read_pos + g_serial_buffer.write_pos;
        }
    }
    return &g_serial_buffer.buffer[g_serial_buffer.read_pos];
}

/* 清空缓冲区 */
void serial_service_clear_buffer(void) {
    g_serial_buffer.write_pos = 0;
    g_serial_buffer.read_pos = 0;
    g_serial_buffer.wrap_count = 0;
    memset(g_serial_buffer.buffer, 0, SERIAL_BUFFER_SIZE);
}