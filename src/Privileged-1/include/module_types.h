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
    
    /* 自动重启 */
    u8                 auto_restart;      /* 是否自动重启 */
    u64                last_error_time;   /* 最后错误时间 */
    
    /* 备份（用于滚动更新） */
    void               *backup_state;     /* 备份状态 */
    u32                 backup_size;      /* 备份大小 */
} module_instance_t;

/* 模块信息（简化版，用于 CLI） */
typedef struct {
    char                name[64];         /* 模块名称 */
    u8                  uuid[16];         /* 模块 UUID */
    u32                 version;          /* 语义版本 */
    module_state_t      state;            /* 模块状态 */
    u32                 flags;            /* 模块标志 */
} module_info_t;

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

#endif /* MODULE_TYPES_H */