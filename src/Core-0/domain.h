/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC域(Domain)管理头文件
 * 遵循三层模型文档第2.2节：Privileged-1层特权服务沙箱
 */

#ifndef HIC_KERNEL_DOMAIN_H
#define HIC_KERNEL_DOMAIN_H

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
    
    /* 子配额支持（分级委托） */
    size_t memory_delegated;   /* 已委托给子域的内存 */
    u32    threads_delegated;  /* 已委托给子域的线程 */
    u32    caps_delegated;     /* 已委托给子域的能力 */
} domain_quota_t;

/* 配额检查结果 */
typedef enum {
    QUOTA_CHECK_OK,            /* 配额充足 */
    QUOTA_CHECK_WARNING,       /* 接近上限(80%) */
    QUOTA_CHECK_CRITICAL,      /* 接近上限(95%) */
    QUOTA_CHECK_EXCEEDED,      /* 超出配额 */
} quota_check_result_t;

/* 配额类型 */
typedef enum {
    QUOTA_TYPE_MEMORY,
    QUOTA_TYPE_THREADS,
    QUOTA_TYPE_CAPABILITIES,
    QUOTA_TYPE_CPU,
} quota_type_t;

/* 配额使用情况（用于系统调用返回） */
typedef struct domain_quota_usage {
    size_t memory_used;         /* 已用内存 */
    u32    thread_used;         /* 已用线程数 */
    u32    cap_used;            /* 已用能力数 */
    size_t max_memory;          /* 最大内存 */
    u32    max_threads;         /* 最大线程数 */
    u32    max_caps;            /* 最大能力数 */
} domain_quota_usage_t;

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
        u32    cap_used;         /* 已用能力数 */
        u64    cpu_time_used;    /* 已用CPU时间 */
        
        /* 速率跟踪（用于异常检测） */
        u64    last_check_time;  /* 上次检查时间 */
        size_t memory_alloc_rate;/* 内存分配速率(字节/秒) */
        u32    thread_create_rate;/* 线程创建速率(个/秒) */
    } usage;
    
    /* 统计信息 */
    u64    cpu_time_total;       /* 总CPU时间 */
    u64    syscalls_total;       /* 总系统调用数 */
    
    /* 域私有审计计数器（可通过只读能力授予审计服务） */
    struct {
        u64 privileged_mem_access;  /* 特权内存访问次数 */
        u64 ipc_calls;              /* IPC 调用次数 */
        u64 thread_creates;         /* 线程创建次数 */
        u64 memory_allocs;          /* 内存分配次数 */
        u64 cap_transfers;          /* 能力传递次数 */
        u64 cap_derives;            /* 能力派生次数 */
        u64 cap_revokes;            /* 能力撤销次数 */
        u64 reserved[1];            /* 预留对齐 */
    } audit_counters;
    
    /* 标志 */
    u32    flags;
#define DOMAIN_FLAG_TRUSTED      (1U << 0)  /* 可信域 */
#define DOMAIN_FLAG_CRITICAL     (1U << 1)  /* 关键域 */
#define DOMAIN_FLAG_PRIVILEGED   (1U << 2)  /* 特权域（可绕过能力系统） */
#define DOMAIN_FLAG_PRIMARY      (1U << 3)  /* 主实例（滚动更新） */
    
    /* 父域（用于资源继承） */
    domain_id_t parent_domain;
} domain_t;

/* 全局域数组（供其他模块访问域私有数据） */
extern domain_t g_domains[];

/* 域管理接口 */
void domain_system_init(void);

/* 创建域 */
hic_status_t domain_create(domain_type_t type, domain_id_t parent,
                           const domain_quota_t *quota, domain_id_t *out);

/* 销毁域 */
hic_status_t domain_destroy(domain_id_t domain_id);

/* 查询域 */
hic_status_t domain_get_info(domain_id_t domain_id, domain_t *info);

/* 暂停/恢复域 */
hic_status_t domain_suspend(domain_id_t domain_id);
hic_status_t domain_resume(domain_id_t domain_id);

/* 资源分配检查 */
hic_status_t domain_check_memory_quota(domain_id_t domain_id, size_t size);
hic_status_t domain_check_thread_quota(domain_id_t domain_id);

/* ==================== 机制层：配额强制检查原语 ==================== */

/**
 * 检查配额是否充足（机制层）
 * @param domain_id 域ID
 * @param type 配额类型
 * @param amount 请求数量
 * @param actual_available 输出实际可用量
 * @return 检查结果
 */
quota_check_result_t domain_quota_check(domain_id_t domain_id, 
                                         quota_type_t type, 
                                         size_t amount,
                                         size_t *actual_available);

/**
 * 强制消耗配额（机制层）
 * @param domain_id 域ID
 * @param type 配额类型
 * @param amount 消耗数量
 * @return 成功或配额不足错误
 */
hic_status_t domain_quota_consume(domain_id_t domain_id, 
                                   quota_type_t type, 
                                   size_t amount);

/**
 * 释放配额（机制层）
 * @param domain_id 域ID
 * @param type 配额类型
 * @param amount 释放数量
 */
void domain_quota_release(domain_id_t domain_id, 
                          quota_type_t type, 
                          size_t amount);

