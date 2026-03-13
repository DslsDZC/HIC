/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务端点注册表 (Core-0版本)
 * 
 * 用于 Core-0 静态模块的服务注册
 */

#ifndef HIC_CORE0_SERVICE_REGISTRY_H
#define HIC_CORE0_SERVICE_REGISTRY_H

#include "../types.h"
#include "../capability.h"

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
    domain_id_t owner;            /* 所属域 */
    cap_id_t endpoint_cap;        /* 端点能力 ID */
    cap_handle_t endpoint_handle; /* 端点句柄 */
    endpoint_type_t type;         /* 端点类型 */
    service_state_t state;        /* 服务状态 */
    u32 version;                  /* API 版本 */
    u32 flags;                    /* 标志 */
} service_endpoint_t;

/* ==================== 初始化 ==================== */

void service_registry_init(void);

/* ==================== 注册/注销 ==================== */

hic_status_t service_register_endpoint(const char *name,
                                        const u8 uuid[16],
                                        domain_id_t owner,
                                        cap_id_t endpoint_cap,
                                        endpoint_type_t type,
                                        u32 version);

hic_status_t service_unregister_endpoint(const char *name);

/* ==================== 查找 ==================== */

service_endpoint_t* service_find_by_name(const char *name);
service_endpoint_t* service_find_by_uuid(const u8 uuid[16]);
service_endpoint_t* service_find_by_cap(cap_id_t cap_id);

/* ==================== 状态管理 ==================== */

hic_status_t service_set_state(const char *name, service_state_t state);
service_state_t service_get_state(const char *name);

/* ==================== 端点句柄 ==================== */

hic_status_t service_get_endpoint_handle(const char *name, cap_handle_t *handle);

/* ==================== 枚举 ==================== */

u32 service_enumerate(service_endpoint_t **endpoints, u32 max_count);

#endif /* HIC_CORE0_SERVICE_REGISTRY_H */
