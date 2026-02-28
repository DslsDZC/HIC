/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* 标准输出流 */
extern int putchar(int c);
extern int puts(const char *s);
extern int printf(const char *format, ...);
extern int sprintf(char *str, const char *format, ...);
extern int snprintf(char *str, size_t size, const char *format, ...);
extern int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/* 标准输入流 */
extern int getchar(void);
extern int scanf(const char *format, ...);
extern int sscanf(const char *str, const char *format, ...);

/* 文件操作（简化版） */
typedef struct FILE FILE;

extern FILE *fopen(const char *filename, const char *mode);
extern int fclose(FILE *stream);
extern size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int fseek(FILE *stream, long offset, int whence);
extern long ftell(FILE *stream);
extern int fflush(FILE *stream);

/* 标准流 */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#endif /* STDIO_H */