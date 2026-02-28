/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef STRING_H
#define STRING_H

#include "stddef.h"
#include "stdint.h"

/* 字符串长度 */
size_t strlen(const char *s);

/* 字符串比较 */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

/* 字符串复制 */
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

/* 字符串连接 */
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

/* 字符串查找 */
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

/* 字符串分割 */
char *strtok(char *str, const char *delim);

/* 内存操作 */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);

/* 字符检查 */
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int islower(int c);
int isupper(int c);
int isspace(int c);
int tolower(int c);
int toupper(int c);

/* 字符串转换 */
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);

#endif /* STRING_H */