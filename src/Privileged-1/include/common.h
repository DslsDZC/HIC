/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef COMMON_H
#define COMMON_H

/* 基本类型定义 */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* 简写类型 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* NULL 定义 */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* HIC 状态码（与Core-0保持一致，使用u32） */
typedef u32 hic_status_t;

/* 状态码常量 */
#define HIC_SUCCESS 0
#define HIC_ERROR 1
#define HIC_INVALID_PARAM 2
#define HIC_NOT_FOUND 3
#define HIC_PERMISSION_DENIED 4
#define HIC_OUT_OF_MEMORY 5
#define HIC_TIMEOUT 6
#define HIC_BUSY 7
#define HIC_NOT_IMPLEMENTED 8
#define HIC_BUFFER_TOO_SMALL 9
#define HIC_PARSE_FAILED 10
#define HIC_NOT_INITIALIZED 11

/* 服务 API 结构 */
typedef struct service_api {
    hic_status_t (*init)(void);
    hic_status_t (*start)(void);
    hic_status_t (*stop)(void);
    hic_status_t (*cleanup)(void);
    hic_status_t (*get_info)(char*, u32);
} service_api_t;

/* 字符串和内存函数 - 由 libc_service 提供 */
/* 包含 string.h 使用完整实现 */
#include "string.h"

/* 服务注册函数（需要在内核中实现） */
extern void service_register(const char *name, const service_api_t *api);

#endif /* COMMON_H */