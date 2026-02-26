/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 模块系统底层原语接口
 * 
 * 这些是Core-0提供的底层原语，用于Privileged-1服务实现高层模块管理功能。
 * Core-0只提供原子化的、无策略的执行原语，所有策略逻辑由Privileged-1服务实现。
 */

#ifndef HIC_MODULE_PRIMITIVES_H
#define HIC_MODULE_PRIMITIVES_H

#include "types.h"

/* 模块类型 */
typedef enum module_type {
    MODULE_TYPE_CORE = 0,      /* 核心模块（内核） */
    MODULE_TYPE_PRIVILEGED,     /* 特权服务模块 */
    MODULE_TYPE_APPLICATION     /* 应用模块 */
} module_type_t;

/* 页帧类型（用于模块内存分配） */
typedef enum module_page_type {
    MODULE_PAGE_CODE = 0,      /* 代码段（可执行、只读） */
    MODULE_PAGE_DATA,          /* 数据段（可读写） */
    MODULE_PAGE_RODATA,        /* 只读数据段 */
    MODULE_PAGE_BSS,           /* BSS段（零初始化数据） */
    MODULE_PAGE_SHARED,        /* 共享内存 */
    MODULE_PAGE_CONFIG         /* 配置数据 */
} module_page_type_t;

/* 模块段描述符（底层原语） */
typedef struct module_segment {
    u64 file_offset;       /* 文件中的偏移 */
    u64 memory_offset;     /* 内存中的偏移 */
    u64 file_size;         /* 文件大小 */
    u64 memory_size;       /* 内存大小 */
    module_page_type_t type;
    u32 flags;
} module_segment_t;

/* 模块创建参数（底层原语） */
typedef struct module_create_params {
    module_type_t type;         /* 模块类型 */
    domain_id_t domain_id;       /* 域ID（由Core-0分配） */
    u64 module_base;            /* 模块基地址 */
    u64 module_size;            /* 模块大小 */
    module_segment_t* segments;  /* 段描述符数组 */
    u32 segment_count;          /* 段数量 */
    cap_id_t* initial_caps;      /* 初始能力数组 */
    u32 cap_count;              /* 初始能力数量 */
    void* entry_point;           /* 入口点 */
} module_create_params_t;

/* ============= Core-0底层原语 ============= */

/**
 * @brief 创建模块域（底层原语）
 * 
 * 这是Core-0提供的底层原语，用于创建模块运行域。
 * Privileged-1服务负责调用此原语来创建模块域。
 * 
 * @param type 模块类型
 * @param quota 资源配额
 * @param domain_id 输出域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_create(module_type_t type, 
                                    const domain_quota_t* quota,
                                    domain_id_t* domain_id);

/**
 * @brief 销毁模块域（底层原语）
 * 
 * 销毁模块域并回收所有资源。
 * 
 * @param domain_id 域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_destroy(domain_id_t domain_id);

/**
 * @brief 分配模块内存（底层原语）
 * 
 * 为模块分配物理内存，直接映射到模块域。
 * 
 * @param domain_id 域ID
 * @param size 内存大小
 * @param type 页类型
 * @param phys_addr 输出物理地址
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_memory_alloc(domain_id_t domain_id,
                                  u64 size,
                                  module_page_type_t type,
                                  u64* phys_addr);

/**
 * @brief 释放模块内存（底层原语）
 * 
 * @param domain_id 域ID
 * @param phys_addr 物理地址
 * @param size 内存大小
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_memory_free(domain_id_t domain_id,
                                 u64 phys_addr,
                                 u64 size);

/**
 * @brief 创建模块能力（底层原语）
 * 
 * 为模块创建能力。
 * 
 * @param domain_id 域ID
 * @param type 能力类型
 * @param resource 资源描述
 * @param cap_id 输出能力ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_cap_create(domain_id_t domain_id,
                                cap_type_t type,
                                cap_rights_t rights,
                                void* resource,
                                cap_id_t* cap_id);

/**
 * @brief 创建IPC端点能力（底层原语）
 * 
 * 为模块创建IPC端点能力。
 * 
 * @param domain_id 域ID
 * @param endpoint_name 端点名称
 * @param cap_id 输出能力ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_endpoint_create(domain_id_t domain_id,
                                     const char* endpoint_name,
                                     cap_id_t* cap_id);

/**
 * @brief 注册模块端点（底层原语）
 * 
 * 将模块的IPC端点注册到Core-0系统服务注册表。
 * 
 * @param domain_id 域ID
 * @param endpoint_id 端点能力ID
 * @param endpoint_name 端点名称
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_endpoint_register(domain_id_t domain_id,
                                       cap_id_t endpoint_id,
                                       const char* endpoint_name);

/**
 * @brief 模块域切换（底层原语）
 * 
 * 切换到模块域执行。
 * 
 * @param domain_id 域ID
 * @param context 上下文参数
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_switch(domain_id_t domain_id,
                                  void* context);

/**
 * @brief 暂停模块域（底层原语）
 * 
 * @param domain_id 域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_suspend(domain_id_t domain_id);

/**
 * @brief 恢复模块域（底层原语）
 * 
 * @param domain_id 域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_resume(domain_id domain_id);

/**
 * @brief 获取模块状态（底层原语）
 * 
 * @param domain_id 域ID
 * @param state 输出状态
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_get_state(domain_id domain_id,
                                    domain_state_t* state);

/**
 * @brief 发送域异常信号（底层原语）
 * 
 * 向模块域发送异常信号。
 * 
 * @param domain_id 域ID
 * @param exception_code 异常代码
 * @param exception_info 异常信息
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_signal(domain_id_t domain_id,
                                    u32 exception_code,
                                    const char* exception_info);

/**
 * @brief 获取域资源使用统计（底层原语）
 * 
 * @param domain_id 域ID
 * @param usage 输出使用统计
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_get_usage(domain_id_t domain_id,
                                    domain_quota_t* quota);

/**
 * @brief 验证域能力（底层原语）
 * 
 * 验证域是否拥有特定能力。
 * 
 * @param domain_id 域ID
 * @param cap_id 能力ID
 * @param required_rights 所需权限
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_domain_check_cap(domain_id_t domain_id,
                                   cap_id_t cap_id,
                                   cap_rights_t required_rights);

/**
 * @brief 审计日志记录（底层原语）
 * 
 * 记录模块相关的审计日志。
 * 
 * @param domain_id 域ID
 * @param event_type 事件类型
 * @param event_data 事件数据
 * @param data_len 数据长度
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_audit_log(domain_id_t domain_id,
                               u32 event_type,
                               const void* event_data,
                               u32 data_len);

/* ============= 审计事件类型 ============= */

#define AUDIT_EVENT_MODULE_LOAD     0x1001
#define AUDIT_EVENT_MODULE_UNLOAD   0x1002
#define MODULE_EVENT_MODULE_START    0x1003
#define MODULE_EVENT_MODULE_STOP     0x1004
#define AUDIT_EVENT_MODULE_ERROR    0x1005
#define AUDIT_EVENT_CAP_CREATE     0x2001
#define AUDIT_EVENT_CAP_TRANSFER   0x2002
#define AUDIT_EVENT_CAP_REVOKE     0x2003

#endif /* HIC_MODULE_PRIMITIVES_H */