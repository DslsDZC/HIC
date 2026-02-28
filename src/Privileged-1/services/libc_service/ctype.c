/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "../include/ctype.h"

/* ASCII 字符分类表 */
static const unsigned char ctype_table[256] = {
    /* 控制字符 (0-31) */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 空格和可打印字符 (32-63) */
    2, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 8, 8, 8, 8, 8, 8,
    /* 字母 (64-95) */
    8, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 8, 8, 8, 8, 8,
    /* 字母 (96-127) */
    8, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 8, 8, 8, 8, 1,
    /* 扩展 ASCII (128-255) - 全部视为控制字符 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/* 字符分类宏定义 */
#define IS_CNTRL  (1 << 0)  /* 控制字符 */
#define IS_SPACE  (1 << 1)  /* 空白字符 */
#define IS_PUNCT  (1 << 3)  /* 标点符号 */
#define IS_DIGIT  (1 << 4)  /* 十进制数字 */
#define IS_UPPER  (1 << 5)  /* 大写字母 */
#define IS_LOWER  (1 << 6)  /* 小写字母 */
#define IS_HEX    (1 << 7)  /* 十六进制数字 */

/* 字符检查函数 */
int isdigit(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_DIGIT);
}

int isalpha(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & (IS_UPPER | IS_LOWER));
}

int isalnum(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & (IS_UPPER | IS_LOWER | IS_DIGIT));
}

int islower(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_LOWER);
}

int isupper(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_UPPER);
}

int isspace(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_SPACE);
}

int isprint(int c) {
    return (c >= 0 && c < 256) && !(ctype_table[c] & IS_CNTRL);
}

int iscntrl(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_CNTRL);
}

int isxdigit(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & (IS_DIGIT | IS_HEX));
}

int isgraph(int c) {
    return (c >= 0 && c < 256) && !(ctype_table[c] & (IS_CNTRL | IS_SPACE));
}

int ispunct(int c) {
    return (c >= 0 && c < 256) && (ctype_table[c] & IS_PUNCT);
}

/* 字符转换函数 */
int tolower(int c) {
    if (isupper(c)) {
        return c + ('a' - 'A');
    }
    return c;
}

int toupper(int c) {
    if (islower(c)) {
        return c - ('a' - 'A');
    }
    return c;
}