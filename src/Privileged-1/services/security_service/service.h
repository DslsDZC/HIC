/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全服务策略层 (Security Service Policy Layer)
 * 
 * 实现安全策略决策：
 * - 安全策略配置与执行
 * - CFI 违规响应策略
 * - 密钥生命周期管理
 * - 安全事件响应
 */

#ifndef SECURITY_SERVICE_H
#define SECURITY_SERVICE_H

#include "../../include/common.h"

/* ========== 安全策略配置 ========== */

typedef enum security_policy_level {
    SECURITY_POLICY_PERMISSIVE = 0,   /* 宽松模式：仅记录 */
    SECURITY_POLICY_MODERATE,          /* 中等模式：记录+限制 */
    SECURITY_POLICY_STRICT,            /* 严格模式：阻止违规 */
    SECURITY_POLICY_PARANOID,          /* 偏执模式：阻止+审计 */
} security_policy_level_t;

typedef struct security_policy {
    security_policy_level_t level;
    
    /* 内存加密策略 */
    bool mem_enc_enabled;
    bool mem_enc_force_all;
    
    /* CFI 策略 */
    bool cfi_enabled;
    bool cfi_block_on_violation;
    u32 cfi_max_violations;
    
    /* 密钥策略 */
    u32 key_max_age_seconds;
    u32 key_max_use_count;
    bool key_auto_rotate;
    
    /* 审计策略 */
    bool audit_enabled;
    bool audit_on_violation;
} security_policy_t;

/* ========== CFI 违规响应 ========== */

typedef struct cfi_violation_event {
    u64 timestamp;
    u64 instruction_addr;
    u64 target_addr;
    u32 domain_id;
    bool blocked;
} cfi_violation_event_t;

typedef enum cfi_response_action {
    CFI_RESPONSE_LOG = 0,
    CFI_RESPONSE_WARN,
    CFI_RESPONSE_BLOCK,
    CFI_RESPONSE_TERMINATE_DOMAIN,
} cfi_response_action_t;

/* ========== 安全事件 ========== */

typedef enum security_event_type {
    SECURITY_EVENT_CFI_VIOLATION = 0,
    SECURITY_EVENT_MEM_ENC_FAILURE,
    SECURITY_EVENT_KEY_EXPIRED,
    SECURITY_EVENT_KEY_COMPROMISED,
    SECURITY_EVENT_TEE_FAILURE,
    SECURITY_EVENT_RNG_FAILURE,
} security_event_type_t;

typedef struct security_event {
    security_event_type_t type;
    u64 timestamp;
    u32 domain_id;
    union {
        cfi_violation_event_t cfi;
        struct { u64 key_id; } key;
    } data;
} security_event_t;

/* ========== 密钥管理策略 ========== */

typedef struct key_policy {
    u32 max_lifetime_seconds;
    u32 max_use_count;
    bool auto_rotate;
    u32 rotation_interval_seconds;
    bool require_hw_backed;
} key_policy_t;

/* ========== 策略层接口 ========== */

/* 初始化 */
void security_service_init(void);

/* 安全策略 */
void security_set_policy(const security_policy_t *policy);
const security_policy_t* security_get_policy(void);
void security_apply_policy(void);

/* CFI 违规处理 */
cfi_response_action_t security_handle_cfi_violation(const cfi_violation_event_t *event);
void security_set_cfi_response(security_policy_level_t level, cfi_response_action_t action);

/* 安全事件 */
void security_handle_event(const security_event_t *event);
u64 security_get_event_count(security_event_type_t type);

/* 密钥管理 */
u64 security_create_key(u32 type, u32 algo, const void *data, u32 size);
void security_rotate_key(u64 key_id);
void security_check_key_policies(void);

/* 服务入口 */
int security_service_start(void);

#endif /* SECURITY_SERVICE_H */
