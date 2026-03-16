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

/* ==================== 流量控制机制层实现 ==================== */

/**
 * 初始化端点流量控制
 */
hic_status_t flow_control_init(const char *name, flow_control_policy_t policy,
                                const u32 *config)
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
    
    /* 初始化流量控制状态 */
    flow_control_state_t *fc = &endpoint->flow;
    memzero(fc, sizeof(flow_control_state_t));
    fc->policy = policy;
    
    /* 根据策略设置默认值 */
    switch (policy) {
        case FLOW_CONTROL_CREDIT:
            fc->credits_total = config ? config[0] : 16;
            fc->credits_available = fc->credits_total;
            fc->credit_refill_rate = config ? config[1] : 4;
            break;
            
        case FLOW_CONTROL_BACKPRESSURE:
            fc->queue_max_depth = config ? config[0] : 32;
            fc->queue_high_watermark = (fc->queue_max_depth * 3) / 4;  /* 75% */
            fc->queue_low_watermark = fc->queue_max_depth / 4;         /* 25% */
            break;
            
        case FLOW_CONTROL_HYBRID:
            fc->credits_total = config ? config[0] : 16;
            fc->credits_available = fc->credits_total;
            fc->credit_refill_rate = config ? config[1] : 4;
            fc->queue_max_depth = config ? config[2] : 32;
            fc->queue_high_watermark = (fc->queue_max_depth * 3) / 4;
            fc->queue_low_watermark = fc->queue_max_depth / 4;
            break;
            
        default:
            break;
    }
    
    fc->last_refill_time = 0;  /* 由调用者设置 */
    
    atomic_exit_critical(irq);
    
    console_puts("[FLOW] Initialized flow control for ");
    console_puts(name);
    console_puts(", policy=");
    console_putu32(policy);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/**
 * 检查是否可以发送消息
 */
flow_control_action_t flow_control_check(service_endpoint_t *endpoint,
                                          domain_id_t sender_domain)
{
    (void)sender_domain;  /* 可用于未来扩展：基于发送方的差异化处理 */
    
    if (!endpoint) {
        return FLOW_ACTION_DROP;
    }
    
    flow_control_state_t *fc = &endpoint->flow;
    
    switch (fc->policy) {
        case FLOW_CONTROL_NONE:
            return FLOW_ACTION_ACCEPT;
            
        case FLOW_CONTROL_CREDIT:
            /* 检查信用是否足够 */
            if (fc->credits_available > 0) {
                return FLOW_ACTION_ACCEPT;
            }
            /* 无信用，阻塞 */
            fc->total_backpressure_events++;
            return FLOW_ACTION_BLOCK;
            
        case FLOW_CONTROL_BACKPRESSURE:
            /* 检查队列深度 */
            if (fc->queue_depth >= fc->queue_high_watermark) {
                if (fc->queue_depth >= fc->queue_max_depth) {
                    fc->total_messages_dropped++;
                    return FLOW_ACTION_DROP;
                }
                fc->total_backpressure_events++;
                return FLOW_ACTION_BLOCK;
            }
            return FLOW_ACTION_ACCEPT;
            
        case FLOW_CONTROL_HYBRID:
            /* 先检查信用 */
            if (fc->credits_available == 0) {
                fc->total_backpressure_events++;
                return FLOW_ACTION_BLOCK;
            }
            /* 再检查队列 */
            if (fc->queue_depth >= fc->queue_high_watermark) {
                fc->total_messages_dropped++;
                return FLOW_ACTION_THROTTLE;
            }
            return FLOW_ACTION_ACCEPT;
    }
    
    return FLOW_ACTION_ACCEPT;
}

/**
 * 消费信用
 */
bool flow_control_consume_credits(service_endpoint_t *endpoint, u32 count)
{
    if (!endpoint || count == 0) {
        return false;
    }
    
    flow_control_state_t *fc = &endpoint->flow;
    
    if (fc->policy == FLOW_CONTROL_NONE) {
        return true;
    }
    
    if (fc->credits_available >= count) {
        fc->credits_available -= count;
        fc->total_messages_sent += count;
        return true;
    }
    
    return false;
}

/**
 * 补充信用
 */
hic_status_t flow_control_refill_credits(const char *name, u32 count)
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
    
    flow_control_state_t *fc = &endpoint->flow;
    
    fc->credits_available += count;
    if (fc->credits_available > fc->credits_total) {
        fc->credits_available = fc->credits_total;
    }
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 增加队列深度
 */
bool flow_control_enqueue(service_endpoint_t *endpoint)
{
    if (!endpoint) {
        return false;
    }
    
    flow_control_state_t *fc = &endpoint->flow;
    
    if (fc->queue_depth < fc->queue_max_depth) {
        fc->queue_depth++;
        
        /* 检查是否触发背压 */
        if (fc->queue_depth >= fc->queue_high_watermark) {
            fc->total_backpressure_events++;
            return true;  /* 触发背压 */
        }
    }
    
    return false;
}

/**
 * 减少队列深度
 */
bool flow_control_dequeue(service_endpoint_t *endpoint)
{
    if (!endpoint) {
        return false;
    }
    
    flow_control_state_t *fc = &endpoint->flow;
    
    if (fc->queue_depth > 0) {
        fc->queue_depth--;
        
        /* 检查是否解除背压 */
        if (fc->queue_depth <= fc->queue_low_watermark) {
            return true;  /* 解除背压 */
        }
    }
    
    return false;
}

/**
 * 获取流量控制统计
 */
hic_status_t flow_control_get_stats(const char *name, flow_control_state_t *stats)
{
    if (!name || !stats) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    bool irq = atomic_enter_critical();
    
    service_endpoint_t *endpoint = find_by_name_internal(name);
    if (!endpoint) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NOT_FOUND;
    }
    
    *stats = endpoint->flow;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/**
 * 设置流量控制策略（策略层调用）
 */
hic_status_t flow_control_set_policy(const char *name, flow_control_policy_t policy,
                                      const u32 *config)
{
    /* 重新初始化即可 */
    return flow_control_init(name, policy, config);
}
