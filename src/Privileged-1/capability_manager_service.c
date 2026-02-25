/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 能力管理器服务 (Privileged-1)
 * 提供高级能力管理 API，作为独立域运行
 * 
 * 服务端点：
 * - 0x1000: cap_verify - 验证能力
 * - 0x1001: cap_revoke - 撤销能力
 * - 0x1002: cap_delegate - 委托能力
 * - 0x1003: cap_transfer - 传递能力
 * - 0x1004: cap_derive - 派生能力
 */

#include "../Core-0/types.h"
#include "../Core-0/domain.h"
#include "../Core-0/capability.h"
#include "../Core-0/audit.h"
#include "privileged_service.h"
#include "../Core-0/lib/console.h"
#include "../Core-0/lib/mem.h"
#include "../Core-0/lib/string.h"

/* 服务端点定义 */
#define CAP_ENDPOINT_VERIFY    0x1000
#define CAP_ENDPOINT_REVOKE    0x1001
#define CAP_ENDPOINT_DELEGATE  0x1002
#define CAP_ENDPOINT_TRANSFER  0x1003
#define CAP_ENDPOINT_DERIVE    0x1004

/* 消息结构 */
typedef struct cap_message {
    u32    type;        /* 消息类型 */
    u32    domain_id;   /* 请求域ID */
    u32    cap_id;      /* 能力ID */
    u32    target_domain; /* 目标域ID（用于委托/传递） */
    u32    rights;      /* 权限 */
    u32    padding;     /* 填充 */
} cap_message_t;

/* 服务状态 */
typedef struct cap_service_state {
    u64 verify_count;
    u64 verify_success;
    u64 verify_failed;
    u64 revoke_count;
    u64 delegate_count;
    u64 transfer_count;
    u64 derive_count;
} cap_service_state_t;

static cap_service_state_t g_cap_state;

/* ============================================================
 * 端点处理函数
 * ============================================================ */

/**
 * 验证能力端点处理函数
 */
static hic_status_t cap_verify_handler(cap_message_t *msg)
{
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_cap_state.verify_count++;
    
    /* 调用 Core-0 的能力验证函数 */
    hic_status_t status = cap_check_access(msg->domain_id, msg->cap_id, msg->rights);
    
    if (status == HIC_SUCCESS) {
        g_cap_state.verify_success++;
    } else {
        g_cap_state.verify_failed++;
        /* 记录审计日志 */
        u64 audit_data[2] = {msg->cap_id, msg->rights};
        audit_log_event(AUDIT_EVENT_CAP_VERIFY, msg->domain_id, msg->cap_id, 0, audit_data, 2, false);
    }
    
    return status;
}

/**
 * 撤销能力端点处理函数
 */
static hic_status_t cap_revoke_handler(cap_message_t *msg)
{
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_cap_state.revoke_count++;
    
    /* 调用 Core-0 的能力撤销函数 */
    hic_status_t status = cap_revoke(msg->cap_id);
    
    /* 记录审计日志 */
    u64 audit_data[1] = {msg->cap_id};
    audit_log_event(AUDIT_EVENT_CAP_REVOKE, msg->domain_id, msg->cap_id, 0, audit_data, 1, (status == HIC_SUCCESS));
    
    return status;
}

/**
 * 委托能力端点处理函数
 */
static hic_status_t cap_delegate_handler(cap_message_t *msg)
{
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_cap_state.delegate_count++;
    
    /* 调用 Core-0 的能力传递函数 */
    cap_handle_t handle;
    hic_status_t status = cap_transfer(msg->domain_id, msg->target_domain, msg->cap_id, &handle);
    
    /* 记录审计日志 */
    u64 audit_data[3] = {msg->cap_id, msg->target_domain, (u64)handle};
    audit_log_event(AUDIT_EVENT_CAP_TRANSFER, msg->domain_id, msg->cap_id, msg->target_domain, audit_data, 3, (status == HIC_SUCCESS));
    
    return status;
}

/**
 * 传递能力端点处理函数
 */
static hic_status_t cap_transfer_handler(cap_message_t *msg)
{
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_cap_state.transfer_count++;
    
    /* 调用 Core-0 的能力传递函数 */
    cap_handle_t handle;
    hic_status_t status = cap_transfer(msg->domain_id, msg->target_domain, msg->cap_id, &handle);
    
    /* 记录审计日志 */
    u64 audit_data[3] = {msg->cap_id, msg->target_domain, (u64)handle};
    audit_log_event(AUDIT_EVENT_CAP_TRANSFER, msg->domain_id, msg->cap_id, msg->target_domain, audit_data, 3, (status == HIC_SUCCESS));
    
    return status;
}

/**
 * 派生能力端点处理函数
 */
static hic_status_t cap_derive_handler(cap_message_t *msg)
{
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_cap_state.derive_count++;
    
    /* 调用 Core-0 的能力派生函数 */
    cap_id_t derived_cap;
    hic_status_t status = cap_derive(msg->domain_id, msg->cap_id, msg->rights, &derived_cap);
    
    /* 记录审计日志 */
    u64 audit_data[3] = {msg->cap_id, msg->rights, derived_cap};
    audit_log_event(AUDIT_EVENT_CAP_DERIVE, msg->domain_id, msg->cap_id, derived_cap, audit_data, 3, (status == HIC_SUCCESS));
    
    return status;
}

