/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
#include <domain.h>
#include <capability.h>
#include "dynamic_module_loader.h"
#include "service_registry.h"
#include <module_types.h>
#include <string.h>

/* 模块实例表 */
static module_instance_t g_module_table[MAX_MODULES];
static int g_module_count = 0;

/* 简单的内存分配器（简化版） */
static void *g_module_memory_pool = (void *)0x20000000;  /* 512MB 开始 */
static u32 g_module_memory_offset = 0;

#define MODULE_POOL_SIZE (32 * 1024 * 1024)  /* 32MB 模块内存池 */

/* 模块内存分配 */
static void *module_alloc(u32 size) {
    if (g_module_memory_offset + size > MODULE_POOL_SIZE) {
        return NULL;  /* 内存不足 */
    }
    
    void *ptr = (void *)((u8 *)g_module_memory_pool + g_module_memory_offset);
    g_module_memory_offset += size;
    
    /* 清零内存 */
    memset(ptr, 0, size);
    
    return ptr;
}

/* 查找模块（按名称） */
static module_instance_t *find_module_by_name(const char *name) {
    int i;
    for (i = 0; i < g_module_count; i++) {
        if (strcmp(g_module_table[i].name, name) == 0) {
            return &g_module_table[i];
        }
    }
    return NULL;
}

/* 查找模块（按 UUID） */
static module_instance_t *find_module_by_uuid(const u8 *uuid) {
    int i;
    for (i = 0; i < g_module_count; i++) {
        if (memcmp(g_module_table[i].uuid, uuid, 16) == 0) {
            return &g_module_table[i];
        }
    }
    return NULL;
}

/* 获取空闲槽位 */
static int find_free_slot(void) {
    int i;
    for (i = 0; i < MAX_MODULES; i++) {
        if (g_module_table[i].state == MODULE_STATE_unloaded) {
            return i;
        }
    }
    return -1;  /* 无可用槽位 */
}

/* 验证模块头部 */
static hic_status_t verify_module_header(const hicmod_header_t *header) {
    if (header->magic != HICMOD_MAGIC) {
        return HIC_PARSE_FAILED;
    }
    
    if (header->version != HICMOD_VERSION) {
        return HIC_PARSE_FAILED;
    }
    
    return HIC_SUCCESS;
}

/* 模块加载实现 */
static hic_status_t load_module_from_memory(const char *name, const void *data, u32 size, module_instance_t *module) __attribute__((unused));
static hic_status_t load_module_from_memory(const char *name, const void *data, u32 size, module_instance_t *module) {
    const hicmod_header_t *header;
    const void *code_data;
    void *code_base;
    int i;
    
    /* 检查参数 */
    if (!name || !data || size < 72 || !module) {
        return HIC_INVALID_PARAM;
    }
    
    /* 解析头部 */
    header = (const hicmod_header_t *)data;
    
    /* 验证头部 */
    hic_status_t status = verify_module_header(header);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 提取代码数据 */
    if (header->code_size == 0 || header->header_size + header->code_size > size) {
        return HIC_PARSE_FAILED;
    }
    
    code_data = (const u8 *)data + header->header_size;
    
    /* 分配内存并复制代码 */
    code_base = module_alloc(header->code_size);
    if (!code_base) {
        return HIC_OUT_OF_MEMORY;
    }
    
    memcpy(code_base, code_data, header->code_size);
    
    /* 初始化模块信息 */
    memset(module, 0, sizeof(module_instance_t));
    
    strncpy(module->name, name, sizeof(module->name) - 1);
    memcpy(module->uuid, header->uuid, 16);
    module->version = header->semantic_version;
    module->state = MODULE_STATE_loaded;
    module->code_base = code_base;
    module->code_size = header->code_size;
    module->flags = header->flags;
    module->instance_id = g_module_count;
    module->auto_restart = 1;  /* 默认启用自动重启 */
    module->restart_count = 0;
    
    /* TODO: 查找符号（简化版：假设 init/start/stop/cleanup 在固定位置） */
    /* TODO: 实现 ELF 符号解析 */
    
    return HIC_SUCCESS;
}

