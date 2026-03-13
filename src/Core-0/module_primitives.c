/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC模块原语实现
 * 
 * Core-0 提供给 Privileged-1 服务的底层原语实现
 */

#include "include/module_primitives.h"
#include "include/service_registry.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "atomic.h"
#include "console.h"
#include "lib/string.h"
#include "lib/mem.h"

/* ==================== 初始化 ==================== */

void module_primitives_init(void)
{
    console_puts("[PRIMITIVES] Module primitives initialized\n");
}

/* ==================== 域管理原语 ==================== */

hic_status_t module_domain_create(module_type_t type,
                                    const domain_quota_t* quota,
                                    domain_id_t* domain_id)
{
    domain_type_t domain_type;
    
    if (!quota || !domain_id) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 转换模块类型到域类型 */
    switch (type) {
        case MODULE_TYPE_CORE:
            domain_type = DOMAIN_TYPE_CORE;
            break;
        case MODULE_TYPE_PRIVILEGED:
            domain_type = DOMAIN_TYPE_PRIVILEGED;
            break;
        case MODULE_TYPE_APPLICATION:
            domain_type = DOMAIN_TYPE_APPLICATION;
            break;
        default:
            return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 调用域系统创建域 */
    return domain_create(domain_type, HIC_INVALID_DOMAIN, quota, domain_id);
}

hic_status_t module_domain_destroy(domain_id_t domain_id)
{
    return domain_destroy(domain_id);
}

hic_status_t module_domain_suspend(domain_id_t domain_id)
{
    return domain_suspend(domain_id);
}

hic_status_t module_domain_resume(domain_id_t domain_id)
{
    return domain_resume(domain_id);
}

hic_status_t module_domain_get_state(domain_id_t domain_id,
                                      domain_state_t* state)
{
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status == HIC_SUCCESS && state) {
        *state = info.state;
    }
    return status;
}

hic_status_t module_domain_signal(domain_id_t domain_id,
                                    u32 exception_code,
                                    const char* exception_info)
{
    /* TODO: 实现域信号发送 */
    return HIC_SUCCESS;
}

hic_status_t module_domain_get_quota(domain_id_t domain_id,
                                      domain_quota_t* quota)
{
    domain_t info;
    hic_status_t status = domain_get_info(domain_id, &info);
    if (status == HIC_SUCCESS && quota) {
        *quota = info.quota;
    }
    return status;
}

/* ==================== 内存管理原语 ==================== */

hic_status_t module_memory_alloc(domain_id_t domain_id,
                                   u64 size,
                                   module_page_type_t type,
                                   u64* phys_addr)
{
    u32 page_type;
    
    if (!phys_addr || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 转换页类型 */
    switch (type) {
        case MODULE_PAGE_CODE:
        case MODULE_PAGE_DATA:
        case MODULE_PAGE_STACK:
            page_type = PAGE_FRAME_PRIVILEGED;
            break;
        case MODULE_PAGE_SHARED:
            page_type = PAGE_FRAME_SHARED;
            break;
        default:
            page_type = PAGE_FRAME_PRIVILEGED;
    }
    
    /* 对齐大小到页边界 */
    u32 page_count = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    
    return pmm_alloc_frames(domain_id, page_count, page_type, phys_addr);
}

hic_status_t module_memory_free(domain_id_t domain_id,
                                  u64 phys_addr,
                                  u64 size)
{
    u32 page_count = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    pmm_free_frames(phys_addr, page_count);
    return HIC_SUCCESS;
}

hic_status_t module_memory_map(domain_id_t domain_id,
                                u64 phys_addr,
                                u64 size,
                                u64* virt_addr)
{
    /* TODO: 实现页表映射 */
    if (virt_addr) {
        *virt_addr = phys_addr;  /* 恒等映射 */
    }
    return HIC_SUCCESS;
}

hic_status_t module_memory_unmap(domain_id_t domain_id,
                                  u64 virt_addr,
                                  u64 size)
{
    /* TODO: 实现页表取消映射 */
    return HIC_SUCCESS;
}

/* ==================== 能力管理原语 ==================== */

hic_status_t module_cap_create(domain_id_t domain_id,
                                cap_type_t type,
                                cap_rights_t rights,
                                cap_id_t* cap_id)
{
    if (!cap_id) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 根据能力类型创建能力 */
    if (type == CAP_MEMORY) {
        /* 创建空内存能力（稍后填充） */
        return cap_create_memory(domain_id, 0, PAGE_SIZE, rights, cap_id);
    }
    
    /* 默认创建端点能力 */
    return cap_create_endpoint(domain_id, domain_id, cap_id);
}

hic_status_t module_cap_revoke(cap_id_t cap_id)
{
    return cap_revoke(cap_id);
}

hic_status_t module_cap_derive(cap_id_t parent_cap,
                                cap_rights_t sub_rights,
                                cap_id_t* child_cap)
{
    if (!child_cap) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 使用当前域作为拥有者 */
    domain_id_t owner = HIC_DOMAIN_CORE;  /* 简化实现 */
    return cap_derive(owner, parent_cap, sub_rights, child_cap);
}

hic_status_t module_cap_transfer(cap_id_t cap_id,
                                  domain_id_t target_domain)
{
    /* 简化实现：直接改变拥有者 */
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    g_global_cap_table[cap_id].owner = target_domain;
    return HIC_SUCCESS;
}

hic_status_t module_cap_grant(domain_id_t domain_id,
                               cap_id_t cap_id,
                               cap_handle_t* handle)
{
    return cap_grant(domain_id, cap_id, handle);
}

hic_status_t module_cap_check(cap_id_t cap_id,
                               cap_rights_t required_rights)
{
    if (cap_id >= CAP_TABLE_SIZE) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    cap_entry_t *entry = &g_global_cap_table[cap_id];
    
    if (entry->cap_id != cap_id || (entry->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    if ((entry->rights & required_rights) != required_rights) {
        return HIC_ERROR_PERMISSION;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 端点管理原语 ==================== */

hic_status_t module_endpoint_create(domain_id_t domain_id,
                                     const char* name,
                                     cap_id_t* endpoint_cap)
{
    if (!name || !endpoint_cap) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 创建端点能力 */
    return cap_create_endpoint(domain_id, domain_id, endpoint_cap);
}

hic_status_t module_endpoint_register(domain_id_t domain_id,
                                       cap_id_t endpoint_cap,
                                       const char* name)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 在服务注册表中注册 */
    service_endpoint_t endpoint;
    memzero(&endpoint, sizeof(endpoint));
    
    strncpy(endpoint.name, name, sizeof(endpoint.name) - 1);
    endpoint.owner = domain_id;
    endpoint.endpoint_cap = endpoint_cap;
    endpoint.type = ENDPOINT_TYPE_GENERIC;
    endpoint.state = SERVICE_STATE_RUNNING;
    endpoint.version = 1;
    
    return service_register_endpoint(&endpoint);
}

hic_status_t module_endpoint_lookup(const char* name,
                                     cap_id_t* endpoint_cap,
                                     domain_id_t* owner_domain)
{
    if (!name) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    service_endpoint_t* ep = service_find_by_name(name);
    if (!ep) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    if (endpoint_cap) {
        *endpoint_cap = ep->endpoint_cap;
    }
    if (owner_domain) {
        *owner_domain = ep->owner;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 审计原语 ==================== */

hic_status_t module_audit_log(u32 event_type,
                               domain_id_t domain_id,
                               const u64* data,
                               u32 data_count)
{
    return audit_log_event(event_type, domain_id, 0, 0, data, data_count, true);
}