/**
 * 创建子配额委托（机制层）
 * @param parent 父域ID
 * @param child 子域ID
 * @param memory_quota 委托的内存配额
 * @param thread_quota 委托的线程配额
 * @return 操作状态
 */
hic_status_t domain_quota_delegate(domain_id_t parent, 
                                    domain_id_t child,
                                    size_t memory_quota,
                                    u32 thread_quota);

/**
 * 获取资源使用率（机制层）
 * @param domain_id 域ID
 * @param type 配额类型
 * @return 使用率百分比(0-100)
 */
u32 domain_get_usage_percent(domain_id_t domain_id, quota_type_t type);

/**
 * 获取资源分配速率（机制层，用于异常检测）
 * @param domain_id 域ID
 * @param type 配额类型
 * @return 分配速率（字节/秒 或 个/秒）
 */
u64 domain_get_allocation_rate(domain_id_t domain_id, quota_type_t type);

/* ==================== 机制层：紧急状态检测原语 ==================== */

/* 系统紧急状态级别 */
typedef enum {
    EMERGENCY_LEVEL_NONE,      /* 正常 */
    EMERGENCY_LEVEL_WARNING,   /* 警告：资源紧张 */
    EMERGENCY_LEVEL_CRITICAL,  /* 严重：需要降级 */
    EMERGENCY_LEVEL_EMERGENCY, /* 紧急：系统不稳定 */
} emergency_level_t;

/* 系统资源状态 */
typedef struct {
    size_t total_memory;
    size_t free_memory;
    size_t used_memory;
    u32    free_percent;
    u32    domain_count;
    u32    thread_count;
    u32    cap_count;
    emergency_level_t level;
} system_resource_status_t;

/**
 * 获取系统资源状态（机制层）
 * @param status 输出状态
 */
void domain_get_system_status(system_resource_status_t *status);

/**
 * 检测紧急状态（机制层）
 * @return 当前紧急级别
 */
emergency_level_t domain_detect_emergency(void);

/**
 * 触发紧急状态动作（机制层）
 * @param level 紧急级别
 * @param exclude_critical 是否排除关键服务
 * @return 受影响的域数量
 */
u32 domain_trigger_emergency_action(emergency_level_t level, bool exclude_critical);

/* ==================== 机制层：零停机更新原语 ==================== */

/**
 * @brief 并行域创建（机制层）
 * 
 * 基于模板域创建可并行运行的新域。
 * 用于零停机更新场景。
 * 
 * 机制保证：
 * 1. 新域与模板域共享必要的硬件能力
 * 2. 新域有独立的物理内存区域
 * 3. 新旧域可同时运行
 * 4. 创建后可进行能力转移
 * 
 * @param template_domain 模板域ID（将继承配置）
 * @param name 新域名称
 * @param quota_override 配额覆盖（NULL则使用模板配额）
 * @param out 新域ID输出
 * @return 状态码
 */
hic_status_t domain_parallel_create(domain_id_t template_domain,
                                     const char *name,
                                     const domain_quota_t *quota_override,
                                     domain_id_t *out);

/**
 * @brief 设置域的并行运行伙伴（机制层）
 * 
 * 将两个域标记为并行运行伙伴。
 * 用于零停机更新期间跟踪新旧实例关系。
 * 
 * @param domain_a 域A
 * @param domain_b 域B
 * @return 状态码
 */
hic_status_t domain_set_parallel_partner(domain_id_t domain_a, domain_id_t domain_b);

/**
 * @brief 获取域的并行运行伙伴（机制层）
 * 
 * @param domain 域ID
 * @param partner 输出伙伴域ID
 * @return 状态码
 */
hic_status_t domain_get_parallel_partner(domain_id_t domain, domain_id_t *partner);

/**
 * @brief 原子性域切换（机制层）
 * 
 * 原子性地将执行权从一个域切换到另一个域。
 * 用于零停机更新的最后一步。
 * 
 * @param from 当前活跃域
 * @param to 目标域
 * @param endpoint_caps 要重定向的端点能力数组
 * @param cap_count 能力数量
 * @return 状态码
 */
hic_status_t domain_atomic_switch(domain_id_t from,
                                   domain_id_t to,
                                   cap_id_t *endpoint_caps,
                                   u32 cap_count);

/**
 * @brief 域优雅关闭（机制层）
 * 
 * 优雅关闭域，等待现有请求完成。
 * 用于零停机更新后清理旧实例。
 * 
 * @param domain 域ID
 * @param timeout_ms 等待超时（毫秒）
 * @param force 超时后是否强制关闭
 * @return 状态码
 */
hic_status_t domain_graceful_shutdown(domain_id_t domain,
                                        u32 timeout_ms,
                                        bool force);

/* 域标志扩展（用于零停机更新） */
#define DOMAIN_FLAG_UPDATING      (1U << 8)  /* 正在更新中 */
#define DOMAIN_FLAG_NEW_INSTANCE  (1U << 9)  /* 新实例（更新目标） */
#define DOMAIN_FLAG_OLD_INSTANCE  (1U << 10) /* 旧实例（待清理） */
#define DOMAIN_FLAG_DRAINING      (1U << 11) /* 正在排空连接 */

#endif /* HIC_KERNEL_DOMAIN_H */