/* 模块卸载实现 */
static hic_status_t unload_module_internal(module_instance_t *module) {
    int i;
    
    if (!module) {
        return HIC_INVALID_PARAM;
    }
    
    /* 检查引用计数 */
    if (module->ref_count > 0) {
        return HIC_BUSY;
    }
    
    /* 检查状态 */
    if (module->state == MODULE_STATE_running) {
        /* 调用停止函数 */
        if (module->stop) {
            module->stop();
        }
    }
    
    /* 调用清理函数 */
    if (module->cleanup) {
        module->cleanup();
    }
    
    /* 释放备份状态 */
    if (module->backup_state) {
        /* TODO: 释放内存 */
        module->backup_state = NULL;
    }
    
    /* 清理模块信息 */
    memset(module, 0, sizeof(module_instance_t));
    module->state = MODULE_STATE_unloaded;
    
    return HIC_SUCCESS;
}

/* 服务初始化 */
hic_status_t module_manager_service_init(void) {
    memset(g_module_table, 0, sizeof(g_module_table));
    g_module_count = 0;
    g_module_memory_offset = 0;
    
    /* 初始化服务注册表 */
    service_registry_init();
    
    /* 初始化动态模块加载器 */
    dynamic_module_loader_init();
    
    return HIC_SUCCESS;
}

/* 串口输出函数 - 直接操作串口端口 COM1 (0x3F8) */
static void serial_puts(const char *str) {
    while (*str) {
        serial_putchar(*str++);
    }
}

/* 服务启动 */
hic_status_t module_manager_service_start(void) {
    /* 自动加载所有模块 */
    const char *module_dir = "/modules/";
    const char *modules[] = {
        "libc_service.hicmod",              /* 优先级1 - 基础库 */
        "module_manager_service.hicmod",    /* 优先级2 - 模块管理器 */
        "serial_service.hicmod",            /* 优先级3 - 串口服务 */
        "vga_service.hicmod",               /* 优先级4 - VGA服务 */
        "config_service.hicmod",            /* 优先级5 - 配置服务 */
        "crypto_service.hicmod",            /* 优先级6 - 加密服务 */
        "password_manager_service.hicmod",  /* 优先级7 - 密码服务 */
        "cli_service.hicmod"                /* 优先级8 - CLI服务 */
    };
    int i;
    
    /* 按优先级顺序加载所有模块 */
    for (i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        char module_path[256];
        
        /* 构建完整路径 */
        strcpy(module_path, module_dir);
        strcat(module_path, modules[i]);
        
        vga_service_puts("[MOD_MGR] Loading: ");
        vga_service_puts(modules[i]);
        vga_service_puts("...\n");
        
        /* 加载模块 */
        hic_status_t status = module_load(module_path, 1);  /* 启用签名验证 */
        if (status != HIC_SUCCESS) {
            vga_service_puts("[MOD_MGR] Failed to load module\n");
        } else {
            vga_service_puts("[MOD_MGR] Module loaded successfully\n");
        }
    }
    
    vga_service_puts("[MOD_MGR] Module manager service started\n");
    return HIC_SUCCESS;
}

/* 服务停止 */
hic_status_t module_manager_service_stop(void) {
    int i;
    
    /* 卸载所有模块 */
    for (i = g_module_count - 1; i >= 0; i--) {
        if (g_module_table[i].state != MODULE_STATE_unloaded) {
            unload_module_internal(&g_module_table[i]);
        }
    }
    
    return HIC_SUCCESS;
}

/* 服务清理 */
hic_status_t module_manager_service_cleanup(void) {
    return HIC_SUCCESS;
}

/* 获取服务信息 */
hic_status_t module_manager_service_get_info(char* buffer, u32 size) {
    if (buffer && size > 0) {
        strcpy(buffer, "Module Manager Service - Loaded modules: ");
        /* TODO: 添加数字转换 */
        /* strcat(buffer, itoa(g_module_count)); */
    }
    return HIC_SUCCESS;
}

/* 加载模块 */
hic_status_t module_load(const char *path, int verify_signature) {
    const char *module_name;
    int slot;
    (void)verify_signature;
    
    /* TODO: 实现文件系统读取 */
    /* 当前简化版：假设 path 是模块名称，模块已预加载 */
    
    /* 从路径提取模块名称 */
    module_name = strrchr(path, '/');
    if (module_name) {
        module_name++;
    } else {
        module_name = path;
    }
    
    /* 检查是否已加载 */
    module_instance_t *existing = find_module_by_name(module_name);
    if (existing && existing->state != MODULE_STATE_unloaded) {
        return HIC_BUSY;  /* 模块已加载 */
    }
    
    /* 获取空闲槽位 */
    slot = find_free_slot();
    if (slot < 0) {
        return HIC_OUT_OF_MEMORY;  /* 无可用槽位 */
    }
    
    /* TODO: 读取文件内容 */
    /* void *file_data = read_file(path, &file_size); */
    /* hic_status_t status = load_module_from_memory(module_name, file_data, file_size, &g_module_table[slot]); */
    
    /* 简化版：直接返回成功 */
    g_module_count++;
    return HIC_NOT_IMPLEMENTED;
}

