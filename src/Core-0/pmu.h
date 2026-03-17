/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 性能监控抽象层 (Performance Monitoring Unit Abstraction Layer)
 * 
 * 提供跨平台的性能监控接口：
 * 1. 统一性能计数器：将架构特定的性能事件映射到标准分类
 * 2. 采样与分析：跨平台的性能采样基础设施
 * 3. PMU 抽象：统一的 PMU 编程接口
 * 4. 调试支持：硬件断点/观察点、跟踪与剖析
 */

#ifndef HIC_KERNEL_PMU_H
#define HIC_KERNEL_PMU_H

#include "types.h"

/* ========== 性能事件类型 ========== */

/* 标准性能事件分类（架构无关） */
typedef enum pmu_event_type {
    /* 周期和指令 */
    PMU_EVENT_CPU_CYCLES = 0,           /* CPU 周期数 */
    PMU_EVENT_INSTRUCTIONS,             /* 已执行指令数 */
    PMU_EVENT_BRANCHES,                 /* 分支指令数 */
    PMU_EVENT_BRANCH_MISSES,            /* 分支误预测数 */
    
    /* 缓存事件 */
    PMU_EVENT_CACHE_REFERENCES,         /* 缓存访问数 */
    PMU_EVENT_CACHE_MISSES,             /* 缓存未命中数 */
    PMU_EVENT_CACHE_L1D_READ,           /* L1 数据缓存读 */
    PMU_EVENT_CACHE_L1D_MISS,           /* L1 数据缓存未命中 */
    PMU_EVENT_CACHE_L1I_READ,           /* L1 指令缓存读 */
    PMU_EVENT_CACHE_L1I_MISS,           /* L1 指令缓存未命中 */
    PMU_EVENT_CACHE_LL_READ,            /* 最后级缓存读 */
    PMU_EVENT_CACHE_LL_MISS,            /* 最后级缓存未命中 */
    
    /* TLB 事件 */
    PMU_EVENT_TLB_DTLB_READ,            /* 数据 TLB 读 */
    PMU_EVENT_TLB_DTLB_MISS,            /* 数据 TLB 未命中 */
    PMU_EVENT_TLB_ITLB_READ,            /* 指令 TLB 读 */
    PMU_EVENT_TLB_ITLB_MISS,            /* 指令 TLB 未命中 */
    
    /* 内存事件 */
    PMU_EVENT_MEMORY_READS,             /* 内存读取 */
    PMU_EVENT_MEMORY_WRITES,            /* 内存写入 */
    PMU_EVENT_MEMORY_STALLED_CYCLES,    /* 内存停顿周期 */
    
    /* 流水线事件 */
    PMU_EVENT_STALLED_CYCLES_FRONTEND,  /* 前端停顿周期 */
    PMU_EVENT_STALLED_CYCLES_BACKEND,   /* 后端停顿周期 */
    PMU_EVENT_UOPS_RETIRED,             /* 已退休微操作 */
    PMU_EVENT_UOPS_ISSUED,              /* 已发射微操作 */
    
    /* 中断和异常 */
    PMU_EVENT_INTERRUPTS,               /* 中断数 */
    PMU_EVENT_EXCEPTIONS,               /* 异常数 */
    PMU_EVENT_SYSCALLS,                 /* 系统调用数 */
    
    /* 上下文切换 */
    PMU_EVENT_CONTEXT_SWITCHES,         /* 上下文切换数 */
    PMU_EVENT_CPU_MIGRATIONS,           /* CPU 迁移数 */
    PMU_EVENT_PAGE_FAULTS,              /* 页错误数 */
    
    /* 自定义事件 */
    PMU_EVENT_RAW_START = 1000,         /* 原始事件起始 */
    PMU_EVENT_MAX = 1024
} pmu_event_type_t;

/* 性能事件属性 */
typedef struct pmu_event_attr {
    pmu_event_type_t type;          /* 事件类型 */
    u64 config;                     /* 架构特定配置 */
    u64 config1;                    /* 扩展配置 1 */
    u64 config2;                    /* 扩展配置 2 */
    
    /* 采样配置 */
    u64 sample_period;              /* 采样周期 */
    u64 sample_freq;                /* 采样频率 (Hz) */
    
    /* 事件过滤 */
    u32 cpu_filter;                 /* CPU 过滤掩码 */
    u32 domain_filter;              /* 域过滤掩码 */
    u32 privilege_level;            /* 特权级别过滤 */
    
    /* 标志 */
    u32 flags;                      /* 事件标志 */
} pmu_event_attr_t;

