/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK域(Domain)管理头文件
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#ifndef HIK_KERNEL_DOMAIN_H
#define HIK_KERNEL_DOMAIN_H

#include "types.h"
#include "capability.h"

/* 域类型 */
typedef enum {
    DOMAIN_TYPE_CORE,          /* Core-0内核核心 */
    DOMAIN_TYPE_PRIVILEGED,    /* Privileged-1服务沙箱 */
    DOMAIN_TYPE_APPLICATION,   /* Application-3应用 */
} domain_type_t;

/* 域状态 */
typedef enum {
    DOMAIN_STATE_INIT,         /* 初始化中 */
    DOMAIN_STATE_READY,        /* 就绪 */
    DOMAIN_STATE_RUNNING,      /* 运行中 */
    DOMAIN_STATE_SUSPENDED,    /* 暂停 */
    DOMAIN_STATE_TERMINATED,   /* 已终止 */
} domain_state_t;

/* 域资源配额 */
typedef struct domain_quota {
    size_t max_memory;         /* 最大物理内存(字节) */
    u32    max_threads;        /* 最大线程数 */
    u32    max_caps;           /* 最大能力数 */
    u32    cpu_quota_percent;  /* CPU配额(百分比) */
} domain_quota_t;

/* 域控制块 */
typedef struct domain {
    domain_id_t    domain_id;     /* 域ID */
    domain_type_t  type;          /* 域类型 */
    domain_state_t state;         /* 域状态 */
    
    /* 物理内存布局 */
    phys_addr_t    phys_base;     /* 物理基地址 */
    size_t         phys_size;     /* 物理大小 */
    
    /* 页表 */
    virt_addr_t    page_table;    /* 页表基址 */
    
    /* 能力空间 */
    cap_handle_t  *cap_space;     /* 能力句柄数组 */
    u32            cap_count;     /* 当前能力数 */
    u32            cap_capacity;  /* 能力容量 */
    
    /* 线程列表 */
    struct thread *thread_list;   /* 线程链表 */
    u32            thread_count;  /* 线程数量 */
    
    /* 资源配额 */
    domain_quota_t quota;
    struct {
        size_t memory_used;      /* 已用内存 */
        u32    thread_used;      /* 已用线程数 */
    } usage;
    
    /* 统计信息 */
    u64    cpu_time_total;       /* 总CPU时间 */
    u64    syscalls_total;       /* 总系统调用数 */
    
    /* 标志 */
    u32    flags;
#define DOMAIN_FLAG_TRUSTED   (1U << 0)  /* 可信域 */
#define DOMAIN_FLAG_CRITICAL   (1U << 1)  /* 关键域 */
    
    /* 父域（用于资源继承） */
    domain_id_t parent_domain;
} domain_t;

/* 域管理接口 */
void domain_system_init(void);

/* 创建域 */
hik_status_t domain_create(domain_type_t type, domain_id_t parent,
                           const domain_quota_t *quota, domain_id_t *out);

/* 销毁域 */
hik_status_t domain_destroy(domain_id_t domain_id);

/* 查询域 */
hik_status_t domain_get_info(domain_id_t domain_id, domain_t *info);

/* 暂停/恢复域 */
hik_status_t domain_suspend(domain_id_t domain_id);
hik_status_t domain_resume(domain_id_t domain_id);

/* 资源分配检查 */
hik_status_t domain_check_memory_quota(domain_id_t domain_id, size_t size);
hik_status_t domain_check_thread_quota(domain_id_t domain_id);

#endif /* HIK_KERNEL_DOMAIN_H */