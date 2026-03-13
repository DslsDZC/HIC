/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务端点注册表 (Privileged-1版本)
 */

#ifndef HIC_SERVICE_REGISTRY_H
#define HIC_SERVICE_REGISTRY_H

#include <common.h>

/* 最大服务数 */
#define SERVICE_REGISTRY_SIZE 64

/* 端点类型 */
typedef enum endpoint_type {
    ENDPOINT_TYPE_GENERIC,      /* 通用端点 */
    ENDPOINT_TYPE_FILESYSTEM,   /* 文件系统 */
    ENDPOINT_TYPE_SIGNER,       /* 签名验证 */
    ENDPOINT_TYPE_MODULE_MGR,   /* 模块管理 */
    ENDPOINT_TYPE_DEVICE,       /* 设备服务 */
} endpoint_type_t;

/* 服务状态 */
typedef enum service_state {
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_STARTING,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_ERROR,
} service_state_t;

/* 服务端点结构 */
typedef struct service_endpoint {
    char name[64];                /* 服务名称 */
    u8 uuid[16];                  /* 服务 UUID */
    u32 owner;                    /* 所属域（简化为 u32） */
    u32 endpoint_cap;             /* 端点能力 ID（简化为 u32） */
    u32 endpoint_handle;          /* 端点句柄（简化为 u32） */
    endpoint_type_t type;         /* 端点类型 */
    service_state_t state;        /* 服务状态 */
    u32 version;                  /* API 版本 */
    u32 flags;                    /* 标志 */
} service_endpoint_t;

/* ==================== 初始化 ==================== */

void service_registry_init(void);

/* ==================== 注册/注销 ==================== */

hic_status_t service_register_endpoint(service_endpoint_t *endpoint);
hic_status_t service_unregister_endpoint(const char *name);

/* ==================== 查找 ==================== */

service_endpoint_t* service_find_by_name(const char *name);
service_endpoint_t* service_find_by_uuid(const u8 uuid[16]);
service_endpoint_t* service_find_by_cap(u32 cap_id);

/* ==================== 状态管理 ==================== */

hic_status_t service_set_state(const char *name, service_state_t state);
service_state_t service_get_state(const char *name);

/* ==================== 枚举 ==================== */

u32 service_enumerate(service_endpoint_t **endpoints, u32 max_count);

#endif /* HIC_SERVICE_REGISTRY_H */
