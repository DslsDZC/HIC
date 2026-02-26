/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 模块管理服务实现
 */

#include "service.h"
#include "../include/service_api.h"
#include "../../Core-0/lib/mem.h"
#include "../../Core-0/lib/console.h"
#include "../../Core-0/lib/string.h"

/* 模块实例表 */
static module_instance_t g_module_instances[MAX_MODULES];
static u32 g_module_count = 0;

/* 外部函数（从Core-0和密码管理服务调用） */
extern hic_status_t password_verify(const char* password);
extern hic_status_t crypto_sha384(const u8* data, u32 len, u8* hash);
extern hic_status_t domain_create(domain_id_t* out);
extern hic_status_t domain_destroy(domain_id_t domain_id);
extern hic_status_t pmm_alloc_frames(domain_id_t owner, u64 count, u32 type, u64* out);
extern hic_status_t pmm_free_frames(u64 addr, u64 count);

/* ============= 密码验证 ============= */

bool module_verify_password(const char* password) {
    return (password_verify(password) == HIC_SUCCESS);
}

/* ============= 文件读取 ============= */

hic_status_t module_read_file(const char* path, void** buffer, u64* size) {
    /* TODO: 实现文件读取 */
    /* 临时返回成功 */
    (void)path;
    (void)buffer;
    (void)size;
    return HIC_ERROR_NOT_IMPLEMENTED;
}

/* ============= 模块解析 ============= */

hic_status_t module_parse_header(const void* data, u64 size, hicmod_header_t* header) {
    if (!data || size < sizeof(hicmod_header_t) || !header) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memcopy(header, data, sizeof(hicmod_header_t));
    
    /* 验证魔数 */
    if (header->magic != HICMOD_MAGIC) {
        console_puts("[MODULE] Invalid magic number\n");
        return HIC_ERROR_INVALID_FORMAT;
    }
    
    /* 验证版本 */
    if (header->version != HICMOD_VERSION) {
        console_puts("[MODULE] Unsupported version\n");
        return HIC_ERROR_UNSUPPORTED;
    }
    
    /* 验证大小 */
    if (header->header_size > size) {
        console_puts("[MODULE] Invalid header size\n");
        return HIC_ERROR_INVALID_FORMAT;
    }
    
    return HIC_SUCCESS;
}

/* ============= 域管理 ============= */

hic_status_t module_create_domain(const hicmod_metadata_t* metadata, domain_id_t* domain_id) {
    /* TODO: 调用Core-0的domain_create */
    (void)metadata;
    (void)domain_id;
    return HIC_ERROR_NOT_IMPLEMENTED;
}

hic_status_t module_destroy_domain(domain_id_t domain_id) {
    /* TODO: 调用Core-0的domain_destroy */
    (void)domain_id;
    return HIC_ERROR_NOT_IMPLEMENTED;
}

/* ============= 资源分配 ============= */

