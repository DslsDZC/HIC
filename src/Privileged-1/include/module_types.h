/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#ifndef MODULE_TYPES_H
#define MODULE_TYPES_H

#include "common.h"

/* 模块魔数 */
#define HICMOD_MAGIC 0x48494B4D  /* "HICM" */
#define HICMOD_VERSION 1

/* 最大模块数量 */
#define MAX_MODULES 32

/* 最大重试次数 */
#define MAX_RESTART_ATTEMPTS 3

/* 模块头结构（72字节） */
typedef struct {
    u32     magic;              /* 魔数: 0x48494B4D */
    u32     version;            /* 模块格式版本 */
    u8      uuid[16];           /* 模块 UUID */
    u32     semantic_version;   /* 语义版本 (major<<16 | minor<<8 | patch) */
    u32     api_desc_offset;    /* API 描述符偏移 */
    u32     code_size;          /* 代码段大小 */
    u32     data_size;          /* 数据段大小 */
    u32     signature_offset;   /* 签名偏移 */
    u32     header_size;        /* 头部大小 */
    u8      checksum[16];       /* SHA-256 校验和 */
    u32     signature_size;     /* 签名大小 */
    u32     flags;              /* 标志位: bit0=已签名 */
} __attribute__((packed)) hicmod_header_t;

/* 模块状态 */
typedef enum {
    MODULE_STATE_unloaded = 0,  /* 未加载 */
    MODULE_STATE_loading,       /* 加载中 */
    MODULE_STATE_loaded,        /* 已加载 */
    MODULE_STATE_running,       /* 运行中 */
    MODULE_STATE_suspended,     /* 已暂停 */
    MODULE_STATE_error,         /* 错误 */
    MODULE_STATE_unloading      /* 卸载中 */
} module_state_t;

/* 状态迁移协议回调（模块实现）- 前向声明 */
typedef struct state_migration_protocol {
    /* 导出状态到缓冲区，返回实际大小 */
    hic_status_t (*export_state)(void *buffer, u32 *size);
    /* 从缓冲区导入状态 */
    hic_status_t (*import_state)(const void *buffer, u32 size);
    /* 准备迁移（可选） */
    hic_status_t (*prepare_migration)(void);
    /* 完成迁移（可选） */
    hic_status_t (*complete_migration)(void);
    /* 中止迁移（可选） */
    hic_status_t (*abort_migration)(void);
} state_migration_protocol_t;

/* 模块实例 */
typedef struct module_instance {
    u64                 instance_id;      /* 实例 ID */
    char                name[64];         /* 模块名称 */
    u8                  uuid[16];         /* 模块 UUID */
    u32                 version;          /* 语义版本 */
    module_state_t      state;            /* 模块状态 */
    void               *code_base;        /* 代码基地址 */
    u32                 code_size;        /* 代码大小 */
    u32                 flags;            /* 模块标志 */
    u32                 ref_count;        /* 引用计数 */
    u32                 restart_count;    /* 重启次数 */
    
    /* 服务 API */
    hic_status_t      (*init)(void);      /* 初始化函数 */
    hic_status_t      (*start)(void);     /* 启动函数 */
    hic_status_t      (*stop)(void);      /* 停止函数 */
    hic_status_t      (*cleanup)(void);   /* 清理函数 */
    
    /* 状态迁移协议（滚动更新） */
    state_migration_protocol_t migration; /* 状态迁移回调 */
    
    /* 自动重启 */
    u8                 auto_restart;      /* 是否自动重启 */
    u64                last_error_time;   /* 最后错误时间 */
    
    /* 备份（用于滚动更新） */
    void               *backup_state;     /* 备份状态 */
    u32                 backup_size;      /* 备份大小 */
    u32                 backup_version;   /* 备份时的模块版本 */
} module_instance_t;

/* 模块信息（简化版，用于 CLI） */
typedef struct {
    char                name[64];         /* 模块名称 */
    u8                  uuid[16];         /* 模块 UUID */
    u32                 version;          /* 语义版本 */
    module_state_t      state;            /* 模块状态 */
    u32                 flags;            /* 模块标志 */
    u32                 code_size;        /* 代码大小 */
    u32                 data_size;        /* 数据大小 */
} module_info_t;

/* 状态备份魔数 */
#define MODULE_STATE_BACKUP_MAGIC  0x4D535442  /* "MSTB" */
#define MODULE_STATE_BACKUP_VERSION 1

/* 模块状态备份头 */
typedef struct {
    u32                 magic;            /* 魔数: 0x4D535442 */
    u32                 version;          /* 备份格式版本 */
    u8                  uuid[16];         /* 模块 UUID */
    u32                 module_version;   /* 模块版本 */
    u32                 state_size;       /* 状态数据大小 */
    u64                 timestamp;        /* 备份时间戳 */
    u32                 checksum;         /* CRC32 校验和 */
    u32                 flags;            /* 备份标志 */
} module_state_backup_header_t;

/* 模块状态备份标志 */
#define MODULE_BACKUP_FLAG_NONE         0x00
#define MODULE_BACKUP_FLAG_COMPRESSED   0x01  /* 压缩存储 */
#define MODULE_BACKUP_FLAG_ENCRYPTED    0x02  /* 加密存储 */
#define MODULE_BACKUP_FLAG_INCREMENTAL  0x04  /* 增量备份 */

/* 模块依赖 */
typedef struct {
    char                name[64];         /* 依赖名称 */
    u32                 min_version;      /* 最小版本 */
} module_dependency_t;

