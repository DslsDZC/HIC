/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 电源管理抽象层 (Power Management Abstraction Layer)
 * 
 * 提供跨平台的电源状态管理接口：
 * 1. CPU 电源状态 (C-states): 从 C0（活跃）到 Cn（深度睡眠）
 * 2. 设备电源状态 (D-states): 设备特定的低功耗模式
 * 3. 系统电源状态 (S-states): S0（开机）到 S5（机械关闭）
 * 4. DVFS: 动态电压频率调节
 * 5. 功耗域管理: 抽象的平台功耗域控制
 */

#ifndef HIC_KERNEL_PM_H
#define HIC_KERNEL_PM_H

#include "types.h"

/* ========== CPU 电源状态 (C-states) ========== */

/* CPU 电源状态定义 */
typedef enum pm_cstate {
    PM_CSTATE_C0 = 0,       /* 活跃状态：CPU 全速运行 */
    PM_CSTATE_C1,           /* HLT/Wait：停止时钟，最小延迟 */
    PM_CSTATE_C2,           /* Stop-Clock：停止时钟，较长延迟 */
    PM_CSTATE_C3,           /* Sleep：关闭时钟和缓存 */
    PM_CSTATE_C4,           /* Deeper Sleep：降低电压 */
    PM_CSTATE_C5,           /* Enhanced Deeper Sleep */
    PM_CSTATE_C6,           /* Deep Power Down：保存上下文 */
    PM_CSTATE_C7,           /* 最深睡眠状态 */
    PM_CSTATE_MAX
} pm_cstate_t;

/* CPU 电源状态信息 */
typedef struct pm_cstate_info {
    pm_cstate_t state;          /* 状态 ID */
    const char *name;           /* 状态名称 */
    u32 latency_us;             /* 进入/退出延迟（微秒） */
    u32 power_mw;               /* 功耗（毫瓦） */
    u32 flags;                  /* 状态标志 */
} pm_cstate_info_t;

/* C-state 标志 */
#define PM_CSTATE_FLAG_CACHE_FLUSH   (1 << 0)  /* 需要刷新缓存 */
#define PM_CSTATE_FLAG_TLB_FLUSH     (1 << 1)  /* 需要刷新 TLB */
#define PM_CSTATE_FLAG_CONTEXT_SAVE  (1 << 2)  /* 需要保存上下文 */
#define PM_CSTATE_FLAG_HALT_ONLY     (1 << 3)  /* 仅使用 HLT 指令 */

/* ========== 设备电源状态 (D-states) ========== */

/* 设备电源状态定义 */
typedef enum pm_dstate {
    PM_DSTATE_D0 = 0,       /* 全功率：设备完全活跃 */
    PM_DSTATE_D1,           /* 低功耗：设备部分功能可用 */
    PM_DSTATE_D2,           /* 更低功耗：大多数功能关闭 */
    PM_DSTATE_D3HOT,        /* D3 Hot：设备仍有电源，可快速唤醒 */
    PM_DSTATE_D3COLD,       /* D3 Cold：设备完全断电 */
    PM_DSTATE_MAX
} pm_dstate_t;

/* 设备电源信息 */
typedef struct pm_device_power {
    u64 device_id;              /* 设备标识符 */
    pm_dstate_t current_state;  /* 当前状态 */
    pm_dstate_t max_state;      /* 最大支持状态 */
    u32 wakeup_latency_us;      /* 唤醒延迟 */
    u32 flags;                  /* 设备标志 */
} pm_device_power_t;

/* D-state 标志 */
#define PM_DSTATE_FLAG_WAKEUP_CAPABLE  (1 << 0)  /* 支持唤醒 */
#define PM_DSTATE_FLAG_WAKEUP_ENABLED  (1 << 1)  /* 唤醒已启用 */

/* ========== 系统电源状态 (S-states) ========== */

/* 系统电源状态定义 */
typedef enum pm_sstate {
    PM_SSTATE_S0 = 0,       /* 工作：系统完全运行 */
    PM_SSTATE_S1,           /* 待机：CPU 停止，内存刷新 */
    PM_SSTATE_S2,           /* 睡眠：CPU 断电，内存刷新 */
    PM_SSTATE_S3,           /* 挂起到内存 (Suspend to RAM) */
    PM_SSTATE_S4,           /* 挂起到磁盘 (Suspend to Disk) */
    PM_SSTATE_S5,           /* 软关机：仅电源管理单元工作 */
    PM_SSTATE_S6,           /* 保留 */
    PM_SSTATE_MAX
} pm_sstate_t;

