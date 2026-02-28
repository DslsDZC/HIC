/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

/* NULL 宏（如果未定义） */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* 退出状态码 */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* 动态内存分配 */
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

/* 进程终止 */
extern void exit(int status) __attribute__((noreturn));
extern void abort(void) __attribute__((noreturn));

/* 环境访问 */
extern char *getenv(const char *name);
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);

/* 随机数 */
extern int rand(void);
extern void srand(unsigned int seed);

/* 绝对值 */
extern int abs(int x);
extern long labs(long x);
extern long long llabs(long long x);

/* 除法结果结构体 */
typedef struct {
    int quot;  /* 商 */
    int rem;   /* 余数 */
} div_t;

typedef struct {
    long quot; /* 商 */
    long rem;  /* 余数 */
} ldiv_t;

typedef struct {
    long long quot; /* 商 */
    long long rem;  /* 余数 */
} lldiv_t;

/* 除法 */
extern div_t div(int numer, int denom);
extern ldiv_t ldiv(long numer, long denom);
extern lldiv_t lldiv(long long numer, long long denom);

/* 字符串转换 */
extern double strtod(const char *str, char **endptr);
extern float strtof(const char *str, char **endptr);
extern long double strtold(const char *str, char **endptr);

extern long strtol(const char *str, char **endptr, int base);
extern unsigned long strtoul(const char *str, char **endptr, int base);
extern long long strtoll(const char *str, char **endptr, int base);
extern unsigned long long strtoull(const char *str, char **endptr, int base);

extern int atoi(const char *str);
extern long atol(const char *str);
extern long long atoll(const char *str);
extern double atof(const char *str);

/* 排序和搜索 */
extern void qsort(void *base, size_t nmemb, size_t size,
                  int (*compar)(const void *, const void *));
extern void *bsearch(const void *key, const void *base,
                     size_t nmemb, size_t size,
                     int (*compar)(const void *, const void *));

#endif /* STDLIB_H */