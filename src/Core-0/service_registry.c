/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC服务端点注册表实现 (Core-0版本)
 */

#include "include/service_registry.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "console.h"

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
    memzero(g_service_registry, sizeof(g_service_registry));
    g_service_count = 0;
    console_puts("[REGISTRY] Service registry initialized (");
    console_putu32(SERVICE_REGISTRY_SIZE);
    console_puts(" slots)\n");
}

/* ==================== 注册/注销 ==================== */

hic_status_t service_register_endpoint(const char *name,
                                        const u8 uuid[16],
                                        domain_id_t owner,
                                        cap_id_t endpoint_cap,
                                        endpoint_type_t type,
                                        u32 version)
{
    if (!name || name[0] == '\0') {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    /* 检查是否已存在 */
    if (find_by_name_internal(name)) {
        atomic_exit_critical(irq);
        console_puts("[REGISTRY] Service already exists: ");
        console_puts(name);
        console_puts("\n");
        return HIC_ERROR_ALREADY_EXISTS;
    }
    
    /* 查找空闲槽位 */
    service_endpoint_t *slot = NULL;
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE && !slot; i++) {
        if (g_service_registry[i].name[0] == '\0') {
            slot = &g_service_registry[i];
        }
    }
    
    if (!slot) {
        atomic_exit_critical(irq);
        console_puts("[REGISTRY] Registry full\n");
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 填充端点信息 */
    strncpy(slot->name, name, sizeof(slot->name) - 1);
    if (uuid) {
        memcpy(slot->uuid, uuid, 16);
    }
    slot->owner = owner;
    slot->endpoint_cap = endpoint_cap;
    slot->type = type;
    slot->state = SERVICE_STATE_RUNNING;
    slot->version = version;
    slot->flags = 0;
    
    g_service_count++;
    
    atomic_exit_critical(irq);
    
    console_puts("[REGISTRY] Registered: ");
    console_puts(name);
    console_puts(" (domain=");
    console_putu64(owner);
    console_puts(", cap=");
    console_putu64(endpoint_cap);
    console_puts(")\n");
    
    return HIC_SUCCESS;
}

hic_status_t service_unregister_endpoint(const char *name)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    memzero(endpoint, sizeof(service_endpoint_t));
    g_service_count--;
    
    atomic_exit_critical(irq);
    
    console_puts("[REGISTRY] Unregistered: ");
    console_puts(name);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/* 简化版服务注册（兼容接口） */
hic_status_t service_register(const char *name, domain_id_t owner, cap_id_t endpoint_cap)
{
    /* 使用默认 UUID (全零) 和类型 */
    static const u8 default_uuid[16] = {0};
    return service_register_endpoint(name, default_uuid, owner, endpoint_cap, 
                                      ENDPOINT_TYPE_GENERIC, 1);
}

/* ==================== 查找 ==================== */

service_endpoint_t* service_find_by_name(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    bool irq = atomic_enter_critical();
    service_endpoint_t *result = find_by_name_internal(name);
    atomic_exit_critical(irq);
    
    return result;
}

service_endpoint_t* service_find_by_uuid(const u8 uuid[16])
{
    if (!uuid) {
        return NULL;
    }
    
    bool irq = atomic_enter_critical();
    service_endpoint_t *result = find_by_uuid_internal(uuid);
    atomic_exit_critical(irq);
    
    return result;
}

service_endpoint_t* service_find_by_cap(cap_id_t cap_id)
{
    bool irq = atomic_enter_critical();
    
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE; i++) {
        if (g_service_registry[i].name[0] != '\0' &&
            g_service_registry[i].endpoint_cap == cap_id) {
            atomic_exit_critical(irq);
            return &g_service_registry[i];
        }
    }
    
    atomic_exit_critical(irq);
    return NULL;
}

/* ==================== 状态管理 ==================== */

hic_status_t service_set_state(const char *name, service_state_t state)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    endpoint->state = state;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

service_state_t service_get_state(const char *name)
{
    if (!name) {
        return SERVICE_STATE_STOPPED;
    }
    
    bool irq = atomic_enter_critical();
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    service_state_t state = endpoint ? endpoint->state : SERVICE_STATE_STOPPED;
    
    atomic_exit_critical(irq);
    
    return state;
}

/* ==================== 端点句柄 ==================== */

hic_status_t service_get_endpoint_handle(const char *name, cap_handle_t *handle)
{
    if (!name || !handle) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    *handle = endpoint->endpoint_handle;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/* ==================== 枚举 ==================== */

u32 service_enumerate(service_endpoint_t **endpoints, u32 max_count)
{
    bool irq = atomic_enter_critical();
    
    u32 count = 0;
    for (u32 i = 0; i < SERVICE_REGISTRY_SIZE && count < max_count; i++) {
        if (g_service_registry[i].name[0] != '\0') {
            if (endpoints) {
                endpoints[count] = &g_service_registry[i];
            }
            count++;
        }
    }
    
    atomic_exit_critical(irq);
    
    return count;
}
