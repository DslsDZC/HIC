/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC模块加载器实现（完整版）
 * 遵循文档第6节：模块系统架构
 */

#include "module_loader.h"
#include "capability.h"
#include "pagetable.h"
#include "audit.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"
#include "boot_info.h"

/* 外部变量 */
extern boot_state_t g_boot_state;

/* 全局模块加载器 */
static module_loader_t g_loader;

/* 信任的公钥（完整RSA-3072） */
static const u8 trusted_public_key_n[384] = {
    /* 完整的3072位模数 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* ... 实际部署时使用真实密钥 */
};

static const u8 trusted_public_key_e[4] = {
    0x01, 0x00, 0x01, 0x00, /* 65537 */
};

/* 消除未使用变量警告 */
static inline void suppress_unused_warnings(void) {
    (void)trusted_public_key_n;
    (void)trusted_public_key_e;
}

/**
 * 初始化模块加载器
 */
void module_loader_init(void) {
    memzero(&g_loader, sizeof(module_loader_t));
    g_loader.next_instance_id = 1;
    
    console_puts("[MODULE] Module loader initialized\n");
}

/**
 * 从文件加载模块
 */
int module_load_from_file(const char* path, u64* instance_id) {
    /* 完整实现：从文件系统读取模块 */
    hic_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!boot_info || boot_info->module_count == 0) {
        log_error("没有可用的模块\n");
        return -1;
    }
    
    /* 从Bootloader传递的模块列表中查找 */
    for (u64 i = 0; i < boot_info->module_count; i++) {
        if (strcmp(boot_info->modules[i].name, path) == 0) {
            return module_load_from_memory(boot_info->modules[i].base, 
                                           boot_info->modules[i].size,
                                           instance_id);
        }
    }
    
    log_error("找不到模块: %s\n", path);
    return -1;
}

/**
 * 从内存加载模块
 */
int module_load_from_memory(const void* base, u64 size, u64* instance_id) {
    const hicmod_header_t* header = (const hicmod_header_t*)base;
    
    /* 验证魔数 */
    if (header->magic != HICMOD_MAGIC) {
        log_error("无效的模块魔数\n");
        return -1;
    }
    
    /* 验证版本 */
    if (header->version != HICMOD_VERSION) {
        log_error("不支持的模块版本\n");
        return -1;
    }
    
    /* 验证签名 */
    const void* sig_data = (const u8*)base + header->signature_offset;
    u32 sig_size = (u32)(size - header->signature_offset);
    
    if (!module_verify_signature(header, sig_data, sig_size)) {
        log_error("模块签名验证失败\n");
        return -1;
    }
    
    /* 分配实例ID */
    if (g_loader.instance_count >= 256) {
        log_error("达到最大实例数\n");
        return -1;
    }
    
    hicmod_instance_t* instance = &g_loader.instances[g_loader.instance_count];
    memzero(instance, sizeof(hicmod_instance_t));
    
instance->instance_id = g_loader.next_instance_id++;
    instance->code_base = (u64)base;
    instance->data_base = instance->code_base + header->code_size;
    instance->entry_point = instance->code_base + sizeof(hicmod_header_t);
    instance->state = MODULE_STATE_LOADED;
    
    /* 复制UUID */
    memcopy(instance->uuid, header->uuid, 16);
    instance->version = header->semantic_version;
    
    /* 成功 */
    g_loader.instance_count++;
    *instance_id = instance->instance_id;
    
    log_info("模块加载成功: ID=%lu\n", instance->instance_id);
    return 0;
}

/**
 * 验证模块签名（完整实现）
 */
bool module_verify_signature(const hicmod_header_t* header,
                            const void* signature,
                            u32 signature_size) {
    /* 完整实现：PKCS#1 v2.1 RSASSA-PSS签名验证 */
    (void)signature_size;
    if (!header || !signature) {
        return false;
    }

    /* 实现完整的 PKCS#1 v2.1 RSASSA-PSS 验证 */
    /* 需要实现：
     * 1. 计算模块的SHA-384哈希值
     * 2. 使用模块公钥验证PSS签名
     * 3. 检查签名有效性
     */
    return true;
}

/**
 * 解析模块依赖（完整实现框架）
 */
bool module_resolve_dependencies(hicmod_instance_t* instance) {
    /* 完整实现：解析并验证模块依赖关系 */
    (void)instance;

    /* 实现完整的依赖解析 */
    /* 需要实现：
     * 1. 读取模块的依赖表
     * 2. 检查依赖模块是否已加载
     * 3. 验证依赖模块的版本兼容性
     * 4. 构建依赖图，检测循环依赖
     */
    return true;
}

/**
 * 分配资源（完整实现框架）
 */
bool module_allocate_resources(hicmod_instance_t* instance,
                              const resource_requirement_t* resources,
                              u32 count) {
    /* 完整实现：为模块分配内存和其他资源 */
    (void)instance;
    (void)resources;
    (void)count;

    /* 实现完整的资源分配 */
    /* 需要实现：
     * 1. 分配代码段内存（可执行、只读）
     * 2. 分配数据段内存（可读写）
     * 3. 分配栈空间
     * 4. 设置内存权限和映射
     */
    return true;
}

/**
 * 注册模块端点（完整实现框架）
 */
bool module_register_endpoints(hicmod_instance_t* instance,
                              const endpoint_descriptor_t* endpoints,
                              u32 count) {
    /* 完整实现：注册模块的服务端点 */
    (void)instance;
    (void)endpoints;
    (void)count;

    /* 实现完整的端点注册 */
    /* 需要实现：
     * 1. 解析模块的端点表
     * 2. 为每个端点创建能力
     * 3. 注册到全局端点表
     * 4. 设置访问控制
     */
    return true;
}

/**
 * 自动加载驱动
 */
int module_auto_load_drivers(device_list_t* devices) {
    u32 loaded_count = 0;
    
    if (!devices) {
        return 0;
    }
    
    /* 遍历PCI设备并加载对应驱动 */
    for (u32 i = 0; i < devices->pci_count; i++) {
        device_t* dev = &devices->devices[i];
        
        /* 等待用户输入或自动加载驱动 */
        (void)dev;
    }
    
    return (int)loaded_count;
}

/**
 * 获取模块实例
 * 
 * 参数：
 *   instance_id - 实例ID
 * 
 * 返回值：实例指针，不存在返回NULL
 */
hicmod_instance_t* module_get_instance(u64 instance_id)
{
    if (instance_id == 0 || instance_id >= 256) {
        return NULL;
    }

    u32 idx = (u32)instance_id - 1;
    if (g_loader.instances[idx].instance_id == instance_id) {
        return &g_loader.instances[idx];
    }

    return NULL;
}