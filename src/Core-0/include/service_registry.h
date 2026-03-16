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
#include "../monitor.h"  /* service_state_t */

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

/* ==================== IPC 流量控制 ==================== */

/* 流量控制策略 */
typedef enum flow_control_policy {
    FLOW_CONTROL_NONE,          /* 无流量控制 */
    FLOW_CONTROL_CREDIT,        /* 信用机制 */
    FLOW_CONTROL_BACKPRESSURE,  /* 队列背压 */
    FLOW_CONTROL_HYBRID,        /* 混合模式 */
} flow_control_policy_t;

/* 流量控制状态 */
typedef struct flow_control_state {
    flow_control_policy_t policy;    /* 控制策略 */
    
    /* 信用机制 */
    u32 credits_available;            /* 可用信用 */
    u32 credits_total;                /* 总信用 */
    u32 credit_refill_rate;           /* 信用补充速率（个/秒） */
    u64 last_refill_time;             /* 上次补充时间 */
    
    /* 队列背压 */
    u32 queue_depth;                  /* 当前队列深度 */
    u32 queue_max_depth;              /* 最大队列深度 */
    u32 queue_high_watermark;         /* 高水位线（触发背压） */
    u32 queue_low_watermark;          /* 低水位线（解除背压） */
    
    /* 统计 */
    u64 total_messages_sent;          /* 总发送消息数 */
    u64 total_messages_dropped;       /* 总丢弃消息数 */
    u64 total_backpressure_events;    /* 背压事件数 */
    u64 total_blocked_time_us;        /* 总阻塞时间（微秒） */
} flow_control_state_t;

/* 流量控制动作 */
typedef enum flow_control_action {
    FLOW_ACTION_ACCEPT,        /* 接受消息 */
    FLOW_ACTION_BLOCK,         /* 阻塞发送方 */
    FLOW_ACTION_DROP,          /* 丢弃消息 */
    FLOW_ACTION_THROTTLE,      /* 限流 */
} flow_control_action_t;

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
    
    /* 流量控制状态 */
    flow_control_state_t flow;    /* 流量控制 */
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

/**
 * @brief 简化版服务注册（兼容接口）
 * 用于动态模块加载器
 */
hic_status_t service_register(const char *name, domain_id_t owner, cap_id_t endpoint_cap);

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

/* ==================== 流量控制机制层原语 ==================== */

/**
 * 初始化端点流量控制
 * @param name 服务名称
 * @param policy 控制策略
 * @param config 配置参数（credits_total, queue_max_depth 等）
 * @return 状态码
 */
hic_status_t flow_control_init(const char *name, flow_control_policy_t policy,
                                const u32 *config);

/**
 * 检查是否可以发送消息（机制层）
 * @param endpoint 端点指针
 * @param sender_domain 发送方域ID
 * @return 动作类型
 */
flow_control_action_t flow_control_check(service_endpoint_t *endpoint,
                                          domain_id_t sender_domain);

/**
 * 消费信用（发送消息后调用）
 * @param endpoint 端点指针
 * @param count 消费数量
 * @return 成功返回 true
 */
bool flow_control_consume_credits(service_endpoint_t *endpoint, u32 count);

/**
 * 补充信用（接收方处理后调用）
 * @param name 服务名称
 * @param count 补充数量
 * @return 状态码
 */
hic_status_t flow_control_refill_credits(const char *name, u32 count);

/**
 * 增加队列深度（消息入队时调用）
 * @param endpoint 端点指针
 * @return 是否触发背压
 */
bool flow_control_enqueue(service_endpoint_t *endpoint);

/**
 * 减少队列深度（消息出队时调用）
 * @param endpoint 端点指针
 * @return 是否解除背压
 */
bool flow_control_dequeue(service_endpoint_t *endpoint);

/**
 * 获取流量控制统计
 * @param name 服务名称
 * @param stats 输出统计信息
 * @return 状态码
 */
hic_status_t flow_control_get_stats(const char *name, flow_control_state_t *stats);

/**
 * 设置流量控制策略（策略层调用）
 * @param name 服务名称
 * @param policy 新策略
 * @param config 配置参数
 * @return 状态码
 */
hic_status_t flow_control_set_policy(const char *name, flow_control_policy_t policy,
                                      const u32 *config);

#endif /* HIC_CORE0_SERVICE_REGISTRY_H */