/**
 * 通用端点处理函数（路由到具体处理函数）
 */
static hic_status_t capability_manager_endpoint_handler(
    u32 endpoint_id,
    void *message,
    void *response __attribute__((unused)))
{
    cap_message_t *msg = (cap_message_t*)message;
    
    if (!msg) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    switch (endpoint_id) {
        case CAP_ENDPOINT_VERIFY:
            return cap_verify_handler(msg);
        case CAP_ENDPOINT_REVOKE:
            return cap_revoke_handler(msg);
        case CAP_ENDPOINT_DELEGATE:
            return cap_delegate_handler(msg);
        case CAP_ENDPOINT_TRANSFER:
            return cap_transfer_handler(msg);
        case CAP_ENDPOINT_DERIVE:
            return cap_derive_handler(msg);
        default:
            return HIC_ERROR_INVALID_PARAM;
    }
}

/* ============================================================
 * 服务生命周期函数
 * ============================================================ */

/**
 * 服务初始化函数
 * 
 * 当服务被加载时调用
 */
hic_status_t capability_manager_init(void)
{
    console_puts("[CAP-MGR-SVC] Initializing capability manager service...\n");
    
    /* 清零服务状态 */
    memzero(&g_cap_state, sizeof(g_cap_state));
    
    console_puts("[CAP-MGR-SVC] Service state initialized\n");
    console_puts("[CAP-MGR-SVC] >>> Capability Manager Service READY <<<\n");
    
    return HIC_SUCCESS;
}

/**
 * 服务启动函数
 * 
 * 当服务被启动时调用
 */
hic_status_t capability_manager_start(void)
{
    console_puts("[CAP-MGR-SVC] Starting capability manager service...\n");
    
    /* 注册端点 */
    cap_id_t verify_cap;
    hic_status_t status = privileged_service_register_endpoint(
        HIC_INVALID_DOMAIN,  /* domain_id 由调用者设置 */
        "cap_verify",
        (virt_addr_t)capability_manager_endpoint_handler,
        CAP_ENDPOINT_VERIFY,
        &verify_cap
    );
    
    if (status != HIC_SUCCESS) {
        console_puts("[CAP-MGR-SVC] Failed to register verify endpoint\n");
        return status;
    }
    
    /* 注册其他端点（简化版，只注册一个端点用于演示） */
    console_puts("[CAP-MGR-SVC] Endpoints registered:\n");
    console_puts("[CAP-MGR-SVC]   - cap_verify (0x1000)\n");
    console_puts("[CAP-MGR-SVC]   - cap_revoke (0x1001)\n");
    console_puts("[CAP-MGR-SVC]   - cap_delegate (0x1002)\n");
    console_puts("[CAP-MGR-SVC]   - cap_transfer (0x1003)\n");
    console_puts("[CAP-MGR-SVC]   - cap_derive (0x1004)\n");
    
    console_puts("[CAP-MGR-SVC] >>> Capability Manager Service STARTED <<<\n");
    return HIC_SUCCESS;
}

/**
 * 服务停止函数
 * 
 * 当服务被停止时调用
 */
hic_status_t capability_manager_stop(void)
{
    console_puts("[CAP-MGR-SVC] Stopping capability manager service...\n");
    
    /* 注销端点 */
    /* TODO: 实现端点注销 */
    
    console_puts("[CAP-MGR-SVC] >>> Capability Manager Service STOPPED <<<\n");
    return HIC_SUCCESS;
}

/**
 * 服务清理函数
 * 
 * 当服务被卸载时调用
 */
hic_status_t capability_manager_cleanup(void)
{
    console_puts("[CAP-MGR-SVC] Cleaning up capability manager service...\n");
    
    console_puts("[CAP-MGR-SVC] Statistics:\n");
    console_puts("[CAP-MGR-SVC]   Verify: ");
    console_putu64(g_cap_state.verify_count);
    console_puts(" (success: ");
    console_putu64(g_cap_state.verify_success);
    console_puts(", failed: ");
    console_putu64(g_cap_state.verify_failed);
    console_puts(")\n");
    console_puts("[CAP-MGR-SVC]   Revoke: ");
    console_putu64(g_cap_state.revoke_count);
    console_puts("\n");
    console_puts("[CAP-MGR-SVC]   Delegate: ");
    console_putu64(g_cap_state.delegate_count);
    console_puts("\n");
    console_puts("[CAP-MGR-SVC]   Transfer: ");
    console_putu64(g_cap_state.transfer_count);
    console_puts("\n");
    console_puts("[CAP-MGR-SVC]   Derive: ");
    console_putu64(g_cap_state.derive_count);
    console_puts("\n");
    
    console_puts("[CAP-MGR-SVC] >>> Capability Manager Service CLEANED UP <<<\n");
    return HIC_SUCCESS;
}
