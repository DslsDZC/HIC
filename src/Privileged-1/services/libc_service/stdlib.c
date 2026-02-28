/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "../include/stdlib.h"
#include "../include/string.h"
#include <stdint.h>

/* 动态内存分配 */
void *malloc(size_t size) {
    /* 简化实现：使用静态内存池 */
    static uint8_t memory_pool[1024 * 1024];  /* 1MB 内存池 */
    static size_t pool_offset = 0;

    void *ptr = &memory_pool[pool_offset];
    pool_offset += size;

    if (pool_offset >= sizeof(memory_pool)) {
        return NULL;  /* 内存池耗尽 */
    }

    return ptr;
}

void free(void *ptr) {
    /* 简化实现：暂时不释放内存 */
    /* TODO: 实现真正的内存释放 */
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    /* 简化实现：分配新内存，复制数据，释放旧内存 */
    if (ptr == NULL) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = malloc(size);
    if (new_ptr) {
        /* 复制数据（假设新大小不小于旧大小） */
        /* 实际实现需要跟踪原始大小 */
        memcpy(new_ptr, ptr, size);
        free(ptr);
    }
    
    return new_ptr;
}

/* 进程终止 */
void exit(int status) {
    /* 简化实现：调用内核的退出系统调用 */
    /* 实际实现需要与内核交互 */
    extern void syscall_exit(int);
    syscall_exit(status);
    
    /* 如果返回，则进入死循环 */
    while (1);
}

void abort(void) {
    /* 终止程序并生成核心转储（简化版） */
    exit(1);
    while (1);
}

/* 环境访问 */
char *getenv(const char *name) {
    /* 简化实现：HIC 不使用环境变量 */
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    /* 简化实现：HIC 不使用环境变量 */
    return -1;
}

int unsetenv(const char *name) {
    /* 简化实现：HIC 不使用环境变量 */
    return -1;
}

/* 随机数 */
static unsigned long next_rand = 1;

int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}

void srand(unsigned int seed) {
    next_rand = seed;
}

/* 绝对值 */
int abs(int x) {
    return (x < 0) ? -x : x;
}

long labs(long x) {
    return (x < 0) ? -x : x;
}

long long llabs(long long x) {
    return (x < 0) ? -x : x;
}

/* 除法 */
div_t div(int numer, int denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/* 字符串转换 */
double strtod(const char *str, char **endptr) {
    /* 简化实现：仅支持整数部分 */
    double result = 0.0;
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
        result = result * 10.0 + (*str - '0');
        str++;
    }
    
    if (endptr) {
        *endptr = (char *)str;
    }
    
    return sign * result;
}

float strtof(const char *str, char **endptr) {
    return (float)strtod(str, endptr);
}

long double strtold(const char *str, char **endptr) {
    return (long double)strtod(str, endptr);
}

long strtol(const char *str, char **endptr, int base) {
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
    
    /* 自动检测基数 */
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }
    
    /* 转换数字 */
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        str++;
    }
    
    if (endptr) {
        *endptr = (char *)str;
    }
    
    return sign * result;
}

unsigned long strtoul(const char *str, char **endptr, int base) {
    unsigned long result = 0;
    
    /* 跳过空白字符 */
    while (*str == ' ' || *str == '\t' || *str == '\n' || 
           *str == '\r' || *str == '\f' || *str == '\v') {
        str++;
    }
    
    /* 自动检测基数 */
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }
    
    /* 转换数字 */
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        str++;
    }
    
    if (endptr) {
        *endptr = (char *)str;
    }
    
    return result;
}

long long strtoll(const char *str, char **endptr, int base) {
    /* 简化实现：使用 strtol */
    return (long long)strtol(str, endptr, base);
}

unsigned long long strtoull(const char *str, char **endptr, int base) {
    /* 简化实现：使用 strtoul */
    return (unsigned long long)strtoul(str, endptr, base);
}

/* 排序和搜索 */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    /* 简化实现：冒泡排序（实际应该使用快速排序） */
    if (nmemb < 2 || size == 0 || compar == NULL) {
        return;
    }
    
    char *array = (char *)base;
    
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - i - 1; j++) {
            void *a = array + j * size;
            void *b = array + (j + 1) * size;
            
            if (compar(a, b) > 0) {
                /* 交换元素 */
                char temp[size];
                memcpy(temp, a, size);
                memcpy(a, b, size);
                memcpy(b, temp, size);
            }
        }
    }
}

void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
    const char *array = (const char *)base;
    size_t low = 0;
    size_t high = nmemb;
    
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        const void *item = array + mid * size;
        int cmp = compar(key, item);
        
        if (cmp == 0) {
            return (void *)item;
        } else if (cmp < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    
    return NULL;
}
