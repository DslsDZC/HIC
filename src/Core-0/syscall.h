/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC系统调用接口
 * 遵循文档第3.2节：统一API访问模型与安全通信
 */

#ifndef HIC_KERNEL_SYSCALL_H
#define HIC_KERNEL_SYSCALL_H

#include "types.h"
#include "capability.h"

/* 系统调用号 */
typedef enum {
    SYSCALL_IPC_CALL,         /* IPC调用 */
    SYSCALL_CAP_TRANSFER,     /* 能力传递 */
    SYSCALL_CAP_DERIVE,       /* 能力派生 */
    SYSCALL_CAP_REVOKE,       /* 能力撤销 */
    SYSCALL_DOMAIN_CREATE,    /* 创建域 */
    SYSCALL_DOMAIN_DESTROY,   /* 销毁域 */
    SYSCALL_THREAD_CREATE,    /* 创建线程 */
    SYSCALL_THREAD_YIELD,     /* 让出CPU */
    SYSCALL_SHMEM_ALLOC,      /* 分配共享内存 */
    SYSCALL_SHMEM_MAP,        /* 映射共享内存 */
    SYSCALL_MAX,
} syscall_num_t;

/* IPC调用参数 */
typedef struct ipc_call_params {
    cap_id_t endpoint_cap;    /* 服务端点能力 */
    u64     message_addr;      /* 消息地址 */
    u64     message_size;      /* 消息大小 */
    u64     response_addr;     /* 响应地址 */
    u64     response_size;     /* 响应大小 */
} ipc_call_params_t;

/* 系统调用接口 */
hic_status_t syscall_ipc_call(ipc_call_params_t *params);
hic_status_t syscall_cap_transfer(domain_id_t to, cap_id_t cap);
hic_status_t syscall_cap_derive(cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out);
hic_status_t syscall_cap_revoke(cap_id_t cap);

/* 系统调用入口（汇编调用） */
void syscall_handler(u64 syscall_num, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

#endif /* HIC_KERNEL_SYSCALL_H */
