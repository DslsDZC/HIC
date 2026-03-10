/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AMP（非对称多处理）架构 - 增强版
 * BSP运行完整内核，AP分担能力验证、中断处理和计算任务
 */

#ifndef HIC_KERNEL_AMP_H
#define HIC_KERNEL_AMP_H

#include "types.h"
#include "hal.h"
#include "capability.h"
#include "kernel.h"

/* ==================== 类型定义 ==================== */

typedef hic_status_t status_t;

/* ==================== 常量定义 ==================== */

#define MAX_CPUS                256
#define AMP_TASK_QUEUE_SIZE     64
#define AMP_CAP_CACHE_SIZE      1024    /* 能力验证缓存大小 */
#define AMP_IRQ_ASSIGN_MAX      32      /* 每个AP最多分配的中断数 */

/* ==================== CPU状态 ==================== */

typedef enum {
    AMP_CPU_OFFLINE,           /* 离线 */
    AMP_CPU_ONLINE,            /* 在线 */
    AMP_CPU_BUSY,              /* 忙碌 */
    AMP_CPU_IDLE,              /* 空闲 */
} amp_cpu_state_t;

/* ==================== 工作模式 ==================== */

typedef enum {
    AMP_MODE_COMPUTE,          /* 计算模式 */
    AMP_MODE_CAP_VERIFY,       /* 能力验证模式 */
    AMP_MODE_IRQ_HANDLER,      /* 中断处理模式 */
    AMP_MODE_MIXED,            /* 混合模式 */
} amp_mode_t;

/* ==================== 任务类型 ==================== */

typedef enum {
    AMP_TASK_COMPUTE,          /* 计算任务 */
    AMP_TASK_MEMORY_COPY,      /* 内存拷贝 */
    AMP_TASK_CRYPTO,           /* 加密任务 */
    AMP_TASK_DATA_PROCESS,     /* 数据处理 */
    AMP_TASK_MAX,
} amp_task_type_t;

/* ==================== 任务结构 ==================== */

typedef struct {
    amp_task_type_t type;       /* 任务类型 */
    u32 priority;               /* 优先级 */
    u64 arg1;                   /* 参数1 */
    u64 arg2;                   /* 参数2 */
    u64 arg3;                   /* 参数3 */
    u64 arg4;                   /* 参数4 */
    phys_addr_t input_buffer;   /* 输入缓冲区 */
    phys_addr_t output_buffer;  /* 输出缓冲区 */
    size_t buffer_size;         /* 缓冲区大小 */
    volatile bool completed;    /* 完成标志 */
    u64 result;                 /* 执行结果 */
    u64 cycles;                 /* 执行周期数 */
} amp_task_t;

/* ==================== 能力验证缓存条目 ==================== */

typedef struct {
    cap_id_t cap_id;            /* 能力ID */
    domain_id_t domain_id;      /* 域ID */
    cap_rights_t required_rights; /* 所需权限 */
    bool valid;                 /* 验证结果 */
    u64 timestamp;              /* 时间戳 */
    volatile bool in_use;       /* 是否在使用 */
} amp_cap_cache_entry_t;

/* ==================== 中断处理记录 ==================== */

typedef struct {
    u32 irq_vector;             /* 中断向量 */
    u32 count;                  /* 处理次数 */
    u64 last_time;              /* 最后处理时间 */
    u64 total_cycles;           /* 总周期数 */
} amp_irq_record_t;

/* ==================== AP数据结构 ==================== */

