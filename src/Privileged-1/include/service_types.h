/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务类型定义
 */

#ifndef HIC_SERVICE_TYPES_H
#define HIC_SERVICE_TYPES_H

#include "../Core-0/types.h"

/* 服务类型 */
typedef enum {
    SERVICE_TYPE_CORE = 0,        /* 核心服务（必须运行） */
    SERVICE_TYPE_STANDARD,         /* 标准服务 */
    SERVICE_TYPE_EXTENSION,        /* 扩展服务（可选） */
    SERVICE_TYPE_DRIVER,           /* 驱动服务 */
    SERVICE_TYPE_MONITOR,          /* 监控服务 */
} service_type_t;

/* 服务元数据 */
typedef struct service_metadata {
    /* 基本信息 */
    char name[64];
    char display_name[128];
    char version[16];
    char api_version[16];
    u8 uuid[16];
    
    /* 作者信息 */
    char author_name[64];
    char author_email[128];
    char license[128];
    
    /* 描述 */
    char short_desc[256];
    char long_desc[1024];
    
    /* 依赖 */
    u32 dependency_count;
    char dependencies[16][64];  /* service_name:min_version:max_version */
    
    /* 资源需求 */
    u64 max_memory;
    u32 max_threads;
    u32 max_capabilities;
    u32 cpu_quota_percent;
    
    /* 端点 */
    u32 endpoint_count;
    char endpoints[32][128];
    
    /* 权限 */
    u32 permission_count;
    char permissions[32][64];
    
    /* 安全设置 */
    bool critical;
    bool privileged;
    bool signature_required;
    
    /* 编译配置 */
    bool static_build;
    u32 priority;
    bool autostart;
} service_metadata_t;

/* 服务配置 */
typedef struct service_config {
    bool enabled;
    u32 start_delay_ms;
    u32 restart_count;
    bool auto_restart;
} service_config_t;

/* 服务统计 */
typedef struct service_stats {
    u64 start_time;
    u64 uptime_ms;
    u64 syscall_count;
    u64 ipc_count;
    u64 memory_used;
    u32 active_threads;
    service_state_t state;
} service_stats_t;

#endif /* HIC_SERVICE_TYPES_H */