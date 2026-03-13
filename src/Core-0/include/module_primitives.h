/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC模块原语接口
 * 
 * Core-0 提供给 Privileged-1 服务的底层原语
 * 原语是无策略的、原子化的操作，所有策略逻辑由 Privileged-1 服务实现
 */

#ifndef HIC_MODULE_PRIMITIVES_H
#define HIC_MODULE_PRIMITIVES_H

#include "../types.h"
#include "../capability.h"
#include "../domain.h"
#include "../audit.h"
#include "../formal_verification.h"

/* 模块类型 */
typedef enum module_type {
    MODULE_TYPE_CORE = 0,      /* 核心模块（内核） */
    MODULE_TYPE_PRIVILEGED,     /* 特权服务模块 */
    MODULE_TYPE_APPLICATION     /* 应用模块 */
} module_type_t;

/* 页帧类型（用于模块内存分配） */
typedef enum module_page_type {
    MODULE_PAGE_CODE = 0,      /* 代码段（可执行） */
    MODULE_PAGE_DATA,          /* 数据段（可读写） */
    MODULE_PAGE_STACK,         /* 栈（可读写） */
    MODULE_PAGE_SHARED,        /* 共享内存 */
} module_page_type_t;

/* ==================== 域管理原语 ==================== */

/**
 * @brief 创建模块域
 * 
 * @param type 模块类型
 * @param quota 资源配额
 * @param domain_id 输出域ID
 * @return 状态码
 */
hic_status_t module_domain_create(module_type_t type,
                                    const domain_quota_t* quota,
                                    domain_id_t* domain_id);

/**
 * @brief 销毁模块域
 */
hic_status_t module_domain_destroy(domain_id_t domain_id);

/**
 * @brief 暂停模块域
 */
hic_status_t module_domain_suspend(domain_id_t domain_id);

/**
 * @brief 恢复模块域
 */
hic_status_t module_domain_resume(domain_id_t domain_id);

/**
 * @brief 获取域状态
 */
hic_status_t module_domain_get_state(domain_id_t domain_id,
                                    domain_state_t* state);

/**
 * @brief 发送域异常信号
 */
hic_status_t module_domain_signal(domain_id_t domain_id,
                                    u32 exception_code,
                                    const char* exception_info);

/**
 * @brief 获取域资源配额
 */
hic_status_t module_domain_get_quota(domain_id_t domain_id,
                                    domain_quota_t* quota);

/* ==================== 内存管理原语 ==================== */

/**
 * @brief 为模块分配内存
 */
hic_status_t module_memory_alloc(domain_id_t domain_id,
                                  u64 size,
                                  module_page_type_t type,
                                  u64* phys_addr);

/**
 * @brief 释放模块内存
 */
hic_status_t module_memory_free(domain_id_t domain_id,
                                  u64 phys_addr,
                                  u64 size);

/**
 * @brief 映射内存到域地址空间
 */
hic_status_t module_memory_map(domain_id_t domain_id,
                                u64 phys_addr,
                                u64 size,
                                u64* virt_addr);

/**
 * @brief 取消内存映射
 */
hic_status_t module_memory_unmap(domain_id_t domain_id,
                                  u64 virt_addr,
                                  u64 size);

/* ==================== 能力管理原语 ==================== */

/**
 * @brief 为模块创建能力
 */
hic_status_t module_cap_create(domain_id_t domain_id,
                                u32 cap_type,
                                cap_rights_t rights,
                                cap_id_t* cap_id);

/**
 * @brief 撤销能力
 */
hic_status_t module_cap_revoke(cap_id_t cap_id);

/**
 * @brief 派生能力
 */
hic_status_t module_cap_derive(cap_id_t parent_cap,
                                cap_rights_t sub_rights,
                                cap_id_t* child_cap);

/**
 * @brief 传递能力到目标域
 */
hic_status_t module_cap_transfer(cap_id_t cap_id,
                                  domain_id_t target_domain);

/**
 * @brief 授予能力给域
 */
hic_status_t module_cap_grant(domain_id_t domain_id,
                               cap_id_t cap_id,
                               cap_handle_t* handle);

/**
 * @brief 检查能力权限
 */
hic_status_t module_cap_check(cap_id_t cap_id,
                               cap_rights_t required_rights);

/* ==================== 端点管理原语 ==================== */

/**
 * @brief 创建服务端点
 */
hic_status_t module_endpoint_create(domain_id_t domain_id,
                                     const char* name,
                                     cap_id_t* endpoint_cap);

/**
 * @brief 注册服务端点
 */
hic_status_t module_endpoint_register(domain_id_t domain_id,
                                       cap_id_t endpoint_cap,
                                       const char* name);

/**
 * @brief 查找服务端点
 */
hic_status_t module_endpoint_lookup(const char* name,
                                     cap_id_t* endpoint_cap,
                                     domain_id_t* owner_domain);

/* ==================== 审计原语 ==================== */

/**
 * @brief 记录模块审计事件
 */
hic_status_t module_audit_log(u32 event_type,
                               domain_id_t domain_id,
                               const u64* data,
                               u32 data_count);

/* ==================== 初始化 ==================== */

/**
 * @brief 初始化模块原语系统
 */
void module_primitives_init(void);

#endif /* HIC_MODULE_PRIMITIVES_H */