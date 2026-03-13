/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC动态模块加载器实现 (Privileged-1版本)
 * 
 * 从引导分区的 modules.list 加载模块
 * 此模块运行在 Privileged-1 层，通过 Core-0 提供的原语接口操作
 */

#include "dynamic_module_loader.h"
#include "service_registry.h"
#include <common.h>
#include <module_types.h>
#include <string.h>

/* 全局加载上下文 */
static dynamic_load_ctx_t g_load_ctx;

/* 模块加载缓冲区 */
#define MODULE_BUFFER_SIZE (1024 * 1024)  /* 1MB */
static u8 g_module_buffer[MODULE_BUFFER_SIZE];

/* ==================== 初始化 ==================== */

void dynamic_module_loader_init(void)
{
    memset(&g_load_ctx, 0, sizeof(g_load_ctx));
    /* TODO: 添加日志输出 */
}

/* ==================== 主加载流程 ==================== */

/**
 * 从 modules.list 加载所有模块
 */
int dynamic_module_load_all(void)
{
    /* TODO: 添加日志输出 */
    
    /* 步骤1: 读取 modules.list */
    int entry_count = dynamic_read_modules_list(
        g_load_ctx.entries, 
        MAX_MODULES_LIST_ENTRIES
    );
    
    if (entry_count <= 0) {
        return 0;
    }
    
    g_load_ctx.entry_count = (u32)entry_count;
    
    /* 步骤2: 依次加载每个模块 */
    for (u32 i = 0; i < g_load_ctx.entry_count; i++) {
        dynamic_module_entry_t *entry = &g_load_ctx.entries[i];
        
        hic_status_t status = dynamic_module_load(entry->name, entry->path);
        
        if (status == HIC_SUCCESS) {
            entry->state = DLOAD_RUNNING;
            g_load_ctx.loaded_count++;
        } else {
            entry->state = DLOAD_FAILED;
            entry->last_error = status;
            g_load_ctx.failed_count++;
        }
    }
    
    return (int)g_load_ctx.loaded_count;
}

/**
 * 加载单个模块
 */
hic_status_t dynamic_module_load(const char *module_name, const char *path)
{
    u32 module_size;
    u32 bytes_read;
    hic_status_t status;
    (void)path;
    
    /* 步骤a: 读取模块文件 */
    status = dynamic_read_module_file(module_name, g_module_buffer, MODULE_BUFFER_SIZE, &bytes_read);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    module_size = bytes_read;
    
    /* 步骤b: 验证模块签名 */
    status = dynamic_verify_module(g_module_buffer, module_size);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 步骤c: 解析模块头 */
    const hicmod_header_t *header = (const hicmod_header_t *)g_module_buffer;
    
    if (header->magic != HICMOD_MAGIC) {
        return HIC_PARSE_FAILED;
    }
    
    /* 步骤d: 创建沙箱 */
    dynamic_module_entry_t temp_entry;
    memset(&temp_entry, 0, sizeof(temp_entry));
    strncpy(temp_entry.name, module_name, sizeof(temp_entry.name) - 1);
    
    status = dynamic_create_sandbox(g_module_buffer, module_size, &temp_entry);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 步骤e: 启动模块 */
    status = dynamic_start_module(&temp_entry);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 流程步骤 ==================== */

/**
 * 读取 modules.list 文件
 */
int dynamic_read_modules_list(dynamic_module_entry_t *entries, u32 max_entries)
{
    /* 查找 FAT32 服务 */
    service_endpoint_t *fat32_svc = service_find_by_name("fat32_service");
    if (!fat32_svc) {
        return 0;
    }
    
    /* 
     * TODO: 通过 IPC 调用 FAT32 服务读取 modules.list
     * 当前简化实现：返回空列表
     */
    
    u32 count = 0;
    (void)entries;
    (void)max_entries;
    
    return (int)count;
}

/**
 * 读取模块文件
 */
hic_status_t dynamic_read_module_file(const char *name, void *buffer, 
                                       u32 buffer_size, u32 *bytes_read)
{
    /* 查找 FAT32 服务 */
    service_endpoint_t *fat32_svc = service_find_by_name("fat32_service");
    if (!fat32_svc) {
        return HIC_NOT_FOUND;
    }
    
    /* 
     * TODO: 通过 IPC 调用 FAT32 服务读取文件
     */
    
    (void)name;
    (void)buffer;
    (void)buffer_size;
    *bytes_read = 0;
    
    return HIC_NOT_FOUND;
}

/**
 * 验证模块签名
 */
hic_status_t dynamic_verify_module(const void *module_data, u32 module_size)
{
    /* 查找签名验证服务 */
    service_endpoint_t *signer_svc = service_find_by_name("module_signer");
    if (!signer_svc) {
        /* 如果签名验证服务不存在，跳过验证 */
        return HIC_SUCCESS;
    }
    
    /* 
     * TODO: 通过 IPC 调用签名验证服务
     */
    
    (void)module_data;
    (void)module_size;
    
    return HIC_SUCCESS;
}

/**
 * 创建模块沙箱
 * 
 * 注意：此函数需要通过 Core-0 提供的原语接口操作
 * 当前为简化实现
 */
hic_status_t dynamic_create_sandbox(const void *module_data, u32 module_size,
                                     dynamic_module_entry_t *entry)
{
    const hicmod_header_t *header = (const hicmod_header_t *)module_data;
    
    /* 验证模块大小 */
    if (module_size < sizeof(hicmod_header_t)) {
        return HIC_INVALID_PARAM;
    }
    
    /* 
     * TODO: 通过 Core-0 原语接口创建域和分配内存
     * 当前简化实现：直接使用固定地址
     */
    
    /* 计算资源需求 */
    u64 code_size = header->code_size;
    u64 data_size = header->data_size;
    
    /* 对齐到页 */
    #define PAGE_SIZE 4096
    u64 total_size = (code_size + data_size + PAGE_SIZE - 1) & ~((u64)(PAGE_SIZE - 1));
    
    /* 简化：分配固定域 ID */
    entry->domain = 100;  /* TODO: 通过原语分配 */
    
    /* 简化：分配固定端点 */
    entry->endpoint = 200;  /* TODO: 通过原语分配 */
    
    (void)total_size;
    
    return HIC_SUCCESS;
}

/**
 * 启动模块
 */
hic_status_t dynamic_start_module(dynamic_module_entry_t *entry)
{
    /* 
     * TODO: 通过 Core-0 原语接口启动模块
     */
    
    (void)entry;
    
    return HIC_SUCCESS;
}

/* ==================== 查询 ==================== */

dynamic_load_ctx_t* dynamic_get_load_context(void)
{
    return &g_load_ctx;
}

dynamic_load_state_t dynamic_get_module_state(const char *name)
{
    for (u32 i = 0; i < g_load_ctx.entry_count; i++) {
        if (strcmp(g_load_ctx.entries[i].name, name) == 0) {
            return g_load_ctx.entries[i].state;
        }
    }
    return DLOAD_PENDING;
}
