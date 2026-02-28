/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "../include/string.h"
#include "../include/stddef.h"
#include "../include/stdint.h"
#include "../include/ctype.h"

/* 字符串长度 */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/* 字符串比较 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* 字符串复制 */
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = '\0';
    return dest;
}

/* 字符串连接 */
char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dest;
}

/* 字符串查找 */
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) {
        return (char *)haystack;
    }
    
    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

/* 字符串分割 */
static char *strtok_last = NULL;
static char *strtok_delim = NULL;

char *strtok(char *str, const char *delim) {
    if (str != NULL) {
        strtok_last = str;
        strtok_delim = delim;
    }
    
    if (strtok_last == NULL) {
        return NULL;
    }
    
    /* 跳过分隔符 */
    while (*strtok_last && strchr(delim, *strtok_last)) {
        *strtok_last++ = '\0';
    }
    
    if (*strtok_last == '\0') {
        return NULL;
    }
    
    /* 找到下一个分隔符 */
    char *token = strtok_last;
    while (*strtok_last && !strchr(delim, *strtok_last)) {
        strtok_last++;
    }
    
    /* 替换分隔符为 NULL */
    if (*strtok_last) {
        *strtok_last++ = '\0';
    }
    
    return token;
}

/* 内存操作 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    if (d < s) {
        /* 向前复制 */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* 向后复制 */
        d += n;
        s += n;
        while (n--) {
            *(--d) = *(--s);
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

/* 字符串转换 */
int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    
    /* 跳过空白字符 */
    while (*str == ' ' || *str == '\t' || *str == '\n' || 
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    
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

long atol(const char *str) {
    long result = 0;
    int sign = 1;
    
    /* 跳过空白字符 */
    while (*str == ' ' || *str == '\t' || *str == '\n' || 
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    
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

long long atoll(const char *str) {
    long long result = 0;
    int sign = 1;
    
    /* 跳过空白字符 */
    while (*str == ' ' || *str == '\t' || *str == '\n' || 
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    
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