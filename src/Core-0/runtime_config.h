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

/* 运行时配置系统 - 所有参数都可固定，如果未指定则自动探测 */
typedef struct runtime_config {
    /* ===== 系统基础配置 ===== */
    log_level_t log_level;        /* 日志级别: ERROR=0, WARN=1, INFO=2, DEBUG=3 (默认: INFO) */
    scheduler_policy_t scheduler_policy;  /* 调度策略: FIFO=0, RR=1, PRIORITY=2 (默认: PRIORITY) */
    memory_policy_t memory_policy;        /* 内存策略: FIRSTFIT=0, BESTFIT=1, BUDDY=2 (默认: FIRSTFIT) */
    security_level_t security_level;      /* 安全级别: MINIMAL=0, STANDARD=1, STRICT=2 (默认: STANDARD) */

    /* ===== 目标架构配置 ===== */
    char target_arch[16];         /* 目标架构: x86_64, arm64, riscv64 (空=自动探测) */
    char target_endian[8];        /* 字节序: little, big (空=自动探测) */
    u32 target_page_size;         /* 页面大小 (0=自动探测) */
    u32 target_word_size;         /* 字长 (0=自动探测) */

    /* ===== CPU特性配置 ===== */
    bool cpu_aes;                 /* AES指令集 (false=自动探测) */
    bool cpu_avx;                 /* AVX指令集 (false=自动探测) */
    bool cpu_avx2;                /* AVX2指令集 (false=自动探测) */
    bool cpu_mmx;                 /* MMX指令集 (false=自动探测) */
    bool cpu_rdrand;              /* RDRAND随机数生成器 (false=自动探测) */
    bool cpu_sse;                 /* SSE指令集 (false=自动探测) */
    bool cpu_sse2;                /* SSE2指令集 (false=自动探测) */
    bool cpu_sse3;                /* SSE3指令集 (false=自动探测) */
    bool cpu_sse4_1;              /* SSE4.1指令集 (false=自动探测) */
    bool cpu_sse4_2;              /* SSE4.2指令集 (false=自动探测) */
    bool cpu_ssse3;               /* SSSE3指令集 (false=自动探测) */

    /* ===== 系统限制配置 ===== */
    u32 max_domains;              /* 最大域数量 (0=根据可用内存自动计算) */
    u32 max_threads;              /* 最大线程数 (0=根据可用内存自动计算) */
    u32 max_capabilities;         /* 最大能力数量 (0=根据可用内存自动计算) */
    u32 capabilities_per_domain;  /* 每域最大能力数 (0=默认128) */
    u32 threads_per_domain;       /* 每域最大线程数 (0=默认16) */
    u32 max_services;             /* 最大服务数 (0=默认64) */
    u32 max_pci_devices;          /* 最大PCI设备数 (0=自动检测) */
    u32 max_memory_regions;       /* 最大内存区域数 (0=自动计算) */
    u32 max_interrupt_routes;     /* 最大中断路由数 (0=根据硬件自动计算) */
    u64 kernel_size_limit;        /* 内核大小限制 (0=无限制) */
    u64 bss_size_limit;           /* BSS段大小限制 (0=默认512KB) */

    /* ===== 内存布局配置 ===== */
    u64 heap_size_mb;             /* 堆大小MB (0=自动分配) */
    u64 stack_size_kb;            /* 栈大小KB (0=默认8KB) */
    u32 page_cache_percent;       /* 页面缓存百分比 (0=默认20%) */
    u64 domain_pool_base;         /* 域内存池基地址 (0=自动选择) */
    u64 domain_pool_size;         /* 域内存池大小 (0=自动计算) */
    u64 app_pool_base;            /* 应用内存池基地址 (0=自动选择) */
    u64 app_pool_size;            /* 应用内存池大小 (0=自动计算) */
    u32 max_page_tables;          /* 最大页表数 (0=自动计算) */
    u32 page_table_entries;       /* 每页表条目数 (0=根据架构自动设置) */
    u64 buffer_cache_size;        /* 缓冲区缓存大小 (0=默认1MB) */

    /* ===== 调度器配置 ===== */
    u32 time_slice_ms;            /* 时间片长度ms (0=默认10) */
    u32 idle_timeout_ms;          /* 空闲超时ms (0=默认0) */
    bool preemptive;              /* 抢占式调度 (true=默认true) */
    u32 load_balancing_threshold; /* 负载均衡阈值% (0=默认80) */
    u32 migration_interval_ms;    /* 迁移间隔ms (0=默认100) */
    bool load_balancing_enabled;  /* 启用负载均衡 (true=默认true) */
    u32 prio_realtime_min;        /* 实时优先级下限 (0=默认0) */
    u32 prio_realtime_max;        /* 实时优先级上限 (0=默认2) */
    u32 prio_high_min;            /* 高优先级下限 (0=默认3) */
    u32 prio_high_max;            /* 高优先级上限 (0=默认10) */
    u32 prio_normal_min;          /* 普通优先级下限 (0=默认11) */
    u32 prio_normal_max;          /* 普通优先级上限 (0=默认50) */
    u32 prio_low_min;             /* 低优先级下限 (0=默认51) */
    u32 prio_low_max;             /* 低优先级上限 (0=默认100) */
    u32 prio_idle_min;            /* 空闲优先级下限 (0=默认101) */
    u32 prio_idle_max;            /* 空闲优先级上限 (0=默认127) */

    /* ===== 安全配置 ===== */
    bool enable_secure_boot;      /* 启用安全启动 (true=默认false) */
    bool enable_kaslr;            /* 启用地址随机化 (true=默认false) */
    bool enable_smap;             /* 启用SMEP (true=默认false) */
    bool enable_smep;             /* 启用SMAP (true=默认false) */
    bool enable_audit;            /* 启用审计 (true=默认true) */
    bool guard_pages;             /* 启用保护页 (true=默认true) */
    u32 guard_page_size;          /* 保护页大小 (0=默认4096) */
    bool zero_on_free;            /* 释放时清零 (true=默认true) */
    bool verify_on_access;        /* 访问时验证 (true=默认true) */
    bool log_privileged_ops;      /* 记录特权操作 (true=默认true) */
    u32 capability_revoke_delay_ms; /* 能力撤销延迟ms (0=默认100) */
    char isolation_mode[16];      /* 隔离模式 (空=默认strict) */
    bool form_verification;       /* 形式验证 (true=默认true) */

    /* ===== 性能配置 ===== */
    bool enable_perf_counters;    /* 启用性能计数器 (true=默认true) */
    bool enable_fast_path;        /* 启用快速路径 (true=默认true) */
    u32 fast_path_threshold;      /* 快速路径阈值 (0=默认50) */

    /* ===== 调试配置 ===== */
    bool enable_debug;            /* 启用调试 (true=默认false) */
    bool enable_trace;            /* 启用跟踪 (true=默认false) */
    bool enable_verbose;          /* 启用详细输出 (true=默认false) */
    bool bounds_check;            /* 边界检查 (true=默认true) */
    bool stack_canary;            /* 栈保护 (true=默认true) */
    bool panic_on_bug;            /* 遇到 bug 时 panic (true=默认true) */
    bool console_log;             /* 控制台日志 (true=默认true) */
    bool serial_log;              /* 串口日志 (true=默认true) */

    /* ===== 设备配置 ===== */
    bool enable_pci;              /* 启用PCI (false=自动检测) */
    bool enable_acpi;             /* 启用ACPI (false=自动检测) */
    bool enable_smp;              /* 启用SMP/AMP (false=自动检测) */
    bool enable_usb;              /* 启用USB (false=自动检测) */
    bool enable_ahci;             /* 启用AHCI (false=自动检测) */
    bool enable_apic;             /* 启用APIC (false=自动检测) */
    bool enable_efi;              /* 启用EFI (false=自动检测) */
    bool enable_virtio;           /* 启用VirtIO (false=自动检测) */

    /* ===== 串口配置 ===== */
    bool enable_serial;           /* 启用串口 (true=默认true) */
    u32 serial_port;              /* 串口I/O端口 (0=自动探测) */
    u32 serial_baud;              /* 串口波特率 (0=默认115200) */
    u32 serial_data_bits;         /* 数据位数 (0=默认8) */
    u32 serial_stop_bits;         /* 停止位数 (0=默认1) */
    char serial_parity;           /* 校验位: 'n'=none, 'o'=odd, 'e'=even (默认'n') */
    u32 serial_buffer_size;       /* 缓冲区大小 (0=默认4096) */
    bool serial_early_init;       /* 早期初始化 (true=默认true) */

    /* ===== 模块配置 ===== */
    bool enable_module_loading;   /* 启用模块加载 (true=默认true) */
    u32 max_modules;              /* 最大模块数 (0=不限制) */
    bool module_cache_enabled;    /* 启用模块缓存 (true=默认true) */
    bool module_verify_signature; /* 验证模块签名 (true=默认true) */
    char module_local_path[256];  /* 本地模块路径 (空=默认/var/lib/hic/modules) */

    /* ===== 中断配置 ===== */
    u32 max_irqs;                 /* 最大中断数 (0=自动检测) */
    bool enable_irq_fairness;     /* 启用中断公平性 (true=默认true) */

    /* ===== 资源配额配置 ===== */
    u32 domain_cpu_quota_percent; /* 域CPU配额% (0=默认10) */
    u32 domain_max_capabilities;  /* 域最大能力数 (0=默认128) */
    u32 domain_max_memory;        /* 域最大内存 (0=默认1MB) */
    u32 domain_max_threads;       /* 域最大线程数 (0=默认16) */

    /* ===== 已移除字段（为了兼容性保留） ===== */
    bool enable_swap;                /* 启用交换空间 (false=默认false) */
    u32 debug_port;                  /* 调试端口 (0=默认0x3F8) */
    bool enable_capability_derivation; /* 启用能力派生 (true=默认true) */
    u32 domain_stack_size_kb;        /* 域栈大小KB (0=默认16) */
    char default_password[64];       /* 默认密码 (空=默认"admin123") */
    bool password_required;          /* 需要密码 (true=默认true) */
    u32 password_min_length;         /* 密码最小长度 (0=默认8) */
    bool password_require_upper;     /* 密码需要大写字母 (true=默认true) */
    bool password_require_lower;     /* 密码需要小写字母 (true=默认true) */
    bool password_require_digit;     /* 密码需要数字 (true=默认true) */
    u8 config_hash[32];              /* 配置哈希 (全零=默认) */
    u32 max_domains_per_user;        /* 每用户最大域数 (0=默认64) */
    u32 num_custom_items;            /* 自定义配置项数量 (0=默认0) */
    config_item_t custom_items[16];  /* 自定义配置项数组 */

    /* ===== 配置元数据 ===== */
    config_source_t config_source;  /* 配置来源 */
    u64 config_timestamp;          /* 配置时间戳 */
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