hic_status_t module_allocate_resources(domain_id_t domain_id, const hicmod_metadata_t* metadata) {
    /* 分配物理内存 */
    u64 phys_addr;
    hic_status_t status = pmm_alloc_frames(domain_id, 
                                          (metadata->max_memory + 4095) / 4096,
                                          1,  /* PAGE_FRAME_PRIVILEGED */
                                          &phys_addr);
    if (status != HIC_SUCCESS) {
        console_puts("[MODULE] Failed to allocate memory\n");
        return status;
    }
    
    console_puts("[MODULE] Allocated ");
    console_putu64(metadata->max_memory);
    console_puts(" bytes for module\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_reclaim_resources(module_instance_t* instance) {
    if (!instance) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* TODO: 释放所有能力 */
    /* TODO: 释放物理内存 */
    
    return HIC_SUCCESS;
}

/* ============= 模块加载 ============= */

hic_status_t module_load_code(module_instance_t* instance, const void* data, u64 size) {
    if (!instance || !data || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* TODO: 加载代码段 */
    /* TODO: 加载数据段 */
    /* TODO: 加载BSS段 */
    /* TODO: 加载只读数据 */
    
    instance->module_base = (u64)data;
    instance->module_size = size;
    
    return HIC_SUCCESS;
}

hic_status_t module_register_endpoints(module_instance_t* instance) {
    if (!instance) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* TODO: 注册模块端点到系统服务注册表 */
    
    return HIC_SUCCESS;
}

/* ============= 签名验证 ============= */

hic_status_t module_verify_signature(const char* module_path) {
    void* data = NULL;
    u64 size = 0;
    
    /* 读取模块文件 */
    hic_status_t status = module_read_file(module_path, &data, &size);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 解析头部 */
    hicmod_header_t header;
    status = module_parse_header(data, size, &header);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* 计算模块哈希 */
    u8 hash[48];
    status = crypto_sha384((const u8*)data, header.code_offset + header.code_size, hash);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    /* TODO: 获取签名并验证 */
    /* 调用crypto_service的RSA验证函数 */
    
    console_puts("[MODULE] Signature verification (TODO: implement full verification)\n");
    
    return HIC_SUCCESS;
}

/* ============= 依赖解析 ============= */

hic_status_t module_parse_dependencies(const char* module_path,
                                       hicmod_dependency_t* dependencies,
                                       u32 max_count, u32* count) {
    (void)module_path;
    (void)dependencies;
    (void)max_count;
    (void)count;
    /* TODO: 实现依赖解析 */
    return HIC_ERROR_NOT_IMPLEMENTED;
}

bool module_check_dependencies(hicmod_dependency_t* dependencies, u32 count) {
    (void)dependencies;
    (void)count;
    /* TODO: 实现依赖检查 */
    return true;
}

/* ============= 公共API ============= */

hic_status_t module_manager_init(void) {
    memzero(g_module_instances, sizeof(g_module_instances));
    g_module_count = 0;
    
    console_puts("[MODULE] Module manager initialized\n");
    return HIC_SUCCESS;
}

hic_status_t module_load(const char* module_path, const char* password,
                         module_load_result_t* result) {
    if (!module_path || !password || !result) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 1. 验证密码 */
    if (!module_verify_password(password)) {
        console_puts("[MODULE] Password verification failed\n");
        result->status = HIC_ERROR_PERMISSION;
        strcpy(result->error_message, "Invalid password");
        return HIC_ERROR_PERMISSION;
    }
    
    /* 2. 检查模块数量 */
    if (g_module_count >= MAX_MODULES) {
        console_puts("[MODULE] Maximum modules reached\n");
        result->status = HIC_ERROR_NO_RESOURCE;
        strcpy(result->error_message, "Maximum modules reached");
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 3. 验证签名 */
    hic_status_t status = module_verify_signature(module_path);
    if (status != HIC_SUCCESS) {
        result->status = status;
        strcpy(result->error_message, "Signature verification failed");
        return status;
    }
    
    /* 4. 读取模块 */
    void* data = NULL;
    u64 size = 0;
    status = module_read_file(module_path, &data, &size);
    if (status != HIC_SUCCESS) {
        result->status = status;
        strcpy(result->error_message, "Failed to read module");
        return status;
    }
    
    /* 5. 解析头部 */
    hicmod_header_t header;
    status = module_parse_header(data, size, &header);
    if (status != HIC_SUCCESS) {
        result->status = status;
        strcpy(result->error_message, "Invalid module format");
        return status;
    }
    
    /* 6. 创建实例 */
    module_instance_t* instance = &g_module_instances[g_module_count];
    memzero(instance, sizeof(module_instance_t));
    
    instance->instance_id = g_module_count + 1;
    instance->state = MODULE_STATE_LOADED;
    instance->load_time = 0;  /* TODO: 获取时间戳 */
    
    /* TODO: 解析元数据 */
    /* TODO: 创建域 */
    /* TODO: 分配资源 */
    /* TODO: 加载代码 */
    
    g_module_count++;
    
    result->status = HIC_SUCCESS;
    result->instance_id = instance->instance_id;
    strcpy(result->error_message, "");
    
    console_puts("[MODULE] Module loaded: ");
    console_puts(module_path);
    console_puts(" (ID=");
    console_putu64(instance->instance_id);
    console_puts(")\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_unload(u64 instance_id, const char* password) {
    if (instance_id == 0 || instance_id > MAX_MODULES || !password) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 1. 验证密码 */
    if (!module_verify_password(password)) {
        console_puts("[MODULE] Password verification failed\n");
        return HIC_ERROR_PERMISSION;
    }
    
    /* 2. 查找实例 */
    module_instance_t* instance = &g_module_instances[instance_id - 1];
    if (instance->state == MODULE_STATE_UNLOADED) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    /* 3. 停止模块 */
    if (instance->state == MODULE_STATE_RUNNING) {
        module_stop(instance_id);
    }
    
    /* 4. 回收资源 */
    module_reclaim_resources(instance);
    
    /* 5. 销毁域 */
    if (instance->domain_id != 0) {
        module_destroy_domain(instance->domain_id);
    }
    
    /* 6. 清除实例 */
    memzero(instance, sizeof(module_instance_t));
    g_module_count--;
    
    console_puts("[MODULE] Module unloaded: ID=");
    console_putu64(instance_id);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_start(u64 instance_id) {
    if (instance_id == 0 || instance_id > MAX_MODULES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    module_instance_t* instance = &g_module_instances[instance_id - 1];
    if (instance->state != MODULE_STATE_LOADED && 
        instance->state != MODULE_STATE_STOPPED) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* TODO: 调用模块的入口点 */
    
    instance->state = MODULE_STATE_RUNNING;
    
    console_puts("[MODULE] Module started: ID=");
    console_putu64(instance_id);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_stop(u64 instance_id) {
    if (instance_id == 0 || instance_id > MAX_MODULES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    module_instance_t* instance = &g_module_instances[instance_id - 1];
    if (instance->state != MODULE_STATE_RUNNING) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* TODO: 调用模块的停止函数 */
    
    instance->state = MODULE_STATE_STOPPED;
    
    console_puts("[MODULE] Module stopped: ID=");
    console_putu64(instance_id);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

hic_status_t module_list(module_instance_t* instances, u32 max_count, u32* count) {
    if (!instances || max_count == 0 || !count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 actual_count = 0;
    for (u32 i = 0; i < MAX_MODULES && actual_count < max_count; i++) {
        if (g_module_instances[i].state != MODULE_STATE_UNLOADED) {
            memcopy(&instances[actual_count], &g_module_instances[i], sizeof(module_instance_t));
            actual_count++;
        }
    }
    
    *count = actual_count;
    return HIC_SUCCESS;
}

hic_status_t module_get_info(u64 instance_id, hicmod_metadata_t* metadata) {
    if (instance_id == 0 || instance_id > MAX_MODULES || !metadata) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    module_instance_t* instance = &g_module_instances[instance_id - 1];
    if (instance->state == MODULE_STATE_UNLOADED) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    memcopy(metadata, &instance->metadata, sizeof(hicmod_metadata_t));
    return HIC_SUCCESS;
}

/* ============= 服务API ============= */

static hic_status_t service_init(void) {
    return module_manager_init();
}

static hic_status_t service_start(void) {
    console_puts("[MODULE] Module manager service started\n");
    return HIC_SUCCESS;
}

static hic_status_t service_stop(void) {
    console_puts("[MODULE] Module manager service stopped\n");
    return HIC_SUCCESS;
}

static hic_status_t service_cleanup(void) {
    /* 卸载所有模块 */
    for (u32 i = 0; i < MAX_MODULES; i++) {
        if (g_module_instances[i].state != MODULE_STATE_UNLOADED) {
            module_unload(i + 1, "admin123");  /* 使用临时密码 */
        }
    }
    
    return HIC_SUCCESS;
}

static hic_status_t service_get_info(char* buffer, u32 buffer_size) {
    if (!buffer || buffer_size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    const char* info = "Module Manager Service v1.0.0 - "
                       "Provides dynamic module loading and management";
    u32 len = strlen(info);
    
    if (len >= buffer_size) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(buffer, info);
    return HIC_SUCCESS;
}

/* 服务API表 */
const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

/* 服务注册函数 */
void service_register_self(void) {
    service_register("module_manager_service", &g_service_api);
}
