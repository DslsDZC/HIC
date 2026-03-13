/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务端点注册表实现 (Privileged-1版本)
 */

#include "service_registry.h"
#include <common.h>
#include <string.h>

/* 全局服务注册表 */
static service_endpoint_t g_service_registry[SERVICE_REGISTRY_SIZE];
static u32 g_service_count = 0;

/* ==================== 内部辅助函数 ==================== */

static service_endpoint_t* find_by_name_internal(const char *name)
{
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE; i++) {
        if (g_service_registry[i].name[0] != '\0' &&
            strcmp(g_service_registry[i].name, name) == 0) {
            return &g_service_registry[i];
        }
    }
    return NULL;
}

static service_endpoint_t* find_by_uuid_internal(const u8 uuid[16])
{
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE; i++) {
        if (g_service_registry[i].name[0] != '\0' &&
            memcmp(g_service_registry[i].uuid, uuid, 16) == 0) {
            return &g_service_registry[i];
        }
    }
    return NULL;
}

/* ==================== 初始化 ==================== */

void service_registry_init(void)
{
    memset(g_service_registry, 0, sizeof(g_service_registry));
    g_service_count = 0;
}

/* ==================== 注册/注销 ==================== */

hic_status_t service_register_endpoint(service_endpoint_t *endpoint)
{
    if (!endpoint || endpoint->name[0] == '\0') {
        return HIC_INVALID_PARAM;
    }
    
    /* 检查是否已存在 */
    if (find_by_name_internal(endpoint->name)) {
        return HIC_BUSY;
    }
    
    /* 查找空闲槽位 */
    service_endpoint_t *slot = NULL;
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE && !slot; i++) {
        if (g_service_registry[i].name[0] == '\0') {
            slot = &g_service_registry[i];
        }
    }
    
    if (!slot) {
        return HIC_OUT_OF_MEMORY;
    }
    
    /* 复制端点信息 */
    *slot = *endpoint;
    slot->state = SERVICE_STATE_RUNNING;
    
    g_service_count++;
    
    return HIC_SUCCESS;
}

hic_status_t service_unregister_endpoint(const char *name)
{
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        return HIC_NOT_FOUND;
    }
    
    memset(endpoint, 0, sizeof(service_endpoint_t));
    g_service_count--;
    
    return HIC_SUCCESS;
}

/* ==================== 查找 ==================== */

service_endpoint_t* service_find_by_name(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    return find_by_name_internal(name);
}

service_endpoint_t* service_find_by_uuid(const u8 uuid[16])
{
    if (!uuid) {
        return NULL;
    }
    
    return find_by_uuid_internal(uuid);
}

service_endpoint_t* service_find_by_cap(u32 cap_id)
{
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE; i++) {
        if (g_service_registry[i].name[0] != '\0' &&
            g_service_registry[i].endpoint_cap == cap_id) {
            return &g_service_registry[i];
        }
    }
    
    return NULL;
}

/* ==================== 状态管理 ==================== */

hic_status_t service_set_state(const char *name, service_state_t state)
{
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        return HIC_NOT_FOUND;
    }
    
    endpoint->state = state;
    
    return HIC_SUCCESS;
}

service_state_t service_get_state(const char *name)
{
    if (!name) {
        return SERVICE_STATE_STOPPED;
    }
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    return endpoint ? endpoint->state : SERVICE_STATE_STOPPED;
}

/* ==================== 枚举 ==================== */

u32 service_enumerate(service_endpoint_t **endpoints, u32 max_count)
{
    u32 count = 0;
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE && count < max_count; i++) {
        if (g_service_registry[i].name[0] != '\0') {
            if (endpoints) {
                endpoints[count] = &g_service_registry[i];
            }
            count++;
        }
    }
    
    return count;
}
