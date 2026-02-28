/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef LIBC_SERVICE_H
#define LIBC_SERVICE_H

#include "../include/stdint.h"
#include "../include/stddef.h"

/* HIC 状态码 */
typedef enum {
    HIC_SUCCESS = 0,
    HIC_ERROR = -1,
    HIC_INVALID_PARAM = -2,
    HIC_NOT_FOUND = -3,
    HIC_PERMISSION_DENIED = -4,
    HIC_OUT_OF_MEMORY = -5,
    HIC_TIMEOUT = -6,
    HIC_BUSY = -7,
    HIC_NOT_IMPLEMENTED = -8,
    HIC_BUFFER_TOO_SMALL = -9,
    HIC_PARSE_FAILED = -10
} hic_status_t;

/* 服务 API 结构 */
typedef struct service_api {
    hic_status_t (*init)(void);
    hic_status_t (*start)(void);
    hic_status_t (*stop)(void);
    hic_status_t (*cleanup)(void);
    hic_status_t (*get_info)(char*, uint32_t);
} service_api_t;

/* libc 服务初始化 */
hic_status_t libc_service_init(void);
hic_status_t libc_service_start(void);
hic_status_t libc_service_stop(void);
hic_status_t libc_service_cleanup(void);

/* 导出 libc 函数表（供其他服务调用） */
extern struct libc_api {
    /* 字符串函数 */
    size_t (*strlen)(const char *s);
    int (*strcmp)(const char *s1, const char *s2);
    int (*strncmp)(const char *s1, const char *s2, size_t n);
    char *(*strcpy)(char *dest, const char *src);
    char *(*strncpy)(char *dest, const char *src, size_t n);
    char *(*strcat)(char *dest, const char *src);
    char *(*strncat)(char *dest, const char *src, size_t n);
    char *(*strchr)(const char *s, int c);
    char *(*strrchr)(const char *s, int c);
    char *(*strstr)(const char *haystack, const char *needle);
    char *(*strtok)(char *str, const char *delim);
    
    /* 内存函数 */
    void *(*memset)(void *s, int c, size_t n);
    void *(*memcpy)(void *dest, const void *src, size_t n);
    void *(*memmove)(void *dest, const void *src, size_t n);
    int (*memcmp)(const void *s1, const void *s2, size_t n);
    void *(*memchr)(const void *s, int c, size_t n);
    
    /* 字符函数 */
    int (*isdigit)(int c);
    int (*isalpha)(int c);
    int (*isalnum)(int c);
    int (*islower)(int c);
    int (*isupper)(int c);
    int (*isspace)(int c);
    int (*tolower)(int c);
    int (*toupper)(int c);
    
    /* 转换函数 */
    int (*atoi)(const char *str);
    long (*atol)(const char *str);
    long long (*atoll)(const char *str);
    long (*strtol)(const char *str, char **endptr, int base);
    unsigned long (*strtoul)(const char *str, char **endptr, int base);
    long long (*strtoll)(const char *str, char **endptr, int base);
    unsigned long long (*strtoull)(const char *str, char **endptr, int base);
    double (*atof)(const char *str);
    double (*strtod)(const char *str, char **endptr);
    float (*strtof)(const char *str, char **endptr);
    
    /* 标准IO函数 */
    int (*printf)(const char *fmt, ...);
    int (*sprintf)(char *str, const char *fmt, ...);
    int (*snprintf)(char *str, size_t size, const char *fmt, ...);
    int (*puts)(const char *s);
    int (*putchar)(int c);
    
    /* 分配函数 */
    void *(*malloc)(size_t size);
    void (*free)(void *ptr);
    void *(*calloc)(size_t nmemb, size_t size);
    void *(*realloc)(void *ptr, size_t size);
    
} libc_api;

#endif /* LIBC_SERVICE_H */