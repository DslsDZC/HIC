/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 模块管理服务
 * 负责模块的动态加载、卸载和管理
 */

#ifndef MODULE_MANAGER_SERVICE_H
#define MODULE_MANAGER_SERVICE_H

#include "../../Core-0/types.h"
#include "../include/module_format.h"
#include "../include/service_types.h"

/* 最大模块数量 */
#define MAX_MODULES 256

/* 模块状态 */
typedef enum {
    MODULE_STATE_UNLOADED = 0,
    MODULE_STATE_LOADED,
    MODULE_STATE_RUNNING,
    MODULE_STATE_STOPPED,
    MODULE_STATE_ERROR
} module_state_t;

/* 模块实例 */
typedef struct module_instance {
    char name[64];
    char version[16];
    u8 uuid[16];
    domain_id_t domain_id;
    u64 module_base;
    u64 module_size;
    module_state_t state;
    hicmod_header_t* header;
    hicmod_metadata_t metadata;
    u64 load_time;
    u64 capabilities[64];  /* 模块持有的能力 */
    u32 capability_count;
} module_instance_t;

/* 模块加载结果 */
typedef struct module_load_result {
    hic_status_t status;
    u64 instance_id;
    char error_message[256];
} module_load_result_t;

/* ============= 模块管理API ============= */

/**
 * 初始化模块管理器
 */
hic_status_t module_manager_init(void);

/**
 * 加载模块（需要密码验证）
 * @param module_path 模块路径
 * @param password 验证密码
 * @param result 输出结果
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_load(const char* module_path, const char* password,
                         module_load_result_t* result);

/**
 * 卸载模块（需要密码验证）
 * @param instance_id 实例ID
 * @param password 验证密码
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_unload(u64 instance_id, const char* password);

/**
 * 启动模块
 * @param instance_id 实例ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_start(u64 instance_id);

/**
 * 停止模块
 * @param instance_id 实例ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_stop(u64 instance_id);

/**
 * 列出所有模块
 * @param instances 输出模块数组
 * @param max_count 最大数量
 * @param count 输出实际数量
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_list(module_instance_t* instances, u32 max_count, u32* count);

/**
 * 获取模块信息
 * @param instance_id 实例ID
 * @param metadata 输出元数据
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_get_info(u64 instance_id, hicmod_metadata_t* metadata);

/**
 * 验证模块签名
 * @param module_path 模块路径
 * @return 验证成功返回HIC_SUCCESS
 */
hic_status_t module_verify_signature(const char* module_path);

/**
 * 解析模块依赖
 * @param module_path 模块路径
 * @param dependencies 输出依赖列表
 * @param max_count 最大数量
 * @param count 输出实际数量
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_parse_dependencies(const char* module_path,
                                       hicmod_dependency_t* dependencies,
                                       u32 max_count, u32* count);

/**
 * 检查依赖是否满足
 * @param dependencies 依赖列表
 * @param count 依赖数量
 * @return 满足返回true
 */
bool module_check_dependencies(hicmod_dependency_t* dependencies, u32 count);

/* ============= 内部函数 ============= */

/**
 * 验证密码
 * @param password 密码
 * @return 验证成功返回true
 */
bool module_verify_password(const char* password);

/**
 * 读取模块文件
 * @param path 文件路径
 * @param buffer 输出缓冲区
 * @param size 输出大小
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_read_file(const char* path, void** buffer, u64* size);

/**
 * 解析模块头部
 * @param data 模块数据
 * @param size 数据大小
 * @param header 输出头部
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_parse_header(const void* data, u64 size, hicmod_header_t* header);

/**
 * 创建模块域
 * @param metadata 模块元数据
 * @param domain_id 输出域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_create_domain(const hicmod_metadata_t* metadata, domain_id_t* domain_id);

/**
 * 分配模块资源
 * @param domain_id 域ID
 * @param metadata 模块元数据
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_allocate_resources(domain_id_t domain_id, const hicmod_metadata_t* metadata);

/**
 * 加载模块代码
 * @param instance 模块实例
 * @param data 模块数据
 * @param size 数据大小
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_load_code(module_instance_t* instance, const void* data, u64 size);

/**
 * 注册模块端点
 * @param instance 模块实例
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_register_endpoints(module_instance_t* instance);

/**
 * 销毁模块域
 * @param domain_id 域ID
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_destroy_domain(domain_id_t domain_id);

/**
 * 回收模块资源
 * @param instance 模块实例
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t module_reclaim_resources(module_instance_t* instance);

#endif /* MODULE_MANAGER_SERVICE_H */