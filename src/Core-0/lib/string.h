/**
 * HIK内核字符串库
 */

#ifndef HIK_LIB_STRING_H
#define HIK_LIB_STRING_H

#include <stdint.h>
#include <stddef.h>

/* 字符串长度 */
size_t strlen(const char *str);

/* 字符串比较 */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

/* 字符串复制 */
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

/* 字符串连接 */
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

/* 字符查找 */
char *strchr(const char *str, int c);
char *strrchr(const char *str, int c);

/* 子串查找 */
char *strstr(const char *haystack, const char *needle);

/* 数值转换 */
int atoi(const char *str);
long atol(const char *str);
int memcmp(const void *ptr1, const void *ptr2, size_t len);

/* 内存操作 */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

#endif /* HIK_LIB_STRING_H */