/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核主头文件
 * 包含所有核心子系统
 */

#ifndef HIC_KERNEL_H
#define HIC_KERNEL_H

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "thread.h"
#include "pmm.h"

/* 内核版本 */
#define HIC_VERSION_MAJOR 0
#define HIC_VERSION_MINOR 1
#define HIC_VERSION_PATCH 0
#define HIC_VERSION "0.1.0"

/* 内核入口点 */
extern void kernel_main(void *info);

#endif /* HIC_KERNEL_H */