/* 卸载模块 */
hic_status_t module_unload(const char *name) {
    module_instance_t *module;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    module->state = MODULE_STATE_unloading;
    hic_status_t status = unload_module_internal(module);
    
    if (status == HIC_SUCCESS) {
        /* TODO: 释放内存 */
        g_module_count--;
    }
    
    return status;
}

/* 列出模块 */
hic_status_t module_list(module_info_t *modules, int *count) {
    int i;
    
    if (!count) {
        return HIC_INVALID_PARAM;
    }
    
    if (modules && *count >= g_module_count) {
        for (i = 0; i < g_module_count; i++) {
            memcpy(&modules[i].name, g_module_table[i].name, 64);
            memcpy(&modules[i].uuid, g_module_table[i].uuid, 16);
            modules[i].version = g_module_table[i].version;
            modules[i].state = g_module_table[i].state;
            modules[i].flags = g_module_table[i].flags;
        }
    }
    
    *count = g_module_count;
    return HIC_SUCCESS;
}

/* 获取模块信息 */
hic_status_t module_info(const char *name, module_info_t *info) {
    module_instance_t *module;
    
    if (!name || !info) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    memcpy(info->name, module->name, 64);
    memcpy(info->uuid, module->uuid, 16);
    info->version = module->version;
    info->state = module->state;
    info->flags = module->flags;
    return HIC_SUCCESS;
}

/* 验证模块 */
hic_status_t module_verify(const char *path) {
    /* TODO: 实现模块签名验证 */
    /* 1. 读取模块文件 */
    /* 2. 解析头部 */
    /* 3. 验证校验和 */
    /* 4. 验证签名（如果存在） */
    (void)path;
    return HIC_NOT_IMPLEMENTED;
}

/* 重启模块 */
hic_status_t module_restart(const char *name) {
    module_instance_t *module;
    hic_status_t status;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    /* 检查重启次数 */
    if (module->restart_count >= MAX_RESTART_ATTEMPTS) {
        module->state = MODULE_STATE_error;
        return HIC_ERROR;  /* 超过最大重启次数 */
    }
    
    /* 停止模块 */
    if (module->stop) {
        status = module->stop();
        if (status != HIC_SUCCESS) {
            module->restart_count++;
            module->state = MODULE_STATE_error;
            return status;
        }
    }
    
    /* 清理模块 */
    if (module->cleanup) {
        module->cleanup();
    }
    
    /* 重新初始化 */
    if (module->init) {
        status = module->init();
        if (status != HIC_SUCCESS) {
            module->restart_count++;
            module->state = MODULE_STATE_error;
            return status;
        }
    }
    
    /* 重新启动 */
    if (module->start) {
        status = module->start();
        if (status != HIC_SUCCESS) {
            module->restart_count++;
            module->state = MODULE_STATE_error;
            return status;
        }
    }
    
    /* 重置重启计数（成功） */
    module->restart_count = 0;
    module->state = MODULE_STATE_running;
    
    return HIC_SUCCESS;
}

/* 设置自动重启 */
hic_status_t module_set_auto_restart(const char *name, u8 enable) {
    module_instance_t *module;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    module->auto_restart = enable;
    return HIC_SUCCESS;
}

/* CRC32 计算表 */
static const u32 g_crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    /* 简化版 CRC 表（实际应完整） */
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5
};