/* 事件标志 */
#define PMU_EVENT_FLAG_ENABLED      (1U << 0)   /* 事件已启用 */
#define PMU_EVENT_FLAG_INHERIT      (1U << 1)   /* 子进程继承 */
#define PMU_EVENT_FLAG_EXCLUSIVE    (1U << 2)   /* 独占计数器 */
#define PMU_EVENT_FLAG_EXCLUDE_USER (1U << 3)   /* 排除用户态 */
#define PMU_EVENT_FLAG_EXCLUDE_KERNEL (1U << 4) /* 排除内核态 */
#define PMU_EVENT_FLAG_EXCLUDE_HV   (1U << 5)   /* 排除虚拟化层 */

/* 特权级别 */
#define PMU_PRIV_ALL    0   /* 所有特权级别 */
#define PMU_PRIV_USER   1   /* 仅用户态 */
#define PMU_PRIV_KERNEL 2   /* 仅内核态 */
#define PMU_PRIV_HV     3   /* 仅虚拟化层 */

/* ========== 性能计数器 ========== */

/* 性能计数器描述符 */
typedef struct pmu_counter {
    u32 counter_id;                 /* 计数器 ID */
    u32 event_type;                 /* 事件类型 */
    u64 value;                      /* 当前值 */
    u64 enabled_time;               /* 启用时间 */
    u64 running_time;               /* 运行时间 */
    u32 flags;                      /* 计数器标志 */
} pmu_counter_t;

/* 性能计数器组 */
typedef struct pmu_counter_group {
    u32 group_id;                   /* 组 ID */
    u32 counter_count;              /* 计数器数量 */
    pmu_counter_t *counters;        /* 计数器数组 */
    u64 enabled_time;               /* 组启用时间 */
    bool enabled;                   /* 是否启用 */
} pmu_counter_group_t;

/* ========== PMU 硬件信息 ========== */

/* PMU 硬件能力 */
typedef struct pmu_hw_info {
    /* 通用计数器 */
    u32 gp_counter_count;           /* 通用计数器数量 */
    u32 gp_counter_width;           /* 通用计数器位宽 */
    
    /* 固定计数器 */
    u32 fixed_counter_count;        /* 固定计数器数量 */
    u32 fixed_counter_width;        /* 固定计数器位宽 */
    
    /* 采样支持 */
    bool supports_sampling;         /* 支持采样 */
    u64 max_sample_period;          /* 最大采样周期 */
    
    /* 事件能力 */
    u32 event_count;                /* 支持的事件数量 */
    pmu_event_type_t *events;       /* 支持的事件列表 */
    
    /* 架构特定信息 */
    u32 arch_version;               /* 架构版本 */
    u32 arch_flags;                 /* 架构标志 */
} pmu_hw_info_t;

/* ========== 采样与分析 ========== */

/* 采样记录 */
typedef struct pmu_sample_record {
    u64 timestamp;                  /* 时间戳 */
    u64 ip;                         /* 指令指针 */
    u64 addr;                       /* 数据地址 */
    u64 value;                      /* 事件值 */
    u32 cpu;                        /* CPU ID */
    u32 domain;                     /* 域 ID */
    u32 event_type;                 /* 事件类型 */
    u32 flags;                      /* 记录标志 */
} pmu_sample_record_t;

/* 采样缓冲区 */
typedef struct pmu_sample_buffer {
    pmu_sample_record_t *records;   /* 记录数组 */
    u32 capacity;                   /* 容量 */
    u32 count;                      /* 当前数量 */
    u32 head;                       /* 头指针 */
    u32 tail;                       /* 尾指针 */
    bool overwrite;                 /* 溢出时覆盖 */
} pmu_sample_buffer_t;

/* 采样配置 */
typedef struct pmu_sample_config {
    pmu_event_type_t event;         /* 采样事件 */
    u64 sample_period;              /* 采样周期 */
    u32 sample_type;                /* 采样类型 */
    u32 buffer_size;                /* 缓冲区大小 */
    bool callchain;                 /* 是否记录调用链 */
    u32 callchain_depth;            /* 调用链深度 */
} pmu_sample_config_t;

/* 采样类型 */
#define PMU_SAMPLE_IP           (1 << 0)   /* 记录 IP */
#define PMU_SAMPLE_ADDR         (1 << 1)   /* 记录数据地址 */
#define PMU_SAMPLE_TIME         (1 << 2)   /* 记录时间戳 */
#define PMU_SAMPLE_CPU          (1 << 3)   /* 记录 CPU ID */
#define PMU_SAMPLE_PERIOD       (1 << 4)   /* 记录采样周期 */
#define PMU_SAMPLE_CALLCHAIN    (1 << 5)   /* 记录调用链 */

/* ========== 调试支持 ========== */

/* 硬件断点类型 */
typedef enum pmu_breakpoint_type {
    PMU_BP_EXECUTE = 0,             /* 执行断点 */
    PMU_BP_WRITE,                   /* 写观察点 */
    PMU_BP_READ,                    /* 读观察点 */
    PMU_BP_ACCESS,                  /* 读写观察点 */
    PMU_BP_MAX
} pmu_breakpoint_type_t;

