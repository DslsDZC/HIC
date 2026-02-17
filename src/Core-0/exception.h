/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK异常处理系统
 * 遵循文档第2.1节：故障隔离与恢复
 */

#ifndef HIK_KERNEL_EXCEPTION_H
#define HIK_KERNEL_EXCEPTION_H

#include "types.h"
#include "domain.h"
#include "thread.h"
#include "hal.h"

/* 异常类型 */
typedef enum {
    EXCEPTION_DIVIDE_ERROR,     /* 除零错误 */
    EXCEPTION_DEBUG,            /* 调试异常 */
    EXCEPTION_NMI,              /* 不可屏蔽中断 */
    EXCEPTION_BREAKPOINT,       /* 断点 */
    EXCEPTION_OVERFLOW,         /* 溢出 */
    EXCEPTION_BOUND_RANGE,      /* 边界检查 */
    EXCEPTION_INVALID_OPCODE,   /* 无效操作码 */
    EXCEPTION_DEVICE_NOT_AVAIL, /* 设备不可用 */
    EXCEPTION_DOUBLE_FAULT,     /* 双重故障 */
    EXCEPTION_INVALID_TSS,      /* 无效TSS */
    EXCEPTION_SEGMENT_NOT_PRESENT, /* 段不存在 */
    EXCEPTION_STACK_SEGMENT,    /* 栈段错误 */
    EXCEPTION_GENERAL_PROTECTION, /* 一般保护错误 */
    EXCEPTION_PAGE_FAULT,       /* 页错误 */
    EXCEPTION_X87_FPU_ERROR,    /* x87 FPU错误 */
    EXCEPTION_ALIGNMENT_CHECK,  /* 对齐检查 */
    EXCEPTION_MACHINE_CHECK,    /* 机器检查 */
    EXCEPTION_SIMD_FP,          /* SIMD浮点错误 */
    EXCEPTION_VIRTUALIZATION,   /* 虚拟化异常 */
    EXCEPTION_SECURITY,         /* 安全异常 */
} exception_type_t;

/* 异常上下文 */
typedef struct {
    exception_type_t type;
    domain_id_t domain;
    thread_id_t thread;
    u64 error_code;
    u64 fault_address;
    hal_context_t* context;
} exception_context_t;

/* 异常处理程序类型 */
typedef enum {
    EXCEPT_HANDLER_CONTINUE,    /* 继续执行 */
    EXCEPT_HANDLER_TERMINATE,   /* 终止线程 */
    EXCEPT_HANDLER_RESTART,     /* 重启服务 */
    EXCEPT_HANDLER_PANIC,       /* 系统恐慌 */
} exception_handler_result_t;

/* 异常处理接口 */
void exception_system_init(void);

/* 处理异常 */
exception_handler_result_t exception_handle(exception_context_t* ctx);

/* 注册异常处理程序 */
typedef exception_handler_result_t (*exception_handler_fn)(exception_context_t*);
void exception_register_handler(exception_type_t type, exception_handler_fn handler);

/* 恐慌系统（不可恢复错误） */
void kernel_panic(const char* message, ...);

#endif /* HIK_KERNEL_EXCEPTION_H */