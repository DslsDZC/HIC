/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核类型定义
 * 遵循三层模型文档：Core-0层基础类型系统
 */

#ifndef HIC_KERNEL_TYPES_H
#define HIC_KERNEL_TYPES_H

#include <stdint.h>

/* C23标准：bool已经是关键字，不需要定义 */
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
/* bool、true、false在C23中是关键字，无需定义 */
#endif

/* 基础类型 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

/* 有符号类型别名（兼容性） */
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef uint64_t size_t;
typedef int64_t  ssize_t;
typedef uint64_t uintptr_t;
typedef int64_t  intptr_t;

typedef uintptr_t phys_addr_t;  /* 物理地址 */
typedef uintptr_t virt_addr_t;  /* 虚拟地址 */

/* 状态码 */
typedef u32 hic_status_t;

#define HIC_SUCCESS              0
#define HIC_ERROR_GENERIC        1
#define HIC_ERROR_INVALID_PARAM  2
#define HIC_ERROR_NO_MEMORY      3
#define HIC_ERROR_PERMISSION     4
#define HIC_ERROR_PERMISSION_DENIED  4
#define HIC_ERROR_NOT_FOUND      5
#define HIC_ERROR_TIMEOUT        6
#define HIC_ERROR_BUSY           7
#define HIC_ERROR_NOT_SUPPORTED  8
#define HIC_ERROR_CAP_INVALID    9
#define HIC_ERROR_CAP_REVOKED    10
#define HIC_ERROR_INVALID_DOMAIN 11
#define HIC_ERROR_QUOTA_EXCEEDED 12
#define HIC_ERROR_INVALID_STATE  13
#define HIC_ERROR_NO_RESOURCE    14
#define HIC_ERROR_ALREADY_EXISTS 15

/* 能力类型 */
typedef u32 cap_id_t;
#define HIC_CAP_INVALID  0
#define INVALID_CAP_ID   ((cap_id_t)-1)

/* 域标识符 */
typedef u32 domain_id_t;
#define HIC_DOMAIN_CORE   0      /* Core-0自身 */
#define HIC_DOMAIN_MAX    256    /* 从65536减小到256，满足形式化验证要求 */
#define HIC_INVALID_DOMAIN ((domain_id_t)-1)  /* 无效域ID */

/* 线程标识符 */
typedef u32 thread_id_t;
#define INVALID_THREAD  ((thread_id_t)-1)
#define MAX_THREADS     256    /* 从1024减小到256，减少BSS段大小 */

/* 优先级 */
typedef u8 priority_t;
#define HIC_PRIORITY_IDLE      0
#define HIC_PRIORITY_LOW       1
#define HIC_PRIORITY_NORMAL    2
#define HIC_PRIORITY_HIGH      3
#define HIC_PRIORITY_REALTIME  4

/* 中断向量 */
typedef u8 irq_vector_t;

/* ==================== Sparse地址空间标记 ==================== */
/**
 * Sparse语义检查标记
 * 用于检测跨域指针误用、权限错误等HIK特定问题
 * 
 * 地址空间映射（遵循HIC三层模型）：
 * - __kernel    : Core-0内核空间（默认，address_space(0)）
 * - __capability: 能力系统对象空间（address_space(1)）
 * - __p1        : Privileged-1服务内存空间（address_space(2)）
 * - __p3        : Application-3用户空间（address_space(3)）
 * - __io        : I/O端口/MMIO空间（address_space(4)）
 * 
 * 使用方式：
 * #ifdef __CHECKER__
 *     __capability cap_entry_t *cap;
 *     __p1 void *p1_mem;
 * #else
 *     cap_entry_t *cap;
 *     void *p1_mem;
 * #endif
 * 
 * 集成方式：
 * make C=1          # 启用Sparse检查
 * make C=2          # Sparse检查所有文件（即使没有修改）
 */

#ifdef __CHECKER__
/* Sparse检查模式：启用地址空间标记 */
# define __kernel    __attribute__((address_space(0)))
# define __capability __attribute__((address_space(1)))
# define __p1        __attribute__((address_space(2)))
# define __p3        __attribute__((address_space(3)))
# define __io        __attribute__((address_space(4)))
#else
/* 正常编译模式：标记消失，无运行时开销 */
# define __kernel
# define __capability
# define __p1
# define __p3
# define __io
#endif

/* Sparse注解（提供额外语义信息） */
#ifdef __CHECKER__
# define __must_hold(x)    __attribute__((context(x, 1, 1)))
# define __acquires(x)     __attribute__((context(x, 0, 1)))
# define __releases(x)     __attribute__((context(x, 1, 0)))
# define __acquire(x)      __context__(x, 1)
# define __release(x)      __context__(x, -1)
# define __cond_lock(x, c) ((c) ? ({ __acquire(x); 1; }) : 0)
# define __force           __attribute__((force))
# define __bitwise         __attribute__((bitwise))
# define __user            __attribute__((noderef, address_space(3)))
#else
# define __must_hold(x)
# define __acquires(x)
# define __releases(x)
# define __acquire(x)
# define __release(x)
# define __cond_lock(x, c) ((c) ? ({ __acquire(x); 1; }) : 0)
# define __force
# define __bitwise
# define __user
#endif

#endif /* HIC_KERNEL_TYPES_H */
