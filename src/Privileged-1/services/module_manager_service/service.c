/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

#include "service.h"
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
    
    return HIC_SUCCESS;
}

/* 服务启动 */
hic_status_t module_manager_service_start(void) {
    /* 自动加载所有模块 */
    const char *module_dir = "/build/privileged-1/modules/";
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
        
        /* 加载模块 */
        hic_status_t status = module_load(module_path, 1);  /* 启用签名验证 */
        if (status != HIC_SUCCESS) {
            /* 加载失败，记录但继续 */
            /* TODO: 添加日志记录 */
            (void)status;
        }
    }
    
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

/* 备份模块状态 */
hic_status_t module_backup_state(const char *name) {
    module_instance_t *module;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    /* TODO: 实现状态备份 */
    /* 1. 分配备份内存 */
    /* 2. 复制模块状态 */
    
    return HIC_NOT_IMPLEMENTED;
}

/* 恢复模块状态 */
hic_status_t module_restore_state(const char *name) {
    module_instance_t *module;
    
    if (!name) {
        return HIC_INVALID_PARAM;
    }
    
    module = find_module_by_name(name);
    if (!module) {
        return HIC_NOT_FOUND;
    }
    
    /* TODO: 实现状态恢复 */
    
    return HIC_NOT_IMPLEMENTED;
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