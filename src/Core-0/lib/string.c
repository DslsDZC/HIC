/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK内核字符串操作实现
 */

#include "string.h"
#include "mem.h"
#include "../types.h"

/* 字符串长度 */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

/* 字符串比较 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n == SIZE_MAX ? 0 : *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* 字符串复制 */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = '\0';
    return dest;
}

/* 字符串连接 */
char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dest;
}

/* 字符查找 */
char *strchr(const char *str, int c)
{
    while (*str) {
        if (*str == (char)c) {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}

char *strrchr(const char *str, int c)
{
    const char *last = NULL;
    while (*str) {
        if (*str == (char)c) {
            last = str;
        }
        str++;
    }
    return (char *)last;
}

/* 子串查找 */
char *strstr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*n && *h == *n) {
            h++;
            n++;
        }
        
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

/* 数值转换 */
int atoi(const char *str)
{
    int result = 0;
    int sign = 1;
    
    /* 跳过空白 */
    while (*str == ' ' || *str == '\t') str++;
    
    /* 处理符号 */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    /* 转换数字 */
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

long atol(const char *str)
{
    long result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

/* 内存复制 */
void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    
    return dest;
}

/* 内存移动（处理重叠） */
void *memmove(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    
    if (d < s) {
        /* 从前向后复制 */
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        /* 从后向前复制 */
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    
    return dest;
}

/* 内存填充 */
void *memset(void *s, int c, size_t n) {
    u8 *p = (u8*)s;
    
    for (size_t i = 0; i < n; i++) {
        p[i] = (u8)c;
    }
    
    return s;
}