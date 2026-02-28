/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef STDDEF_H
#define STDDEF_H

/* NULL 指针常量 */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* size_t 类型 */
typedef unsigned long long size_t;

/* ptrdiff_t 类型 */
typedef long ptrdiff_t;

/* wchar_t 类型 */
typedef int wchar_t;

/* 偏移量宏 */
#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif /* STDDEF_H */