/* 硬件断点描述符 */
typedef struct pmu_breakpoint {
    u32 bp_id;                      /* 断点 ID */
    pmu_breakpoint_type_t type;     /* 断点类型 */
    u64 addr;                       /* 地址 */
    u64 len;                        /* 长度 (1, 2, 4, 8 字节) */
    bool enabled;                   /* 是否启用 */
    u64 hit_count;                  /* 命中次数 */
} pmu_breakpoint_t;

/* 跟踪配置 */
typedef struct pmu_trace_config {
    u32 trace_type;                 /* 跟踪类型 */
    u64 start_addr;                 /* 起始地址 */
    u64 end_addr;                   /* 结束地址 */
    u32 buffer_size;                /* 缓冲区大小 */
    u32 flags;                      /* 跟踪标志 */
} pmu_trace_config_t;

/* 跟踪类型 */
#define PMU_TRACE_BRANCH    (1 << 0)   /* 分支跟踪 */
#define PMU_TRACE_INSTRUCTION (1 << 1) /* 指令跟踪 */
#define PMU_TRACE_DATA      (1 << 2)   /* 数据跟踪 */

/* ========== 架构相关回调 ========== */

typedef struct pmu_arch_ops {
    /* 计数器操作 */
    u32 (*counter_alloc)(void);
    void (*counter_free)(u32 counter_id);
    void (*counter_config)(u32 counter_id, const pmu_event_attr_t *attr);
    void (*counter_enable)(u32 counter_id);
    void (*counter_disable)(u32 counter_id);
    u64 (*counter_read)(u32 counter_id);
    void (*counter_write)(u32 counter_id, u64 value);
    
    /* 采样操作 */
    void (*sampling_start)(const pmu_sample_config_t *config);
    void (*sampling_stop)(void);
    bool (*sampling_has_data)(void);
    
    /* 断点操作 */
    u32 (*breakpoint_set)(u64 addr, u64 len, pmu_breakpoint_type_t type);
    void (*breakpoint_clear)(u32 bp_id);
    bool (*breakpoint_hit)(u32 bp_id);
    
    /* 硬件信息 */
    void (*get_hw_info)(pmu_hw_info_t *info);
} pmu_arch_ops_t;

/* ========== API 函数声明 ========== */

/* 初始化 */
void pmu_init(void);
void pmu_shutdown(void);
void pmu_register_arch_ops(const pmu_arch_ops_t *ops);

/* 硬件信息 */
const pmu_hw_info_t* pmu_get_hw_info(void);
bool pmu_event_supported(pmu_event_type_t event);

/* 计数器管理 */
u32 pmu_counter_alloc(void);
void pmu_counter_free(u32 counter_id);
hic_status_t pmu_counter_config(u32 counter_id, const pmu_event_attr_t *attr);
void pmu_counter_enable(u32 counter_id);
void pmu_counter_disable(u32 counter_id);
u64 pmu_counter_read(u32 counter_id);
void pmu_counter_reset(u32 counter_id);

/* 计数器组 */
u32 pmu_group_create(u32 counter_count, const pmu_event_attr_t *attrs);
void pmu_group_destroy(u32 group_id);
void pmu_group_enable(u32 group_id);
void pmu_group_disable(u32 group_id);
void pmu_group_read(u32 group_id, u64 *values, u32 count);

/* 采样 */
hic_status_t pmu_sampling_start(const pmu_sample_config_t *config);
void pmu_sampling_stop(void);
u32 pmu_sampling_read(pmu_sample_record_t *records, u32 max_count);
void pmu_sampling_clear(void);

/* 调试支持 */
u32 pmu_breakpoint_set(u64 addr, u64 len, pmu_breakpoint_type_t type);
void pmu_breakpoint_clear(u32 bp_id);
void pmu_breakpoint_enable(u32 bp_id);
void pmu_breakpoint_disable(u32 bp_id);
const pmu_breakpoint_t* pmu_breakpoint_get(u32 bp_id);
u32 pmu_breakpoint_get_count(void);

/* 跟踪 */
hic_status_t pmu_trace_start(const pmu_trace_config_t *config);
void pmu_trace_stop(void);
u64 pmu_trace_read(void *buffer, u64 size);

/* 性能统计 */
void pmu_get_global_stats(u64 *cycles, u64 *instructions, u64 *cache_misses);
void pmu_print_stats(void);

/* 事件映射 */
u64 pmu_event_to_arch_config(pmu_event_type_t event);
pmu_event_type_t pmu_arch_config_to_event(u64 arch_config);

#endif /* HIC_KERNEL_PMU_H */
