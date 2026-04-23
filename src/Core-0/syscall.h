/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC系统调用接口
 *
 * IPC 3.0 replaces the old SYSCALL_IPC_CALL with direct entry-page calls.
 * System calls now handle resource/capability management only — the
 * cross-domain data plane uses shared memory + lock-free ring buffers.
 */

#ifndef HIC_KERNEL_SYSCALL_H
#define HIC_KERNEL_SYSCALL_H

#include "types.h"
#include "capability.h"

/* 系统调用号 */
typedef enum {
    /* 能力系统调用 */
    SYSCALL_CAP_TRANSFER,     /* 能力传递 */
    SYSCALL_CAP_DERIVE,       /* 能力派生 */
    SYSCALL_CAP_REVOKE,       /* 能力撤销 */

    /* 域管理系统调用 */
    SYSCALL_DOMAIN_CREATE,    /* 创建域 */
    SYSCALL_DOMAIN_DESTROY,   /* 销毁域 */

    /* 线程管理系统调用 */
    SYSCALL_THREAD_CREATE,    /* 创建线程 */
    SYSCALL_THREAD_YIELD,     /* 让出CPU */

    /* 监控与安全系统调用（机制层） */
    SYSCALL_MONITOR_SET_RULE,     /* 设置监控规则 */
    SYSCALL_MONITOR_GET_RULE,     /* 获取监控规则 */
    SYSCALL_MONITOR_GET_STATS,    /* 获取事件统计 */
    SYSCALL_MONITOR_EXEC_ACTION,  /* 执行监控动作 */
    SYSCALL_CRASH_DUMP_RETRIEVE,  /* 获取崩溃转储 */
    SYSCALL_CRASH_DUMP_CLEAR,     /* 清除崩溃转储 */
    SYSCALL_AUDIT_QUERY,          /* 查询审计日志 */

    /* DoS 防护系统调用（机制层） */
    SYSCALL_QUOTA_CHECK,          /* 检查配额 */
    SYSCALL_QUOTA_CONSUME,        /* 消费配额 */
    SYSCALL_QUOTA_DELEGATE,       /* 委托配额 */
    SYSCALL_QUOTA_GET_USAGE,      /* 获取配额使用情况 */
    SYSCALL_EMERGENCY_GET_LEVEL,  /* 获取紧急状态级别 */
    SYSCALL_EMERGENCY_GET_STATUS, /* 获取系统资源状态 */
    SYSCALL_EMERGENCY_TRIGGER,    /* 触发紧急响应 */
    SYSCALL_FLOW_CONTROL_INIT,    /* 初始化流量控制 */
    SYSCALL_FLOW_CONTROL_CHECK,   /* 检查流量控制 */
    SYSCALL_FLOW_CONTROL_REFILL,  /* 补充信用 */
    SYSCALL_FLOW_CONTROL_GET_STATS, /* 获取流量统计 */
    SYSCALL_DOMAIN_CLEANUP,       /* 原子性清理域资源 */

    /* 能力传递系统调用（机制层） */
    SYSCALL_CAP_TRANSFER_ATTENUATE, /* 带权限衰减的能力传递 */

    /* 共享内存系统调用（机制层）— HIC通信模型核心 */
    SYSCALL_SHMEM_ALLOC,          /* 分配共享内存 */
    SYSCALL_SHMEM_MAP,            /* 映射共享内存 */
    SYSCALL_SHMEM_UNMAP,          /* 解除映射 */
    SYSCALL_SHMEM_GET_INFO,       /* 获取共享内存信息 */

    /* 执行流能力系统调用（EFC） */
    SYSCALL_EXEC_FLOW_CREATE,     /* 创建执行流 */
    SYSCALL_EXEC_FLOW_DESTROY,    /* 销毁执行流 */
    SYSCALL_EXEC_FLOW_DISPATCH,   /* 调度执行流 */
    SYSCALL_EXEC_FLOW_BLOCK,      /* 阻塞执行流 */
    SYSCALL_EXEC_FLOW_WAKE,       /* 唤醒执行流 */
    SYSCALL_EXEC_FLOW_GET_STATE,  /* 获取执行流状态 */
    SYSCALL_EXEC_FLOW_YIELD,      /* 让出执行流 */

    SYSCALL_MAX,
} syscall_num_t;

/* 系统调用处理函数 */
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

/* 系统调用返回 */
void hal_syscall_return(u64 ret_val);

/* 检查是否有待处理的系统调用 */
bool syscalls_pending(void);

/* 处理待处理的系统调用 */
void handle_pending_syscalls(void);

#endif /* HIC_KERNEL_SYSCALL_H */
