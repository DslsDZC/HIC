/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef CTYPE_H
#define CTYPE_H

/* 字符分类 */
extern int isdigit(int c);
extern int isalpha(int c);
extern int isalnum(int c);
extern int islower(int c);
extern int isupper(int c);
extern int isspace(int c);
extern int isprint(int c);
extern int iscntrl(int c);
extern int isxdigit(int c);
extern int isgraph(int c);
extern int ispunct(int c);

/* 字符转换 */
extern int tolower(int c);
extern int toupper(int c);

#endif /* CTYPE_H */