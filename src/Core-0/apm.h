/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC APM (Automatic Parameter Management) 系统
 * 支持 Core-0 核心的全面参数管理
 * 
 * APM 模式说明：
 * 1. 引导层自动分配资源（串口、内存、中断等）
 * 2. 引导层将分配结果写入配置文件
 * 3. 配置文件通过 boot_info 传递给内核
 * 4. 内核从配置文件读取并初始化
 * 
 * 形式化验证：
 * - 资源分配不变式
 * - 配置一致性验证
 * - 启动时验证完整性
 */

#ifndef HIC_KERNEL_APM_H
#define HIC_KERNEL_APM_H

#include "types.h"
#include "build_config.h"

/* APM 模式枚举 */
typedef enum apm_mode {
    APM_MODE_AUTO = 0,          /* 自动分配模式：引导层自动分配，写入配置文件 */
    APM_MODE_CONFIG = 1,        /* 配置文件模式：直接使用配置文件 */
    APM_MODE_HYBRID = 2,        /* 混合模式：自动分配 + 配置文件覆盖 */
} apm_mode_t;

/* APM 资源类型 */
typedef enum apm_resource_type {
    APM_RESOURCE_UART = 0,      /* 串口资源 */
    APM_RESOURCE_MEMORY,        /* 内存资源 */
    APM_RESOURCE_IRQ,           /* 中断资源 */
    APM_RESOURCE_TIMER,         /* 定时器资源 */
    APM_RESOURCE_PCI,           /* PCI 资源 */
    APM_RESOURCE_ACPI,          /* ACPI 资源 */
    APM_RESOURCE_COUNT
} apm_resource_type_t;

/* APM 资源状态 */
typedef enum apm_resource_state {
    APM_STATE_UNALLOCATED = 0,  /* 未分配 */
    APM_STATE_ALLOCATED,        /* 已分配 */
    APM_STATE_INITIALIZED,      /* 已初始化 */
    APM_STATE_ACTIVE,           /* 活跃 */
    APM_STATE_ERROR             /* 错误 */
} apm_resource_state_t;

/* APM 串口配置 */
typedef struct apm_uart_config {
    phys_addr_t base_addr;      /* 基地址 */
    u32        baud_rate;       /* 波特率 */
    u32        data_bits;       /* 数据位 */
    u32        parity;          /* 校验位 (0=none, 1=odd, 2=even) */
    u32        stop_bits;       /* 停止位 */
    u32        irq;             /* 中断号 */
    apm_resource_state_t state; /* 状态 */
} apm_uart_config_t;

/* APM 内存区域配置 */
typedef struct apm_memory_region {
    phys_addr_t base;           /* 基地址 */
    size_t     size;            /* 大小 */
    u32        flags;           /* 标志位 */
    u32        owner;           /* 拥有者域ID */
    apm_resource_state_t state; /* 状态 */
} apm_memory_region_t;

/* APM 中断配置 */
typedef struct apm_irq_config {
    u32        irq_vector;      /* 中断向量 */
    u32        priority;        /* 优先级 */
    u32        trigger_mode;    /* 触发模式 */
    u32        polarity;        /* 极性 */
    void       (*handler)(void);/* 处理函数 */
    apm_resource_state_t state; /* 状态 */
} apm_irq_config_t;

/* APM 定时器配置 */
typedef struct apm_timer_config {
    phys_addr_t base_addr;      /* 基地址 */
    u32        frequency;       /* 频率 (Hz) */
    u32        mode;            /* 模式 */
    u32        irq;             /* 中断号 */
    apm_resource_state_t state; /* 状态 */
} apm_timer_config_t;

/* APM 配置统计 */
typedef struct apm_stats {
    u32 total_resources;        /* 总资源数 */
    u32 allocated_resources;    /* 已分配资源数 */
    u32 initialized_resources;  /* 已初始化资源数 */
    u32 active_resources;       /* 活跃资源数 */
    u32 failed_resources;       /* 失败资源数 */
} apm_stats_t;

/* APM 全局配置 */
typedef struct apm_config {
    /* APM 模式 */
    apm_mode_t mode;            /* APM 模式 */
    
    /* 配置元数据 */
    u64         config_version; /* 配置版本 */
    u64         config_hash;    /* 配置哈希 */
    u64         timestamp;      /* 配置时间戳 */
    
    /* 串口配置 */
    apm_uart_config_t uart[4];  /* 最多4个串口 */
    u32              uart_count;
    
    /* 内存区域配置 */
    apm_memory_region_t memory[16];  /* 最多16个内存区域 */
    u32                   memory_count;
    
    /* 中断配置 */
    apm_irq_config_t irq[32];  /* 最多32个中断 */
    u32            irq_count;
    
    /* 定时器配置 */
    apm_timer_config_t timer[8];  /* 最多8个定时器 */
    u32               timer_count;
    
    /* 统计信息 */
    apm_stats_t stats;
    
    /* 配置完整性标志 */
    bool config_valid;         /* 配置有效 */
    bool config_verified;      /* 配置已验证 */
} apm_config_t;

/* 全局 APM 配置 */
extern apm_config_t g_apm_config;

/* ===== APM 核心接口 ===== */

/**
 * 初始化 APM 系统
 * 从 boot_info 读取配置文件并解析
 */
void apm_init(void);

/**
 * 设置 APM 模式
 */
void apm_set_mode(apm_mode_t mode);

/**
 * 获取 APM 模式
 */
apm_mode_t apm_get_mode(void);

/**
 * 验证 APM 配置完整性
 * 形式化验证入口点
 */
bool apm_verify_config(void);

/**
 * 获取 APM 统计信息
 */
void apm_get_stats(apm_stats_t *stats);

/**
 * 打印 APM 配置信息
 */
void apm_print_config(void);

/* ===== APM 资源接口 ===== */

/**
 * 获取串口配置
 */
apm_uart_config_t* apm_get_uart_config(u32 index);

/**
 * 初始化所有串口
 */
hic_status_t apm_init_all_uarts(void);

/**
 * 获取内存区域配置
 */
apm_memory_region_t* apm_get_memory_region(u32 index);

/**
 * 初始化所有内存区域
 */
hic_status_t apm_init_all_memory(void);

/**
 * 获取中断配置
 */
apm_irq_config_t* apm_get_irq_config(u32 index);

/**
 * 初始化所有中断
 */
hic_status_t apm_init_all_irqs(void);

/**
 * 获取定时器配置
 */
apm_timer_config_t* apm_get_timer_config(u32 index);

/**
 * 初始化所有定时器
 */
hic_status_t apm_init_all_timers(void);

/* ===== APM 形式化验证接口 ===== */

/**
 * 形式化验证：资源分配不变式
 * 验证所有资源分配的一致性
 */
bool apm_verify_allocation_invariant(void);

/**
 * 形式化验证：配置一致性
 * 验证配置参数的合法性
 */
bool apm_verify_config_consistency(void);

/**
 * 形式化验证：启动完整性
 * 验证启动时配置的完整性
 */
bool apm_verify_boot_integrity(void);

/**
 * 形式化验证：资源状态机
 * 验证资源状态转换的正确性
 */
bool apm_verify_state_machine(void);

/**
 * 形式化验证：无冲突分配
 * 验证资源分配无冲突
 */
bool apm_verify_no_conflicts(void);

/**
 * 运行所有形式化验证
 */
bool apm_run_all_verifications(void);

#endif /* HIC_KERNEL_APM_H */