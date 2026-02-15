/**
 * HIK内核类型定义
 * 遵循三层模型文档：Core-0层基础类型系统
 */

#ifndef HIK_KERNEL_TYPES_H
#define HIK_KERNEL_TYPES_H

#include <stdint.h>

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
typedef u32 hik_status_t;

#define HIK_SUCCESS              0
#define HIK_ERROR_GENERIC        1
#define HIK_ERROR_INVALID_PARAM  2
#define HIK_ERROR_NO_MEMORY      3
#define HIK_ERROR_PERMISSION     4
#define HIK_ERROR_PERMISSION_DENIED  4
#define HIK_ERROR_NOT_FOUND      5
#define HIK_ERROR_TIMEOUT        6
#define HIK_ERROR_BUSY           7
#define HIK_ERROR_NOT_SUPPORTED  8
#define HIK_ERROR_CAP_INVALID    9
#define HIK_ERROR_CAP_REVOKED    10
#define HIK_ERROR_INVALID_DOMAIN 11
#define HIK_ERROR_QUOTA_EXCEEDED 12
#define HIK_ERROR_INVALID_STATE  13
#define HIK_ERROR_NO_RESOURCE    14
#define HIK_ERROR_ALREADY_EXISTS 15

/* 能力类型 */
typedef u32 cap_id_t;
#define HIK_CAP_INVALID  0
#define INVALID_CAP_ID   ((cap_id_t)-1)

/* 域标识符 */
typedef u32 domain_id_t;
#define HIK_DOMAIN_CORE   0      /* Core-0自身 */
#define HIK_DOMAIN_MAX    65536  /* 最大域数量 */
#define HIK_INVALID_DOMAIN ((domain_id_t)-1)  /* 无效域ID */

/* 线程标识符 */
typedef u32 thread_id_t;
#define INVALID_THREAD  ((thread_id_t)-1)
#define MAX_THREADS     1024

/* 优先级 */
typedef u8 priority_t;
#define HIK_PRIORITY_IDLE      0
#define HIK_PRIORITY_LOW       1
#define HIK_PRIORITY_NORMAL    2
#define HIK_PRIORITY_HIGH      3
#define HIK_PRIORITY_REALTIME  4

/* 中断向量 */
typedef u8 irq_vector_t;

#endif /* HIK_KERNEL_TYPES_H */