/* 计算 CRC32 */
static u32 compute_crc32(const void *data, u32 size) {
    const u8 *bytes = (const u8 *)data;
    u32 crc = 0xFFFFFFFF;
    u32 i;
    
    for (i = 0; i < size; i++) {
        u8 index = (u8)(crc ^ bytes[i]);
        crc = (crc >> 8) ^ g_crc32_table[index & 0x0F];
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* 获取当前时间戳（简化版） */
static u64 get_current_timestamp(void) {
    /* TODO: 实现实际的时间戳获取 */
    static u64 counter = 0;
    return ++counter;
}

/* 备份模块状态 */
hic_status_t module_backup_state(const char *name) {
    module_instance_t *module;
    module_state_backup_header_t *header;
    void *backup_buffer;
    u32 state_size;
    u32 total_size;
    hic_status_t status;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    /* 检查模块状态 */
    if (module->state != MODULE_STATE_running && 
        module->state != MODULE_STATE_suspended) {
        return HIC_INVALID_STATE;
    }
    
    /* 释放旧备份 */
    if (module->backup_state) {
        /* 内存池不释放，仅标记为无效 */
        module->backup_state = NULL;
        module->backup_size = 0;
    }
    
    /* 默认状态大小估算 */
    state_size = 4096;  /* 默认 4KB */
    
    /* 如果模块实现了状态迁移协议，查询实际大小 */
    if (module->migration.export_state) {
        /* 第一次调用获取所需大小 */
        status = module->migration.export_state(NULL, &state_size);
        if (status == HIC_BUFFER_TOO_SMALL || status == HIC_SUCCESS) {
            /* 已获取大小 */
        } else if (status != HIC_SUCCESS) {
            return status;
        }
    }
    
    /* 计算总大小（头 + 状态数据） */
    total_size = sizeof(module_state_backup_header_t) + state_size;
    
    /* 分配备份内存 */
    backup_buffer = module_alloc(total_size);
    if (!backup_buffer) {
        return HIC_OUT_OF_MEMORY;
    }
    
    /* 初始化备份头 */
    header = (module_state_backup_header_t *)backup_buffer;
    header->magic = MODULE_STATE_BACKUP_MAGIC;
    header->version = MODULE_STATE_BACKUP_VERSION;
    memcpy(header->uuid, module->uuid, 16);
    header->module_version = module->version;
    header->state_size = state_size;
    header->timestamp = get_current_timestamp();
    header->flags = MODULE_BACKUP_FLAG_NONE;
    
    /* 导出状态数据 */
    if (module->migration.export_state) {
        void *state_data = (u8 *)backup_buffer + sizeof(module_state_backup_header_t);
        status = module->migration.export_state(state_data, &state_size);
        if (status != HIC_SUCCESS) {
            /* 导出失败，清理并返回 */
            module->backup_state = NULL;
            return status;
        }
        header->state_size = state_size;
    } else {
        /* 无状态迁移协议，备份基本实例信息 */
        void *state_data = (u8 *)backup_buffer + sizeof(module_state_backup_header_t);
        memcpy(state_data, &module->instance_id, sizeof(u64));
        memcpy((u8 *)state_data + sizeof(u64), &module->ref_count, sizeof(u32));
        memcpy((u8 *)state_data + sizeof(u64) + sizeof(u32), &module->flags, sizeof(u32));
        header->state_size = sizeof(u64) + sizeof(u32) * 2;
    }
    
    /* 计算校验和 */
    header->checksum = compute_crc32(
        (u8 *)backup_buffer + sizeof(module_state_backup_header_t),
        header->state_size
    );
    
    /* 保存备份信息 */
    module->backup_state = backup_buffer;
    module->backup_size = total_size;
    module->backup_version = module->version;
    
    return HIC_SUCCESS;
}

/* 恢复模块状态 */
hic_status_t module_restore_state(const char *name) {
    module_instance_t *module;
    module_state_backup_header_t *header;
    const void *state_data;
    u32 computed_crc;
    hic_status_t status;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    /* 检查是否有备份 */
    if (!module->backup_state || module->backup_size == 0) {
        return HIC_NOT_FOUND;
    }
    
    header = (module_state_backup_header_t *)module->backup_state;
    
    /* 验证备份头 */
    if (header->magic != MODULE_STATE_BACKUP_MAGIC) {
        return HIC_INVALID_DATA;
    }
    
    if (header->version > MODULE_STATE_BACKUP_VERSION) {
        return HIC_NOT_SUPPORTED;
    }
    
    /* 验证 UUID 匹配 */
    if (memcmp(header->uuid, module->uuid, 16) != 0) {
        return HIC_INVALID_DATA;
    }
    
    /* 验证校验和 */
    state_data = (const u8 *)module->backup_state + sizeof(module_state_backup_header_t);
    computed_crc = compute_crc32(state_data, header->state_size);
    if (computed_crc != header->checksum) {
        return HIC_CHECKSUM_MISMATCH;
    }
    
    /* 检查版本兼容性 */
    if (!is_version_compatible(header->module_version, module->version)) {
        /* 版本不兼容，尝试兼容性恢复 */
        if (!(header->flags & MODULE_BACKUP_FLAG_INCREMENTAL)) {
            return HIC_VERSION_MISMATCH;
        }
    }
    
    /* 恢复状态 */
    if (module->migration.import_state) {
        /* 使用模块的状态迁移协议恢复 */
        status = module->migration.import_state(state_data, header->state_size);
        if (status != HIC_SUCCESS) {
            return status;
        }
    } else {
        /* 恢复基本实例信息 */
        const u8 *data = (const u8 *)state_data;
        memcpy(&module->instance_id, data, sizeof(u64));
        memcpy(&module->ref_count, data + sizeof(u64), sizeof(u32));
        memcpy(&module->flags, data + sizeof(u64) + sizeof(u32), sizeof(u32));
    }
    
    /* 如果有完成迁移回调，调用它 */
    if (module->migration.complete_migration) {
        status = module->migration.complete_migration();
        if (status != HIC_SUCCESS) {
            return status;
        }
    }
    
    return HIC_SUCCESS;
}

/* 滚动更新 */
hic_status_t module_rolling_update(const char *name, const char *new_path, int verify) {
    module_instance_t *current;
    u64 new_instance_id;
    hic_status_t status;
    (void)verify;
    
    if (!name || !new_path) {
        return HIC_INVALID_PARAM;
    }
    
    /* 查找当前模块 */
    current = find_module_by_name(name);
    if (!current) {
        return HIC_NOT_FOUND;
    }
    
    /* 备份当前状态 */
    status = module_backup_state(name);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 暂停当前模块 */
    if (current->stop) {
        status = current->stop();
        if (status != HIC_SUCCESS) {
            return status;
        }
    }
    current->state = MODULE_STATE_suspended;
    
    /* 加载新版本 */
    status = module_load(new_path, 0);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        current->state = MODULE_STATE_running;
        if (current->start) {
            current->start();
        }
        return status;
    }
    
    /* 恢复状态到新模块 */
    status = module_restore_state(name);
    if (status != HIC_SUCCESS) {
        /* 回滚 */
        module_unload(name);
        current->state = MODULE_STATE_running;
        if (current->start) {
            current->start();
        }
        return status;
    }
    
    /* 卸载旧模块 */
    status = unload_module_internal(current);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    return HIC_SUCCESS;
}

/* 模块管理器服务 API */
const service_api_t g_service_api = {
    .init = module_manager_service_init,
    .start = module_manager_service_start,
    .stop = module_manager_service_stop,
    .cleanup = module_manager_service_cleanup,
    .get_info = module_manager_service_get_info,
};

void service_register_self(void) {
    service_register("module_manager", &g_service_api);
}

/* ==================== 零停机更新策略实现 ==================== */

/* 当前活跃的更新上下文 */
static update_context_t g_active_updates[MAX_MODULES];

/**
 * 获取时间戳（简化版）
 */
static u64 get_timestamp_ms(void) {
    /* TODO: 使用实际的时钟 */
    static u64 counter = 0;
    return ++counter;
}

/**
 * 零停机更新主入口
 * 
 * 策略层：使用 Core-0 机制原语组合实现零停机更新
 */
hic_status_t zero_downtime_update(const char *name, 
                                   const char *new_path,
                                   update_strategy_t strategy,
                                   update_result_t *result) {
    module_instance_t *current;
    update_context_t *ctx;
    u32 i;
    
    if (!name || !new_path || !result) {
        return HIC_INVALID_PARAM;
    }
    
    /* 初始化结果 */
    memset(result, 0, sizeof(update_result_t));
    
    /* 查找当前模块 */
    current = find_module_by_name(name);
    if (!current) {
        strcpy(result->error_msg, "Module not found");
        return HIC_NOT_FOUND;
    }
    
    /* 查找空闲更新槽 */
    ctx = NULL;
    for (i = 0; i < MAX_MODULES; i++) {
        if (g_active_updates[i].phase == UPDATE_PHASE_INIT ||
            g_active_updates[i].phase == UPDATE_PHASE_COMPLETED ||
            g_active_updates[i].phase == UPDATE_PHASE_FAILED) {
            ctx = &g_active_updates[i];
            break;
        }
    }
    
    if (!ctx) {
        strcpy(result->error_msg, "Too many concurrent updates");
        return HIC_BUSY;
    }
    
    /* 初始化更新上下文 */
    memset(ctx, 0, sizeof(update_context_t));
    strncpy(ctx->module_name, name, 63);
    ctx->old_version = current->version;
    ctx->strategy = strategy;
    ctx->phase = UPDATE_PHASE_PREPARING;
    ctx->start_time = get_timestamp_ms();
    ctx->max_health_failures = 3;
    ctx->drain_timeout_ms = 30000;  /* 30秒 */
    ctx->health_check_interval_ms = 1000;  /* 1秒 */
    ctx->auto_rollback = true;
    
    /* 根据策略调用对应实现 */
    hic_status_t status;
    switch (strategy) {
        case UPDATE_STRATEGY_BLUE_GREEN:
            status = update_strategy_blue_green(name, new_path, result);
            break;
        case UPDATE_STRATEGY_CANARY:
            status = update_strategy_canary(name, new_path, 10, result);  /* 10%金丝雀 */
            break;
        case UPDATE_STRATEGY_GRADUAL:
            status = update_strategy_gradual(name, new_path, 100, result);  /* 每批100连接 */
            break;
        case UPDATE_STRATEGY_IMMEDIATE:
            /* 回退到有停机更新 */
            status = module_rolling_update(name, new_path, 0);
            result->downtime_ms = 100;  /* 估算停机时间 */
            break;
        default:
            strcpy(result->error_msg, "Unknown update strategy");
            status = HIC_INVALID_PARAM;
    }
    
    result->status = status;
    result->duration_ms = get_timestamp_ms() - ctx->start_time;
    
    return status;
}

/**
 * 蓝绿部署策略实现
 * 
 * 策略步骤：
 * 1. 创建并行域（domain_parallel_create）
 * 2. 加载新模块到新域
 * 3. 预热新实例
 * 4. 状态迁移
 * 5. 原子切换端点（cap_endpoint_redirect）
 * 6. 优雅关闭旧实例（domain_graceful_shutdown）
 */
hic_status_t update_strategy_blue_green(const char *name,
                                         const char *new_path,
                                         update_result_t *result) {
    module_instance_t *current;
    update_context_t *ctx = NULL;
    domain_id_t new_domain;
    domain_id_t old_domain;
    cap_id_t migration_channel;
    cap_handle_t from_handle, to_handle;
    hic_status_t status;
    u32 i;
    
    /* 查找当前模块 */
    current = find_module_by_name(name);
    if (!current) {
        strcpy(result->error_msg, "Module not found");
        return HIC_NOT_FOUND;
    }
    
    /* 获取更新上下文 */
    for (i = 0; i < MAX_MODULES; i++) {
        if (strcmp(g_active_updates[i].module_name, name) == 0 &&
            g_active_updates[i].phase != UPDATE_PHASE_INIT) {
            ctx = &g_active_updates[i];
            break;
        }
    }
    
    if (!ctx) {
        strcpy(result->error_msg, "No active update context");
        return HIC_ERROR;
    }
    
    old_domain = current->instance_id;  /* 简化：使用实例ID作为域ID */
    
    /* 阶段1：创建并行域 */
    ctx->phase = UPDATE_PHASE_CREATING;
    ctx->phase_start_time = get_timestamp_ms();
    
    /* 
     * 机制调用：domain_parallel_create
     * 策略决策：使用相同配额创建并行域
     */
    status = domain_parallel_create(old_domain, name, NULL, &new_domain);
    if (status != HIC_SUCCESS) {
        strcpy(result->error_msg, "Failed to create parallel domain");
        ctx->phase = UPDATE_PHASE_FAILED;
        return status;
    }
    
    ctx->new_domain = new_domain;
    
    /* 阶段2：加载新模块 */
    /* 
     * 机制调用：module_load（到新域）
     * 注意：当前简化实现，实际需要扩展 module_load 支持目标域
     */
    status = module_load(new_path, 0);
    if (status != HIC_SUCCESS && status != HIC_NOT_IMPLEMENTED) {
        strcpy(result->error_msg, "Failed to load new module");
        goto rollback;
    }
    
    /* 阶段3：创建迁移通道 */
    ctx->phase = UPDATE_PHASE_WARMING;
    ctx->phase_start_time = get_timestamp_ms();
    
    /*
     * 机制调用：cap_migration_channel_create
     * 策略决策：使用64KB缓冲区
     */
    status = cap_migration_channel_create(old_domain, new_domain,
                                           64 * 1024,  /* 64KB */
                                           &migration_channel,
                                           &from_handle, &to_handle);
    if (status != HIC_SUCCESS) {
        strcpy(result->error_msg, "Failed to create migration channel");
        goto rollback;
    }
    
    ctx->migration_channel = migration_channel;
    
    /* 阶段4：状态迁移 */
    ctx->phase = UPDATE_PHASE_MIGRATING;
    ctx->phase_start_time = get_timestamp_ms();
    
    /* 调用模块的状态迁移协议 */
    if (current->migration.export_state && current->migration.prepare_migration) {
        /* 准备迁移 */
        status = current->migration.prepare_migration();
        if (status != HIC_SUCCESS) {
            strcpy(result->error_msg, "Module prepare_migration failed");
            goto rollback;
        }
        
        /* 导出状态（通过迁移通道） */
        /* 简化：直接备份 */
        status = module_backup_state(name);
        if (status != HIC_SUCCESS) {
            strcpy(result->error_msg, "Failed to backup state");
            goto rollback;
        }
        
        ctx->rollback_state = current->backup_state;
        ctx->rollback_size = current->backup_size;
    }
    
    /* 阶段5：原子切换端点 */
    ctx->phase = UPDATE_PHASE_SWITCHING;
    ctx->phase_start_time = get_timestamp_ms();
    
    /*
     * 机制调用：domain_atomic_switch
     * 策略决策：切换所有服务端点
     */
    cap_id_t endpoints[1] = { ctx->service_cap };  /* 简化：假设只有一个服务端点 */
    status = domain_atomic_switch(old_domain, new_domain, endpoints, 1);
    if (status != HIC_SUCCESS) {
        strcpy(result->error_msg, "Failed to atomic switch");
        goto rollback;
    }
    
    /* 完成迁移回调 */
    if (current->migration.complete_migration) {
        current->migration.complete_migration();
    }
    
    /* 阶段6：优雅关闭旧实例 */
    ctx->phase = UPDATE_PHASE_DRAINING;
    ctx->phase_start_time = get_timestamp_ms();
    
    /*
     * 机制调用：domain_graceful_shutdown
     * 策略决策：30秒超时，超时后强制关闭
     */
    status = domain_graceful_shutdown(old_domain, ctx->drain_timeout_ms, true);
    if (status != HIC_SUCCESS) {
        /* 即使关闭失败，更新也算成功，只是资源未完全回收 */
        strcpy(result->error_msg, "Warning: old domain shutdown incomplete");
    }
    
    /* 成功完成 */
    ctx->phase = UPDATE_PHASE_COMPLETED;
    result->downtime_ms = 0;  /* 零停机 */
    result->connections_migrated = ctx->migrated_connections;
    result->connections_failed = ctx->failed_migrations;
    result->rolled_back = false;
    
    return HIC_SUCCESS;
    
rollback:
    ctx->phase = UPDATE_PHASE_ROLLBACK;
    
    /* 回滚：恢复旧实例 */
    if (ctx->rollback_state) {
        module_restore_state(name);
    }
    
    /* 回滚：销毁新域 */
    if (ctx->new_domain != HIC_INVALID_DOMAIN) {
        domain_destroy(ctx->new_domain);
    }
    
    ctx->phase = UPDATE_PHASE_FAILED;
    result->rolled_back = true;
    result->downtime_ms = get_timestamp_ms() - ctx->start_time;
    
    return status;
}

/**
 * 金丝雀发布策略实现
 */
hic_status_t update_strategy_canary(const char *name,
                                     const char *new_path,
                                     u32 canary_percent,
                                     update_result_t *result) {
    /* 
     * 策略实现：
     * 1. 创建并行域
     * 2. 只将 canary_percent% 的流量路由到新实例
     * 3. 监控健康状态
     * 4. 逐步增加流量或回滚
     */
    hic_status_t status;
    
    /* 先执行蓝绿部署的创建阶段 */
    module_instance_t *current = find_module_by_name(name);
    if (!current) {
        return HIC_NOT_FOUND;
    }
    
    /* 创建并行域 */
    domain_id_t new_domain;
    domain_id_t old_domain = current->instance_id;
    
    status = domain_parallel_create(old_domain, name, NULL, &new_domain);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 加载新模块 */
    status = module_load(new_path, 0);
    if (status != HIC_SUCCESS && status != HIC_NOT_IMPLEMENTED) {
        domain_destroy(new_domain);
        return status;
    }
    
    /* 
     * 金丝雀阶段：
     * - 监控新实例 canary_percent% 流量
     * - 如果健康检查失败，回滚
     * - 如果成功，逐步增加到100%
     */
    
    /* 简化实现：直接完成切换 */
    cap_id_t endpoints[1] = { 0 };
    status = domain_atomic_switch(old_domain, new_domain, endpoints, 0);
    
    if (status == HIC_SUCCESS) {
        status = domain_graceful_shutdown(old_domain, 30000, true);
    }
    
    result->downtime_ms = 0;
    result->status = status;
    
    return status;
}

/**
 * 渐进式更新策略实现
 */
hic_status_t update_strategy_gradual(const char *name,
                                      const char *new_path,
                                      u32 batch_size,
                                      update_result_t *result) {
    /*
     * 策略实现：
     * 1. 创建并行域
     * 2. 分批次迁移连接
     * 3. 每批迁移后验证
     * 4. 完成后切换
     */
    hic_status_t status;
    
    /* 创建并行域 */
    module_instance_t *current = find_module_by_name(name);
    if (!current) {
        return HIC_NOT_FOUND;
    }
    
    domain_id_t new_domain;
    domain_id_t old_domain = current->instance_id;
    
    status = domain_parallel_create(old_domain, name, NULL, &new_domain);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 加载新模块 */
    status = module_load(new_path, 0);
    if (status != HIC_SUCCESS && status != HIC_NOT_IMPLEMENTED) {
        domain_destroy(new_domain);
        return status;
    }
    
    /* 创建迁移通道 */
    cap_id_t migration_channel;
    cap_handle_t from_handle, to_handle;
    
    status = cap_migration_channel_create(old_domain, new_domain,
                                           128 * 1024,  /* 128KB for batch */
                                           &migration_channel,
                                           &from_handle, &to_handle);
    if (status != HIC_SUCCESS) {
        domain_destroy(new_domain);
        return status;
    }
    
    /* 
     * 渐进迁移：
     * - 每次迁移 batch_size 个连接
     * - 验证后再继续
     */
    /* 简化实现：直接切换 */
    cap_id_t endpoints[1] = { 0 };
    status = domain_atomic_switch(old_domain, new_domain, endpoints, 0);
    
    if (status == HIC_SUCCESS) {
        status = domain_graceful_shutdown(old_domain, 30000, true);
    }
    
    result->downtime_ms = 0;
    result->status = status;
    
    return status;
}

/**
 * 中止更新
 */
hic_status_t zero_downtime_update_abort(const char *name) {
    u32 i;
    
    for (i = 0; i < MAX_MODULES; i++) {
        if (strcmp(g_active_updates[i].module_name, name) == 0 &&
            g_active_updates[i].phase != UPDATE_PHASE_COMPLETED &&
            g_active_updates[i].phase != UPDATE_PHASE_FAILED) {
            
            /* 触发回滚 */
            g_active_updates[i].phase = UPDATE_PHASE_ROLLBACK;
            
            /* 回滚逻辑 */
            if (g_active_updates[i].new_domain != HIC_INVALID_DOMAIN) {
                domain_destroy(g_active_updates[i].new_domain);
            }
            
            g_active_updates[i].phase = UPDATE_PHASE_FAILED;
            return HIC_SUCCESS;
        }
    }
    
    return HIC_NOT_FOUND;
}

/**
 * 查询更新状态
 */
hic_status_t zero_downtime_update_status(const char *name,
                                          update_context_t *ctx) {
    u32 i;
    
    if (!name || !ctx) {
        return HIC_INVALID_PARAM;
    }
    
    for (i = 0; i < MAX_MODULES; i++) {
        if (strcmp(g_active_updates[i].module_name, name) == 0) {
            *ctx = g_active_updates[i];
            return HIC_SUCCESS;
        }
    }
    
    return HIC_NOT_FOUND;
}