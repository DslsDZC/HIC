/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统
 * 静态链接到内核中的服务模块
 * 
 * 加载流程：
 * 1. Core-0 解析内核映像的 .static_modules 段
 * 2. 为每个内嵌模块分配沙箱内存
 * 3. 复制代码段、数据段到沙箱
 * 4. 配置页表/MPU，设置内存权限
 * 5. 创建初始能力空间
 * 6. 注册服务端点
 * 7. 启动模块主线程（绑定到逻辑核心）
 */

#ifndef STATIC_MODULE_H
#define STATIC_MODULE_H

#include "../types.h"
#include "../capability.h"

/* 静态模块描述符 */
typedef struct static_module_desc {
    char     name[32];           /* 服务名称 */
    u32      type;               /* 服务类型 */
    u32      version;            /* 版本号 */
    void    *code_start;         /* 代码起始地址 */
    void    *code_end;           /* 代码结束地址 */
    void    *data_start;         /* 数据起始地址 */
    void    *data_end;           /* 数据结束地址 */
    u64      entry_offset;       /* 入口点偏移（相对于代码起始） */
    u64      capabilities[8];    /* 需要授予的初始能力 */
    u64      flags;              /* 标志位 */
} static_module_desc_t;

/* 模块类型 */
#define STATIC_MODULE_TYPE_DRIVER      1
#define STATIC_MODULE_TYPE_SERVICE     2
#define STATIC_MODULE_TYPE_SYSTEM      3
#define STATIC_MODULE_TYPE_USER        4

/* 模块标志 */
#define STATIC_MODULE_FLAG_CRITICAL    (1ULL << 0)  /* 关键服务，必须启动 */
#define STATIC_MODULE_FLAG_AUTO_START  (1ULL << 1)  /* 自动启动 */
#define STATIC_MODULE_FLAG_PRIVILEGED  (1ULL << 2)  /* 特权服务 */

/* 外部符号声明（链接脚本定义） */
extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/* ==================== 初始化 ==================== */

/**
 * 初始化静态模块系统
 */
void static_module_system_init(void);

/* ==================== 加载流程 ==================== */

/**
 * 加载所有静态模块
 * 
 * 执行完整加载流程：
 * 1. 创建沙箱
 * 2. 设置能力
 * 3. 注册服务
 * 4. 启动模块
 * 
 * @return 成功加载的模块数量
 */
int static_module_load_all(void);

/* ==================== 扩展接口（新） ==================== */

/**
 * 创建静态模块的沙箱（扩展版本）
 * - 分配物理内存
 * - 配置页表/MPU
 * - 复制代码段和数据段
 * 
 * @param module 模块描述符
 * @param runtime_idx 运行时索引
 * @return 成功返回 0
 */
int static_module_create_sandbox_ex(static_module_desc_t *module, u32 runtime_idx);

/**
 * 为模块创建初始能力空间
 * - 创建端点能力
 * - 授予请求的能力
 * 
 * @param module 模块描述符
 * @param runtime_idx 运行时索引
 * @return 成功返回 0
 */
int static_module_setup_capabilities(static_module_desc_t *module, u32 runtime_idx);

/**
 * 注册模块的服务端点
 * - 确定端点类型
 * - 注册到服务注册表
 * 
 * @param module 模块描述符
 * @param runtime_idx 运行时索引
 * @return 成功返回 0
 */
int static_module_register_service(static_module_desc_t *module, u32 runtime_idx);

/**
 * 启动模块主线程（扩展版本）
 * - 绑定到逻辑核心
 * - 调用入口点
 * 
 * @param module 模块描述符
 * @param runtime_idx 运行时索引
 * @return 成功返回 0
 */
int static_module_start_ex(static_module_desc_t *module, u32 runtime_idx);

/* ==================== 兼容接口（旧） ==================== */

/**
 * 创建静态模块的沙箱
 */
int static_module_create_sandbox(static_module_desc_t *module);

/**
 * 启动静态模块
 */
int static_module_start(static_module_desc_t *module);

/* ==================== 查询 ==================== */

/**
 * 查找静态模块
 * 
 * @param name 模块名称
 * @return 模块描述符指针，未找到返回 NULL
 */
static_module_desc_t* static_module_find(const char *name);

/**
 * 获取模块的域 ID
 * 
 * @param name 模块名称
 * @return 域 ID，未找到返回 HIC_INVALID_DOMAIN
 */
domain_id_t static_module_get_domain(const char *name);

/**
 * 检查模块是否正在运行
 * 
 * @param name 模块名称
 * @return 正在运行返回 true
 */
bool static_module_is_running(const char *name);

/**
 * 获取模块的服务端点
 * 
 * @param name 模块名称
 * @return 服务端点指针，未找到返回 NULL
 */
struct service_endpoint* static_module_get_service(const char *name);

/**
 * 获取模块端点句柄
 * 
 * @param name 模块名称
 * @param handle 输出句柄
 * @return 状态码
 */
hic_status_t static_module_get_endpoint_handle(const char *name, cap_handle_t *handle);

#endif /* STATIC_MODULE_H */