/* 系统电源状态信息 */
typedef struct pm_sstate_info {
    pm_sstate_t state;          /* 状态 ID */
    const char *name;           /* 状态名称 */
    u32 latency_ms;             /* 唤醒延迟（毫秒） */
    u32 power_mw;               /* 功耗（毫瓦） */
    bool supported;             /* 是否支持 */
} pm_sstate_info_t;

/* ========== DVFS (动态电压频率调节) ========== */

/* DVFS 操作模式 */
typedef enum pm_dvfs_mode {
    PM_DVFS_MODE_PERFORMANCE = 0,   /* 性能优先：最高频率 */
    PM_DVFS_MODE_BALANCED,          /* 平衡模式：动态调节 */
    PM_DVFS_MODE_POWERSAVE,         /* 节能优先：最低频率 */
    PM_DVFS_MODE_USERSPACE,         /* 用户指定频率 */
    PM_DVFS_MODE_MAX
} pm_dvfs_mode_t;

/* DVFS 频率级别 */
typedef struct pm_freq_level {
    u32 freq_khz;               /* 频率（kHz） */
    u32 voltage_mv;             /* 电压（毫伏） */
    u32 power_mw;               /* 功耗（毫瓦） */
    u32 latency_us;             /* 切换延迟 */
    u32 flags;                  /* 级别标志 */
} pm_freq_level_t;

/* DVFS 频率级别标志 */
#define PM_FREQ_FLAG_BOOST     (1 << 0)  /* 超频级别 */
#define PM_FREQ_FLAG_TURBO     (1 << 1)  /* Turbo 级别 */
#define PM_FREQ_FLAG_EFFICIENT (1 << 2)  /* 高效能效比 */

/* DVFS 上下文 */
typedef struct pm_dvfs_context {
    u32 domain_id;              /* 功耗域 ID */
    pm_dvfs_mode_t mode;        /* 当前模式 */
    u32 current_freq_khz;       /* 当前频率 */
    u32 current_voltage_mv;     /* 当前电压 */
    u32 min_freq_khz;           /* 最小频率 */
    u32 max_freq_khz;           /* 最大频率 */
    u32 freq_level_count;       /* 频率级别数量 */
    pm_freq_level_t *freq_levels;  /* 频率级别表 */
} pm_dvfs_context_t;

/* ========== 功耗域管理 ========== */

/* 功耗域类型 */
typedef enum pm_domain_type {
    PM_DOMAIN_CPU = 0,          /* CPU 功耗域 */
    PM_DOMAIN_GPU,              /* GPU 功耗域 */
    PM_DOMAIN_MEMORY,           /* 内存功耗域 */
    PM_DOMAIN_DEVICE,           /* 设备功耗域 */
    PM_DOMAIN_SYSTEM,           /* 系统功耗域 */
    PM_DOMAIN_MAX
} pm_domain_type_t;

/* 功耗域状态 */
typedef struct pm_domain {
    u32 domain_id;              /* 域 ID */
    pm_domain_type_t type;      /* 域类型 */
    const char *name;           /* 域名称 */
    bool is_on;                 /* 是否开启 */
    u32 device_count;           /* 关联设备数量 */
    u64 *devices;               /* 关联设备 ID 列表 */
    pm_dvfs_context_t dvfs;     /* DVFS 上下文 */
} pm_domain_t;

/* ========== 空闲状态预测 ========== */

/* 空闲预测模式 */
typedef enum pm_idle_predict_mode {
    PM_IDLE_PREDICT_DISABLED = 0,   /* 禁用预测 */
    PM_IDLE_PREDICT_SIMPLE,         /* 简单预测：基于历史 */
    PM_IDLE_PREDICT_ADAPTIVE,       /* 自适应预测 */
    PM_IDLE_PREDICT_MAX
} pm_idle_predict_mode_t;

/* 空闲预测配置 */
typedef struct pm_idle_predict_config {
    pm_idle_predict_mode_t mode;    /* 预测模式 */
    u32 history_depth;              /* 历史深度 */
    u32 accuracy_threshold;         /* 准确率阈值 (%) */
    u32 max_cstate;                 /* 最大允许 C-state */
} pm_idle_predict_config_t;

