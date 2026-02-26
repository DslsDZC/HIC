/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块加载器
 * 支持静态链接.hicmod格式的驱动模块
 */

#ifndef HIC_MODULE_LOADER_H
#define HIC_MODULE_LOADER_H

#include <stdint.h>
#include "types.h"
#include "capability.h"
#include "hardware_probe.h"

/* 模块格式定义 */
#define HICMOD_MAGIC 0x48494B4D  // "HICM"
#define HICMOD_VERSION 1

/* 模块头 */
typedef struct hicmod_header {
    u32 magic;                  // 魔数
    u32 version;                // 格式版本
    u8 uuid[16];                // 模块UUID
    u32 semantic_version;       // 语义化版本
    u32 api_descriptor_offset;  // API描述符偏移
    u32 code_size;              // 代码段大小
    u32 data_size;              // 数据段大小
    u32 signature_offset;       // 签名偏移
    u32 header_size;            // 头部大小
} hicmod_header_t;

/* 模块元数据 */
typedef struct hicmod_metadata {
    char name[64];              // 模块名称
    char description[256];      // 描述
    char author[64];            // 作者
    u32 endpoint_count;         // 导出端点数量
    u32 resource_count;         // 资源需求数量
    u32 dependency_count;       // 依赖数量
} hicmod_metadata_t;

/* 端点描述符 */
typedef struct endpoint_descriptor {
    char name[64];              // 端点名称
    u32 endpoint_id;            // 端点ID
    u32 version;                // API版本
    u64 entry_point;            // 入口点地址
} endpoint_descriptor_t;

/* 资源需求 */
typedef struct resource_requirement {
    u32 type;                   // 资源类型
    u64 size;                   // 大小
    u32 flags;                  // 标志
} resource_requirement_t;

/* 依赖项 */
typedef struct dependency {
    u8 uuid[16];                // 依赖模块UUID
    u32 min_version;            // 最小版本
    u32 max_version;            // 最大版本
} dependency_t;

/* 模块实例 */
typedef struct hicmod_instance {
    u64 instance_id;            // 实例ID
    domain_id_t domain;         // 所属域
    u8 uuid[16];                // 模块UUID
    u32 version;                // 版本
    u64 code_base;              // 代码基地址
    u64 data_base;              // 数据基地址
    u64 entry_point;            // 入口点
    cap_id_t* capabilities;     // 能力列表
    u32 cap_count;              // 能力数量
    u8 state;                   // 状态
} hicmod_instance_t;

/* 模块加载器状态 */
typedef struct module_loader {
    hicmod_instance_t instances[256];  // 实例表
    u32 instance_count;                // 实例数量
    u64 next_instance_id;              // 下一个实例ID
} module_loader_t;

/* 状态定义 */
#define MODULE_STATE_LOADED    0
#define MODULE_STATE_RUNNING   1
#define MODULE_STATE_PAUSED    2
#define MODULE_STATE_STOPPED   3

/* 外部API声明 */

/**
 * 初始化模块加载器
 */
void module_loader_init(void);

/**
 * 从文件加载模块
 * 
 * 参数：
 *   path - 模块文件路径
 *   instance_id - 返回实例ID
 * 
 * 返回值：成功返回0，失败返回错误码
 */
int module_load_from_file(const char* path, u64* instance_id);

/**
 * 从内存加载模块
 * 
 * 参数：
 *   data - 模块数据
 *   size - 数据大小
 *   instance_id - 返回实例ID
 * 
 * 返回值：成功返回0，失败返回错误码
 */
int module_load_from_memory(const void* data, u64 size, u64* instance_id);

/**
 * 卸载模块
 * 
 * 参数：
 *   instance_id - 实例ID
 * 
 * 返回值：成功返回0，失败返回错误码
 */
int module_unload(u64 instance_id);

/**
 * 启动模块实例
 * 
 * 参数：
 *   instance_id - 实例ID
 * 
 * 返回值：成功返回0，失败返回错误码
 */
int module_start(u64 instance_id);

/**
 * 停止模块实例
 * 
 * 参数：
 *   instance_id - 实例ID
 * 
 * 返回值：成功返回0，失败返回错误码
 */
int module_stop(u64 instance_id);

/**
 * 自动加载硬件驱动
 * 根据探测到的硬件信息自动加载对应的驱动模块
 * 
 * 参数：
 *   devices - 设备列表
 * 
 * 返回值：成功加载数量
 */
int module_auto_load_drivers(device_list_t* devices);

/**
 * 验证模块签名
 * 
 * 参数：
 *   header - 模块头
 *   signature - 签名数据
 *   signature_size - 签名大小
 * 
 * 返回值：验证通过返回true
 */
bool module_verify_signature(const hicmod_header_t* header, 
                            const void* signature, 
                            u32 signature_size);

/**
 * 解析模块依赖
 * 
 * 参数：
 *   instance - 模块实例
 * 
 * 返回值：依赖解析成功返回true
 */
bool module_resolve_dependencies(hicmod_instance_t* instance);

/**
 * 分配模块资源
 * 
 * 参数：
 *   instance - 模块实例
 *   resources - 资源需求
 *   count - 资源数量
 * 
 * 返回值：分配成功返回true
 */
bool module_allocate_resources(hicmod_instance_t* instance,
                              const resource_requirement_t* resources,
                              u32 count);

/**
 * 注册模块端点
 * 
 * 参数：
 *   instance - 模块实例
 *   endpoints - 端点描述符
 *   count - 端点数量
 * 
 * 返回值：注册成功返回true
 */
bool module_register_endpoints(hicmod_instance_t* instance,
                              const endpoint_descriptor_t* endpoints,
                              u32 count);

/**
 * 获取模块实例
 * 
 * 参数：
 *   instance_id - 实例ID
 * 
 * 返回值：实例指针，不存在返回NULL
 */
hicmod_instance_t* module_get_instance(u64 instance_id);

/**
 * 查找模块实例（通过UUID）
 * 
 * 参数：
 *   uuid - 模块UUID
 * 
 * 返回值：实例指针，不存在返回NULL
 */
hicmod_instance_t* module_find_instance_by_uuid(const u8 uuid[16]);

/**
 * 枚举所有模块实例
 * 
 * 参数：
 *   callback - 回调函数
 *   context - 上下文
 */
void module_enumerate_instances(void (*callback)(hicmod_instance_t*, void*),
                                void* context);

#endif /* MODULE_LOADER_H */