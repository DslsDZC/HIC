/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 兼容层加载器 - 服务侧接口
 * 
 * 统一管理四个兼容层共享库（CHAL, AAL, IMAL, PFL）的加载。
 * 
 * 设计原则：
 * 1. 有 MMU 时：通过共享库机制，多服务共享同一物理代码段
 * 2. 无 MMU 时：静态链接，兼容层代码编译到服务中
 * 
 * 使用流程：
 * 1. 服务启动时调用 compatibility_layer_init()
 * 2. 获取各兼容层的函数指针
 * 3. 直接调用（零开销）
 */

#ifndef HIC_COMPATIBILITY_LAYER_H
#define HIC_COMPATIBILITY_LAYER_H

#include "stdint.h"
#include "stdbool.h"
#include "hiclib.h"

/* ==================== 兼容层类型 ==================== */

typedef enum {
    COMPAT_LAYER_CHAL = 0,   /* 核心硬件抽象层 */
    COMPAT_LAYER_AAL,        /* 架构适配层 */
    COMPAT_LAYER_IMAL,       /* 隔离机制抽象层 */
    COMPAT_LAYER_PFL,        /* 平台特性层 */
    COMPAT_LAYER_MAX
} compat_layer_type_t;

/* ==================== 兼容层信息 ==================== */

typedef struct {
    const char *name;           /* 库名称 */
    const uint8_t *uuid;        /* 库 UUID */
    uint32_t version;           /* 版本号 */
    bool loaded;                /* 是否已加载 */
    uint64_t code_cap;          /* 代码段能力句柄 */
    uint64_t rodata_cap;        /* 只读数据段能力句柄 */
    void *base_addr;            /* 映射基址 */
} compat_layer_info_t;

/* ==================== 错误码 ==================== */

typedef enum {
    COMPAT_OK = 0,
    COMPAT_ERR_NOT_FOUND = 1,
    COMPAT_ERR_VERSION_MISMATCH = 2,
    COMPAT_ERR_NO_MEMORY = 3,
    COMPAT_ERR_PERMISSION = 4,
    COMPAT_ERR_DEPENDENCY = 5,
    COMPAT_ERR_ALREADY_LOADED = 6,
} compat_error_t;

/* ==================== 初始化接口 ==================== */

/**
 * 初始化兼容层
 * 
 * 加载所有必需的兼容层共享库。
 * 在服务启动时调用一次。
 * 
 * @return 错误码
 */
compat_error_t compatibility_layer_init(void);

/**
 * 关闭兼容层
 * 
 * 卸载所有兼容层共享库。
 * 在服务退出时调用。
 */
void compatibility_layer_shutdown(void);

/**
 * 检查兼容层是否已初始化
 */
bool compatibility_layer_is_initialized(void);

/* ==================== 单层加载接口 ==================== */

/**
 * 加载单个兼容层
 * @param type 兼容层类型
 * @return 错误码
 */
compat_error_t compatibility_layer_load(compat_layer_type_t type);

/**
 * 卸载单个兼容层
 * @param type 兼容层类型
 */
void compatibility_layer_unload(compat_layer_type_t type);

/**
 * 获取兼容层信息
 * @param type 兼容层类型
 * @param info 输出信息
 * @return 错误码
 */
compat_error_t compatibility_layer_get_info(compat_layer_type_t type, 
                                             compat_layer_info_t *info);

/* ==================== 符号查询接口 ==================== */

/**
 * 查询兼容层符号
 * @param type 兼容层类型
 * @param symbol_name 符号名称
 * @return 符号地址，未找到返回 NULL
 */
void *compatibility_layer_get_symbol(compat_layer_type_t type, 
                                      const char *symbol_name);

/* ==================== 依赖管理 ==================== */

/**
 * 兼容层依赖关系
 * 
 * CHAL <- AAL <- IMAL <- PFL
 * 
 * CHAL: 无依赖（最底层）
 * AAL: 依赖 CHAL
 * IMAL: 依赖 AAL, CHAL
 * PFL: 依赖 IMAL, AAL, CHAL
 */

/* 依赖标志 */
#define COMPAT_DEP_CHAL   (1U << COMPAT_LAYER_CHAL)
#define COMPAT_DEP_AAL    (1U << COMPAT_LAYER_AAL)
#define COMPAT_DEP_IMAL   (1U << COMPAT_LAYER_IMAL)
#define COMPAT_DEP_PFL    (1U << COMPAT_LAYER_PFL)

/**
 * 获取兼容层的依赖掩码
 * @param type 兼容层类型
 * @return 依赖掩码
 */
uint32_t compatibility_layer_get_dependencies(compat_layer_type_t type);

/* ==================== 版本检查 ==================== */

/**
 * 检查版本兼容性
 * @param type 兼容层类型
 * @param min_version 最低版本
 * @return true 如果版本满足要求
 */
bool compatibility_layer_check_version(compat_layer_type_t type, 
                                        uint32_t min_version);

/* ==================== 统计信息 ==================== */

typedef struct {
    uint32_t layers_loaded;     /* 已加载数量 */
    uint32_t total_refs;        /* 总引用数 */
    uint64_t total_code_size;   /* 总代码大小 */
    uint64_t total_rodata_size; /* 总只读数据大小 */
} compat_stats_t;

/**
 * 获取统计信息
 */
compat_error_t compatibility_layer_get_stats(compat_stats_t *stats);

/* ==================== 便捷宏 ==================== */

/* 获取兼容层函数指针 */
#define COMPAT_GET_FUNC(layer, func_name) \
    compatibility_layer_get_symbol(layer, #func_name)

/* 检查兼容层是否已加载 */
#define COMPAT_IS_LOADED(layer) \
    ({ compat_layer_info_t _info; \
       compatibility_layer_get_info(layer, &_info) == COMPAT_OK && _info.loaded; })

/* ==================== 内置库标识 ==================== */

/*
 * 内置兼容层库（由内核导出）
 * 
 * 这些库在内核启动时自动注册到 lib_manager，
 * 服务无需提供库文件，只需通过 lib_lookup 获取能力。
 */

/* 内置库标志 */
#define HICLIB_FLAG_BUILTIN     (1U << 0)   /* 内置库 */
#define HICLIB_FLAG_CORE        (1U << 1)   /* 核心库 */

/* 内置库 UUID 前缀 */
#define HICLIB_BUILTIN_PREFIX   0x8000000000000000ULL

/* 检查是否为内置库 */
static inline bool hiclib_is_builtin(const uint8_t uuid[16]) {
    return (uuid[8] & 0x80) != 0;
}

#endif /* HIC_COMPATIBILITY_LAYER_H */