/* ========== 全局电源管理状态 ========== */

typedef struct pm_state {
    /* CPU 电源状态 */
    pm_cstate_t current_cstate;     /* 当前 C-state */
    pm_cstate_t max_cstate;         /* 最大支持 C-state */
    u32 cstate_count;               /* C-state 数量 */
    pm_cstate_info_t *cstates;      /* C-state 信息表 */
    
    /* 系统电源状态 */
    pm_sstate_t current_sstate;     /* 当前 S-state */
    pm_sstate_info_t sstates[PM_SSTATE_MAX];  /* S-state 信息 */
    
    /* 功耗域 */
    u32 domain_count;               /* 功耗域数量 */
    pm_domain_t *domains;           /* 功耗域列表 */
    
    /* 空闲预测 */
    pm_idle_predict_config_t idle_predict;  /* 空闲预测配置 */
    
    /* 统计信息 */
    u64 total_idle_time_us;         /* 总空闲时间 */
    u64 total_wakeups;              /* 总唤醒次数 */
    u32 last_idle_duration_us;      /* 上次空闲持续时间 */
} pm_state_t;

/* ========== API 函数声明 ========== */

/* 初始化 */
void pm_init(void);
void pm_shutdown(void);

/* CPU 电源状态管理 */
pm_cstate_t pm_get_cstate(void);
hic_status_t pm_set_cstate(pm_cstate_t state);
pm_cstate_info_t* pm_get_cstate_info(pm_cstate_t state);
u32 pm_get_supported_cstates(void);

/* 设备电源状态管理 */
pm_dstate_t pm_get_device_state(u64 device_id);
hic_status_t pm_set_device_state(u64 device_id, pm_dstate_t state);
hic_status_t pm_device_suspend(u64 device_id);
hic_status_t pm_device_resume(u64 device_id);
bool pm_device_can_wakeup(u64 device_id);
hic_status_t pm_device_enable_wakeup(u64 device_id, bool enable);

/* 系统电源状态管理 */
pm_sstate_t pm_get_sstate(void);
hic_status_t pm_set_sstate(pm_sstate_t state);
hic_status_t pm_suspend(void);
hic_status_t pm_hibernate(void);
hic_status_t pm_shutdown_system(void);
hic_status_t pm_reboot(void);
bool pm_sstate_supported(pm_sstate_t state);

/* DVFS 管理 */
u32 pm_get_frequency(u32 domain_id);
hic_status_t pm_set_frequency(u32 domain_id, u32 freq_khz);
pm_dvfs_mode_t pm_get_dvfs_mode(u32 domain_id);
hic_status_t pm_set_dvfs_mode(u32 domain_id, pm_dvfs_mode_t mode);
u32 pm_get_frequency_count(u32 domain_id);
pm_freq_level_t* pm_get_frequency_levels(u32 domain_id, u32 *count);

/* 功耗域管理 */
pm_domain_t* pm_get_domain(u32 domain_id);
hic_status_t pm_domain_power_on(u32 domain_id);
hic_status_t pm_domain_power_off(u32 domain_id);
bool pm_domain_is_on(u32 domain_id);
u32 pm_get_domain_count(void);

/* 空闲预测 */
void pm_idle_predict_init(pm_idle_predict_mode_t mode);
u32 pm_idle_predict_next_cstate(void);
void pm_idle_predict_update(u32 actual_duration_us);

/* 空闲处理 */
void pm_idle(void);
void pm_deep_idle(void);

/* 架构相关回调（由 HAL 实现） */
typedef struct pm_arch_ops {
    void (*enter_cstate)(pm_cstate_t state);
    void (*exit_cstate)(void);
    void (*set_frequency)(u32 domain_id, u32 freq_khz, u32 voltage_mv);
    u32 (*get_frequency)(u32 domain_id);
    void (*domain_power_on)(u32 domain_id);
    void (*domain_power_off)(u32 domain_id);
    void (*system_suspend)(pm_sstate_t state);
    void (*system_resume)(void);
} pm_arch_ops_t;

void pm_register_arch_ops(const pm_arch_ops_t *ops);

/* 统计信息 */
void pm_get_stats(pm_state_t *state);
void pm_print_stats(void);

#endif /* HIC_KERNEL_PM_H */
