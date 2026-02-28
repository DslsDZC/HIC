/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef SERIAL_SERVICE_H
#define SERIAL_SERVICE_H

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

/* 串口输出缓冲区大小 */
#define SERIAL_BUFFER_SIZE 8192

/* 串口端口 */
#define SERIAL_PORT 0x3F8

/* 缓冲区结构 */
typedef struct {
    uint32_t magic;              /* 魔数 */
    uint32_t write_pos;          /* 写位置 */
    uint32_t read_pos;           /* 读位置 */
    uint32_t wrap_count;         /* 环绕次数 */
    char     buffer[SERIAL_BUFFER_SIZE]; /* 数据缓冲区 */
} serial_buffer_t;

/* 服务接口 */
void serial_service_init(void);
void serial_service_poll(void);
const char* serial_service_read_buffer(uint32_t *size);
void serial_service_clear_buffer(void);

#endif /* SERIAL_SERVICE_H */