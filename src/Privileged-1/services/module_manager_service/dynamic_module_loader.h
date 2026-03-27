/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC动态模块加载器 (Privileged-1版本)
 * 
 * 从引导分区的 modules.list 文件加载模块
 * 此模块运行在 Privileged-1 层，通过 Core-0 提供的原语接口操作
 * 
 * 流程：
 * 1. 通过 fat32_service 端点读取 /modules.list
 * 2. 解析每行作为一个模块文件名
 * 3. 对于每个模块：
 *    a. 通过 fat32_service 读取模块文件
 *    b. 通过 module_signer 端点验证签名
 *    c. 使用 Core-0 模块原语创建沙箱
 *    d. 加载模块代码和数据
 *    e. 启动模块
 */

#ifndef HIC_DYNAMIC_MODULE_LOADER_H
#define HIC_DYNAMIC_MODULE_LOADER_H

#include <common.h>
#include <module_types.h>

/* modules.list 最大行数 */
#define MAX_MODULES_LIST_ENTRIES 64
/* 每个模块最大依赖数 */
#define MAX_MODULE_DEPENDENCIES 8

/* 模块加载状态 */
typedef enum {
    DLOAD_PENDING,      /* 等待加载 */
    DLOAD_READING,      /* 正在读取 */
    DLOAD_VERIFYING,    /* 正在验证 */
    DLOAD_CREATING,     /* 正在创建沙箱 */
    DLOAD_LOADING,      /* 正在加载 */
    DLOAD_STARTING,     /* 正在启动 */
    DLOAD_RUNNING,      /* 已运行 */
    DLOAD_FAILED,       /* 加载失败 */
} dynamic_load_state_t;

/* 模块加载条目 */
typedef struct {
    char name[64];              /* 模块名称（来自 modules.list） */
    char path[256];             /* 模块路径 */
    char dependencies[MAX_MODULE_DEPENDENCIES][64]; /* 依赖列表 */
    u8 dep_count;               /* 依赖数量 */
    u8 dep_satisfied;           /* 已满足的依赖数 */
    dynamic_load_state_t state; /* 当前状态 */
    u32 retry_count;            /* 重试次数 */
    u32 domain;                 /* 分配的域 */
    u32 endpoint;               /* 端点能力 */
    u64 entry_point;            /* 入口点地址 (ELF 解析获得) */
    u64 code_base;              /* 代码段基址 */
    u64 code_size;              /* 代码段大小 */
    u64 data_base;              /* 数据段基址（W^X 保护） */
    u64 data_size;              /* 数据段大小 */
    hic_status_t last_error;    /* 最后错误 */
} dynamic_module_entry_t;

/* 动态加载上下文 */
typedef struct {
    dynamic_module_entry_t entries[MAX_MODULES_LIST_ENTRIES];
    u32 entry_count;
    u32 loaded_count;
    u32 failed_count;
} dynamic_load_ctx_t;

/* ==================== 初始化 ==================== */

/**
 * 初始化动态模块加载器
 */
void dynamic_module_loader_init(void);

/* ==================== 主加载流程 ==================== */

/**
 * 从 modules.list 加载所有模块
 * 
 * @return 成功加载的模块数量
 */
int dynamic_module_load_all(void);

/**
 * 加载单个模块
 * 
 * @param module_name 模块名称
 * @param path 模块路径
 * @return 状态码
 */
hic_status_t dynamic_module_load(const char *module_name, const char *path);

/* ==================== 流程步骤 ==================== */

/**
 * 读取 modules.list 文件
 * 
 * @param entries 输出条目数组
 * @param max_entries 最大条目数
 * @return 实际读取的条目数
 */
int dynamic_read_modules_list(dynamic_module_entry_t *entries, u32 max_entries);

/**
 * 读取模块文件
 * 
 * @param path 模块路径
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @param bytes_read 输出读取字节数
 * @return 状态码
 */
hic_status_t dynamic_read_module_file(const char *path, void *buffer, 
                                       u32 buffer_size, u32 *bytes_read);

/**
 * 验证模块签名
 * 
 * @param module_data 模块数据
 * @param module_size 模块大小
 * @return 验证成功返回 HIC_SUCCESS
 */
hic_status_t dynamic_verify_module(const void *module_data, u32 module_size);

/**
 * 创建模块沙箱
 * 
 * @param module_data 模块数据
 * @param module_size 模块大小
 * @param entry 输出加载条目
 * @return 状态码
 */
hic_status_t dynamic_create_sandbox(const void *module_data, u32 module_size,
                                     dynamic_module_entry_t *entry);

/**
 * 启动模块
 * 
 * @param entry 加载条目
 * @return 状态码
 */
hic_status_t dynamic_start_module(dynamic_module_entry_t *entry);

/* ==================== 查询 ==================== */

/**
 * 获取加载上下文
 */
dynamic_load_ctx_t* dynamic_get_load_context(void);

/**
 * 获取模块加载状态
 */
dynamic_load_state_t dynamic_get_module_state(const char *name);

#endif /* HIC_DYNAMIC_MODULE_LOADER_H */
