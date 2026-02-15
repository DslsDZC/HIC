#ifndef HIK_BOOTLOADER_STRING_H
#define HIK_BOOTLOADER_STRING_H

#include <stdint.h>
#include <stddef.h>

// 字符串操作函数
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
char *strstr(const char *haystack, const char *needle);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
int snprintf(char *str, size_t size, const char *fmt, ...);
int sscanf(const char *str, const char *fmt, ...);

// Unicode字符串操作
size_t wcslen(const uint16_t *s);
int wcscmp(const uint16_t *s1, const uint16_t *s2);
void wcscpy(uint16_t *dest, const uint16_t *src);

// UTF-16转UTF-8
int utf16_to_utf8(const uint16_t *src, char *dest, size_t dest_size);

// 工具函数
int isdigit(int c);
int isxdigit(int c);
int isspace(int c);
int toupper(int c);
int tolower(int c);

// 字符串转数字
uint64_t strtoull(const char *str, char **endptr, int base);
int64_t strtoll(const char *str, char **endptr, int base);

#endif // HIK_BOOTLOADER_STRING_H