/* 版本兼容性检查 */
static inline bool is_version_compatible(u32 old_version, u32 new_version) {
    u32 old_major = (old_version >> 16) & 0xFF;
    u32 old_minor = (old_version >> 8) & 0xFF;
    u32 new_major = (new_version >> 16) & 0xFF;
    u32 new_minor = (new_version >> 8) & 0xFF;
    
    /* 主版本必须相同 */
    if (old_major != new_major) {
        return false;
    }
    
    /* 次版本向下兼容 */
    return new_minor >= old_minor;
}

/* ==================== 零停机更新策略类型 ==================== */

/* 更新策略类型 */
typedef enum {
    UPDATE_STRATEGY_BLUE_GREEN,     /* 蓝绿部署：新旧实例并行，一次性切换 */
    UPDATE_STRATEGY_CANARY,         /* 金丝雀发布：逐步增加新实例流量 */
    UPDATE_STRATEGY_GRADUAL,        /* 渐进式更新：分阶段迁移连接 */
    UPDATE_STRATEGY_IMMEDIATE       /* 立即更新（有短暂停机，回退模式） */
} update_strategy_t;

/* 更新阶段 */
typedef enum {
    UPDATE_PHASE_INIT,              /* 初始化 */
    UPDATE_PHASE_PREPARING,         /* 准备中：资源检查 */
    UPDATE_PHASE_CREATING,          /* 创建新实例 */
    UPDATE_PHASE_WARMING,           /* 预热新实例 */
    UPDATE_PHASE_MIGRATING,         /* 迁移状态/连接 */
    UPDATE_PHASE_SWITCHING,         /* 切换流量 */
    UPDATE_PHASE_DRAINING,          /* 排空旧实例 */
    UPDATE_PHASE_CLEANUP,           /* 清理旧实例 */
    UPDATE_PHASE_COMPLETED,         /* 完成 */
    UPDATE_PHASE_ROLLBACK,          /* 回滚 */
    UPDATE_PHASE_FAILED             /* 失败 */
} update_phase_t;

/* 连接迁移优先级 */
typedef enum {
    CONN_PRIORITY_LOW = 0,          /* 低优先级：后台连接 */
    CONN_PRIORITY_NORMAL = 1,       /* 普通优先级 */
    CONN_PRIORITY_HIGH = 2,         /* 高优先级：VIP客户 */
    CONN_PRIORITY_CRITICAL = 3      /* 关键：实时流媒体 */
} connection_priority_t;

/* 迁移连接描述 */
typedef struct {
    u64                 conn_id;        /* 连接ID */
    connection_priority_t priority;     /* 优先级 */
    u64                 last_active;    /* 最后活动时间 */
    u32                 bytes_pending;  /* 待处理字节数 */
    u8                  state_data[256];/* 连接状态 */
    u32                 state_size;     /* 状态大小 */
} migration_connection_t;

/* 更新上下文（跟踪更新状态） */
typedef struct {
    char                module_name[64];    /* 模块名称 */
    u32                 old_version;        /* 旧版本 */
    u32                 new_version;        /* 新版本 */
    update_strategy_t   strategy;           /* 更新策略 */
    update_phase_t      phase;              /* 当前阶段 */
    
    domain_id_t         old_domain;         /* 旧实例域ID */
    domain_id_t         new_domain;         /* 新实例域ID */
    
    cap_id_t            service_cap;        /* 服务能力ID */
    cap_id_t            migration_channel;  /* 迁移通道能力ID */
    
    u32                 total_connections;  /* 总连接数 */
    u32                 migrated_connections;/* 已迁移连接数 */
    u32                 failed_migrations;  /* 迁移失败数 */
    
    u64                 start_time;         /* 开始时间 */
    u64                 phase_start_time;   /* 当前阶段开始时间 */
    
    /* 健康检查 */
    u32                 health_check_failures;
    u32                 max_health_failures;
    
    /* 回滚点 */
    void               *rollback_state;     /* 回滚状态 */
    u32                 rollback_size;
    
    /* 配置 */
    u32                 drain_timeout_ms;   /* 排空超时 */
    u32                 health_check_interval_ms;
    bool                auto_rollback;      /* 自动回滚 */
} update_context_t;

/* 更新结果 */
typedef struct {
    hic_status_t        status;             /* 最终状态 */
    u64                 duration_ms;        /* 总耗时 */
    u32                 connections_migrated;/* 迁移的连接数 */
    u32                 connections_failed; /* 失败的连接数 */
    u32                 downtime_ms;        /* 停机时间（应为0） */
    bool                rolled_back;        /* 是否回滚 */
    char                error_msg[256];     /* 错误信息 */
} update_result_t;

/* 零停机更新 API */
hic_status_t zero_downtime_update(const char *name, 
                                   const char *new_path,
                                   update_strategy_t strategy,
                                   update_result_t *result);

hic_status_t zero_downtime_update_abort(const char *name);

hic_status_t zero_downtime_update_status(const char *name,
                                          update_context_t *ctx);

/* 策略实现函数 */
hic_status_t update_strategy_blue_green(const char *name,
                                         const char *new_path,
                                         update_result_t *result);

hic_status_t update_strategy_canary(const char *name,
                                     const char *new_path,
                                     u32 canary_percent,
                                     update_result_t *result);

hic_status_t update_strategy_gradual(const char *name,
                                      const char *new_path,
                                      u32 batch_size,
                                      update_result_t *result);

#endif /* MODULE_TYPES_H */