typedef struct {
    cpu_id_t cpu_id;            /* CPU ID */
    amp_cpu_state_t state;      /* CPU状态 */
    amp_mode_t mode;            /* 工作模式 */
    void *stack_base;           /* 栈基地址 */
    void *stack_top;            /* 栈顶 */
    u32 apic_id;                /* APIC ID */
    u32 lapic_address;          /* 本地APIC地址 */

    /* 任务队列 */
    amp_task_t task_queue[AMP_TASK_QUEUE_SIZE];
    volatile u32 queue_head;
    volatile u32 queue_tail;
    volatile u32 queue_count;

    /* 当前任务 */
    amp_task_t *current_task;

    /* 能力验证缓存 */
    amp_cap_cache_entry_t cap_cache[AMP_CAP_CACHE_SIZE];
    volatile u32 cap_cache_hits;
    volatile u32 cap_cache_misses;

    /* 分配的中断 */
    u32 assigned_irqs[AMP_IRQ_ASSIGN_MAX];
    u32 assigned_irq_count;

    /* 中断处理记录 */
    amp_irq_record_t irq_records[AMP_IRQ_ASSIGN_MAX];

    /* 统计信息 */
    u64 tasks_completed;
    u64 tasks_failed;
    u64 caps_verified;
    u64 irqs_handled;
    u64 total_cycles;

} amp_cpu_t;

/* ==================== 全局AMP信息 ==================== */

typedef struct {
    amp_cpu_t cpus[MAX_CPUS];   /* CPU数据 */
    u32 cpu_count;              /* CPU总数 */
    u32 online_cpus;            /* 在线CPU数 */
    cpu_id_t bsp_id;            /* BSP ID */
    bool amp_enabled;           /* AMP是否启用 */
    u32 compute_cpus;           /* 计算CPU数 */
    u32 cap_verify_cpus;        /* 能力验证CPU数 */
    u32 irq_handler_cpus;       /* 中断处理CPU数 */
} amp_info_t;

/* 全局AMP信息 */
extern amp_info_t g_amp_info;
extern bool g_amp_enabled;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化AMP子系统
 */
void amp_init(void);

/**
 * @brief 启动所有AP
 */
status_t amp_boot_aps(void);

/**
 * @brief 等待所有AP就绪
 */
void amp_wait_for_aps(void);

/**
 * @brief 配置AP工作模式
 * @param cpu_id CPU ID
 * @param mode 工作模式
 * @return 状态码
 */
status_t amp_set_mode(cpu_id_t cpu_id, amp_mode_t mode);

/**
 * @brief 分配计算任务给AP
 * @param target_cpu 目标CPU（INVALID_CPU_ID表示任意可用计算CPU）
 * @param task 任务指针
 * @return 状态码
 */
status_t amp_assign_compute_task(cpu_id_t target_cpu, amp_task_t *task);

/**
 * @brief 请求能力验证（可能由AP预验证）
 * @param domain_id 域ID
 * @param cap_id 能力ID
 * @param required_rights 所需权限
 * @return 验证结果（true=有效，false=无效）
 */
bool amp_verify_capability(domain_id_t domain_id, cap_id_t cap_id, cap_rights_t required_rights);

/**
 * @brief 分配中断给AP
 * @param cpu_id CPU ID
 * @param irq_vector 中断向量
 * @return 状态码
 */
status_t amp_assign_irq(cpu_id_t cpu_id, u32 irq_vector);

/**
 * @brief AP中断处理入口
 * @param irq_vector 中断向量
 */
void amp_irq_handler(u32 irq_vector);

/**
 * @brief 等待任务完成
 * @param task 任务指针
 * @return 状态码
 */
status_t amp_wait_task(amp_task_t *task);

/**
 * @brief AP主函数（由AP执行）
 */
void amp_ap_main(void);

/**
 * @brief AP能力验证循环
 */
void amp_cap_verify_loop(void);

/**
 * @brief AP中断处理循环
 */
void amp_irq_handler_loop(void);

/**
 * @brief AP计算任务循环
 */
void amp_compute_loop(void);

/**
 * @brief 获取空闲计算CPU
 * @return CPU ID或INVALID_CPU_ID
 */
cpu_id_t amp_get_idle_compute_cpu(void);

/**
 * @brief 检查AMP是否启用
 * @return 是否启用
 */
bool amp_is_enabled(void);

/**
 * @brief 获取AMP统计信息
 * @param cpu_id CPU ID
 * @param tasks_completed 完成的任务数
 * @param caps_verified 验证的能力数
 * @param irqs_handled 处理的中断数
 */
void amp_get_stats(cpu_id_t cpu_id, u64 *tasks_completed, u64 *caps_verified, u64 *irqs_handled);

#endif /* HIC_KERNEL_AMP_H */