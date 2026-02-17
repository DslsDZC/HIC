/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核运行时配置系统
 * 支持通过引导层传递配置参数，实现无需重新编译即可调整内核行为
 */

#ifndef HIC_RUNTIME_CONFIG_H
#define HIC_RUNTIME_CONFIG_H

#include "types.h"

/* 配置来源 */
typedef enum config_source {
    CONFIG_SOURCE_DEFAULT = 0,    /* 默认值 */
    CONFIG_SOURCE_BOOTLOADER,     /* 引导层传递 */
    CONFIG_SOURCE_PLATFORM_YAML,  /* platform.yaml配置文件 */
    CONFIG_SOURCE_CMDLINE,        /* 命令行参数 */
    CONFIG_SOURCE_COUNT
} config_source_t;

/* 配置项类型 */
typedef enum config_type {
    CONFIG_TYPE_BOOL = 0,         /* 布尔值 */
    CONFIG_TYPE_U32,              /* 32位无符号整数 */
    CONFIG_TYPE_U64,              /* 64位无符号整数 */
    CONFIG_TYPE_STRING,           /* 字符串 */
    CONFIG_TYPE_ENUM,             /* 枚举值 */
    CONFIG_TYPE_COUNT
} config_type_t;

/* 日志级别 */
typedef enum log_level {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_COUNT
} log_level_t;

/* 调度策略 */
typedef enum scheduler_policy {
    SCHEDULER_POLICY_FIFO = 0,    /* 先进先出 */
    SCHEDULER_POLICY_RR,          /* 轮转调度 */
    SCHEDULER_POLICY_PRIORITY,    /* 优先级调度 */
    SCHEDULER_POLICY_COUNT
} scheduler_policy_t;

/* 内存分配策略 */
typedef enum memory_policy {
    MEMORY_POLICY_FIRSTFIT = 0,   /* 首次适配 */
    MEMORY_POLICY_BESTFIT,        /* 最佳适配 */
    MEMORY_POLICY_BUDDY,          /* 伙伴系统 */
    MEMORY_POLICY_COUNT
} memory_policy_t;

/* 安全级别 */
typedef enum security_level {
    SECURITY_LEVEL_MINIMAL = 0,   /* 最小安全 */
    SECURITY_LEVEL_STANDARD,      /* 标准安全 */
    SECURITY_LEVEL_STRICT,        /* 严格安全 */
    SECURITY_LEVEL_COUNT
} security_level_t;

/* 运行时配置项 */
typedef struct config_item {
    char name[64];                /* 配置项名称 */
    config_type_t type;           /* 配置项类型 */
    config_source_t source;       /* 配置来源 */
    u32 flags;                    /* 标志位 */
#define CONFIG_FLAG_READONLY    (1U << 0)  /* 只读配置 */
#define CONFIG_FLAG_RUNTIME     (1U << 1)  /* 可运行时修改 */
#define CONFIG_FLAG_REBOOT      (1U << 2)  /* 修改后需重启 */
    
    union {
        bool    bool_val;
        u32     u32_val;
        u64     u64_val;
        char    string_val[256];
        u32     enum_val;
    } value;
    
    union {
        bool    bool_default;
        u32     u32_default;
        u64     u64_default;
        char    string_default[256];
        u32     enum_default;
    } default_value;
    
    const char* description;      /* 配置项描述 */
} config_item_t;

/* 运行时配置系统 */
typedef struct runtime_config {
    /* 系统配置 */
    log_level_t log_level;        /* 日志级别 */
    scheduler_policy_t scheduler_policy;  /* 调度策略 */
    memory_policy_t memory_policy;        /* 内存分配策略 */
    security_level_t security_level;      /* 安全级别 */
    
    /* 调度器配置 */
    u32 time_slice_ms;            /* 时间片长度(毫秒) */
    u32 max_threads;              /* 最大线程数 */
    u32 idle_timeout_ms;          /* 空闲超时(毫秒) */
    
    /* 内存配置 */
    u64 heap_size_mb;             /* 堆大小(MB) */
    u64 stack_size_kb;            /* 栈大小(KB) */
    u32 page_cache_percent;       /* 页面缓存百分比 */
    bool enable_swap;             /* 启用交换 */
    
    /* 安全配置 */
    bool enable_secure_boot;      /* 启用安全启动 */
    bool enable_kaslr;            /* 启用地址随机化 */
    bool enable_smap;             /* 启用SMEP */
    bool enable_smep;             /* 启用SMAP */
    bool enable_audit;            /* 启用审计 */
    
    /* 性能配置 */
    bool enable_perf_counters;    /* 启用性能计数器 */
    bool enable_fast_path;        /* 启用快速路径 */
    u32 fast_path_threshold;      /* 快速路径阈值 */
    
    /* 调试配置 */
    bool enable_debug;            /* 启用调试 */
    bool enable_trace;            /* 启用跟踪 */
    bool enable_verbose;          /* 启用详细输出 */
    u32 debug_port;               /* 调试端口 */
    
    /* 设备配置 */
    bool enable_pci;              /* 启用PCI */
    bool enable_acpi;             /* 启用ACPI */
    bool enable_serial;           /* 启用串口 */
    u32 serial_baud;              /* 串口波特率 */
    
    /* 能力系统配置 */
    u32 max_capabilities;         /* 最大能力数量 */
    bool enable_capability_derivation;  /* 启用能力派生 */
    
    /* 域配置 */
    u32 max_domains;              /* 最大域数量 */
    u32 domain_stack_size_kb;     /* 域栈大小(KB) */
    
    /* 中断配置 */
    u32 max_irqs;                 /* 最大中断数 */
    bool enable_irq_fairness;     /* 启用中断公平性 */
    
    /* 模块配置 */
    bool enable_module_loading;   /* 启用模块加载 */
    u32 max_modules;              /* 最大模块数 */
    
    /* 自定义配置项 */
    config_item_t custom_items[32];
    u32 num_custom_items;
    
    /* 配置元数据 */
    config_source_t config_source;  /* 主配置来源 */
    u64 config_timestamp;          /* 配置时间戳 */
    char config_hash[64];          /* 配置哈希 */
} runtime_config_t;

/* 全局运行时配置 */
extern runtime_config_t g_runtime_config;

/* 初始化运行时配置系统 */
void runtime_config_init(void);

/* 从引导信息加载配置 */
void runtime_config_load_from_bootinfo(void);

/* 从命令行加载配置 */
void runtime_config_load_from_cmdline(const char* cmdline);

/* 从platform.yaml加载配置 */
void runtime_config_load_from_yaml(const char* yaml_data, size_t yaml_size);

/* 获取配置项值 */
config_item_t* runtime_config_get_item(const char* name);

/* 设置配置项值 */
hic_status_t runtime_config_set_item(const char* name, const void* value, config_type_t type);

/* 重置配置项为默认值 */
void runtime_config_reset_item(const char* name);

/* 打印当前配置 */
void runtime_config_print(void);

/* 验证配置一致性 */
bool runtime_config_validate(void);

/* 导出配置为YAML */
void runtime_config_export_yaml(char* buffer, size_t buffer_size);

/* 运行时配置更新通知 */
typedef void (*config_change_callback_t)(const char* name, const void* old_value, const void* new_value);

/* 注册配置变更回调 */
void runtime_config_register_callback(const char* name, config_change_callback_t callback);

/* 检查配置是否可以运行时修改 */
bool runtime_config_is_runtime_modifiable(const char* name);

#endif /* HIC_RUNTIME_CONFIG_H */