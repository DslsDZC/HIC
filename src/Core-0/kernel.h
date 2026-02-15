/**
 * HIK内核主头文件
 * 包含所有核心子系统
 */

#ifndef HIK_KERNEL_H
#define HIK_KERNEL_H

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "thread.h"
#include "pmm.h"

/* 内核版本 */
#define HIK_VERSION_MAJOR 0
#define HIK_VERSION_MINOR 1
#define HIK_VERSION_PATCH 0
#define HIK_VERSION "0.1.0"

/* 内核入口点 */
extern void kernel_main(void *info);

#endif /* HIK_KERNEL_H */