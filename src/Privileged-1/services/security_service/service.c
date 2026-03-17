/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全服务策略层实现
 */

#include "service.h"

/* 外部机制层接口 */
extern void sec_init(void);
extern const void* sec_get_state(void);
extern bool sec_cfi_supported(int mode);
extern int sec_cfi_enable(int mode);
extern void sec_cfi_disable(void);
extern u64 sec_cfi_get_violations(void);
extern bool sec_mem_enc_supported(int type);
extern int sec_mem_enc_enable(int type);
extern void sec_mem_enc_disable(void);
extern u64 sec_key_create(u32 type, u32 algo, const void *data, u32 size, u32 flags);
extern void sec_key_destroy(u64 key_id);
extern bool sec_key_use(u64 key_id, void *data, u32 size, bool encrypt);

/* 日志输出 */
static void log_info(const char *msg) { extern void serial_print(const char *); serial_print("[SEC_SVC] "); serial_print(msg); serial_print("\n"); }
static void log_warn(const char *msg) { extern void serial_print(const char *); serial_print("[SEC_SVC WARN] "); serial_print(msg); serial_print("\n"); }

/* 全局策略配置 */
static security_policy_t g_policy = {
    .level = SECURITY_POLICY_MODERATE,
    .mem_enc_enabled = false,
    .mem_enc_force_all = false,
    .cfi_enabled = false,
    .cfi_block_on_violation = true,
    .cfi_max_violations = 10,
    .key_max_age_seconds = 86400,
    .key_max_use_count = 1000000,
    .key_auto_rotate = true,
    .audit_enabled = true,
    .audit_on_violation = true,
};

/* CFI 违规响应表 */
static cfi_response_action_t g_cfi_response_table[] = {
    [SECURITY_POLICY_PERMISSIVE] = CFI_RESPONSE_LOG,
    [SECURITY_POLICY_MODERATE] = CFI_RESPONSE_WARN,
    [SECURITY_POLICY_STRICT] = CFI_RESPONSE_BLOCK,
    [SECURITY_POLICY_PARANOID] = CFI_RESPONSE_TERMINATE_DOMAIN,
};

/* 事件统计 */
static u64 g_event_counts[6] = {0};

/* ========== 初始化 ========== */

void security_service_init(void)
{
    log_info("Security service initializing...");
    
    /* 应用默认策略 */
    security_apply_policy();
    
    log_info("Security service ready");
}

/* ========== 策略管理 ========== */

void security_set_policy(const security_policy_t *policy)
{
    if (policy) {
        g_policy = *policy;
        security_apply_policy();
    }
}

const security_policy_t* security_get_policy(void)
{
    return &g_policy;
}

void security_apply_policy(void)
{
    log_info("Applying security policy...");
    
    /* CFI 策略 */
    if (g_policy.cfi_enabled) {
        if (sec_cfi_supported(1)) {  /* SHADOW_STACK */
            sec_cfi_enable(1);
            log_info("CFI enabled (shadow stack)");
        }
    } else {
        sec_cfi_disable();
        log_info("CFI disabled");
    }
    
    /* 内存加密策略 */
    if (g_policy.mem_enc_enabled) {
        if (sec_mem_enc_supported(1)) {  /* AMD SEV */
            sec_mem_enc_enable(1);
            log_info("Memory encryption enabled");
        }
    } else {
        sec_mem_enc_disable();
    }
}

/* ========== CFI 违规处理 ========== */

cfi_response_action_t security_handle_cfi_violation(const cfi_violation_event_t *event)
{
    if (!event) return CFI_RESPONSE_LOG;
    
    g_event_counts[SECURITY_EVENT_CFI_VIOLATION]++;
    
    /* 检查是否超过最大违规次数 */
    u64 total_violations = sec_cfi_get_violations();
    if (total_violations > g_policy.cfi_max_violations) {
        log_warn("CFI violation threshold exceeded");
        return CFI_RESPONSE_TERMINATE_DOMAIN;
    }
    
    /* 根据策略级别决定响应 */
    cfi_response_action_t action = g_cfi_response_table[g_policy.level];
    
    switch (action) {
        case CFI_RESPONSE_LOG:
            log_info("CFI violation logged");
            break;
        case CFI_RESPONSE_WARN:
            log_warn("CFI violation detected");
            break;
        case CFI_RESPONSE_BLOCK:
            log_warn("CFI violation blocked");
            return CFI_RESPONSE_BLOCK;
        case CFI_RESPONSE_TERMINATE_DOMAIN:
            log_warn("CFI violation - terminating domain");
            /* 调用域管理器终止域 */
            break;
    }
    
    return action;
}

void security_set_cfi_response(security_policy_level_t level, cfi_response_action_t action)
{
    if (level < 4) {
        g_cfi_response_table[level] = action;
    }
}

/* ========== 安全事件处理 ========== */

void security_handle_event(const security_event_t *event)
{
    if (!event) return;
    
    g_event_counts[event->type]++;
    
    switch (event->type) {
        case SECURITY_EVENT_CFI_VIOLATION:
            security_handle_cfi_violation(&event->data.cfi);
            break;
            
        case SECURITY_EVENT_MEM_ENC_FAILURE:
            log_warn("Memory encryption failure");
            break;
            
        case SECURITY_EVENT_KEY_EXPIRED:
            log_warn("Key expired");
            if (g_policy.key_auto_rotate) {
                security_rotate_key(event->data.key.key_id);
            }
            break;
            
        case SECURITY_EVENT_KEY_COMPROMISED:
            log_warn("Key compromised - destroying");
            sec_key_destroy(event->data.key.key_id);
            break;
            
        case SECURITY_EVENT_TEE_FAILURE:
            log_warn("TEE failure detected");
            break;
            
        case SECURITY_EVENT_RNG_FAILURE:
            log_warn("RNG failure - entropy may be degraded");
            break;
    }
}

u64 security_get_event_count(security_event_type_t type)
{
    if (type < 6) {
        return g_event_counts[type];
    }
    return 0;
}

/* ========== 密钥管理 ========== */

u64 security_create_key(u32 type, u32 algo, const void *data, u32 size)
{
    u32 flags = 0;
    if (g_policy.key_auto_rotate) {
        flags |= (1 << 2);  /* DERIVE */
    }
    
    u64 key_id = sec_key_create(type, algo, data, size, flags);
    
    if (key_id != 0) {
        log_info("Key created");
    }
    
    return key_id;
}

void security_rotate_key(u64 key_id)
{
    log_info("Rotating key...");
    
    /* 派生新密钥 */
    u64 new_key = sec_key_create(2, 0, NULL, 0, 0);  /* DERIVED, AES-256 */
    
    if (new_key != 0) {
        sec_key_destroy(key_id);
        log_info("Key rotation complete");
    } else {
        log_warn("Key rotation failed");
    }
}

void security_check_key_policies(void)
{
    /* 检查所有密钥是否符合策略 */
    log_info("Checking key policies...");
}

/* ========== 服务入口 ========== */

int security_service_start(void)
{
    security_service_init();
    
    log_info("Security service started");
    return 0;
}
