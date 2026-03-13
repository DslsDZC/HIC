/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统实现
 * 
 * 加载流程：
 * 1. 解析内核映像的 .static_modules 段
 * 2. 为每个模块创建沙箱（域）
 * 3. 复制代码/数据段到沙箱内存
 * 4. 配置页表和内存权限
 * 5. 创建初始能力空间
 * 6. 注册服务端点
 * 7. 启动模块主线程
 */

#include "include/static_module.h"
#include "include/service_registry.h"
#include "include/module_primitives.h"
#include "domain.h"
#include "capability.h"
#include "pmm.h"
#include "pagetable.h"
#include "atomic.h"
#include "audit.h"
#include "console.h"
#include "lib/string.h"
#include "lib/mem.h"

extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/* 模块运行时状态 */
typedef struct static_module_runtime {
    domain_id_t domain_id;          /* 分配的域 ID */
    cap_id_t endpoint_cap;          /* 服务端点能力 */
    bool running;                   /* 是否正在运行 */
} static_module_runtime_t;

#define MAX_STATIC_MODULES 16
static static_module_runtime_t g_module_runtime[MAX_STATIC_MODULES];

/* ==================== 初始化 ==================== */

/**
 * 初始化静态模块系统
 */
void static_module_system_init(void)
{
    memzero(g_module_runtime, sizeof(g_module_runtime));
    
    /* 初始化服务注册表 */
    service_registry_init();
    
    console_puts("[STATIC_MODULE] Static module system initialized\n");
    console_puts("[STATIC_MODULE] Service registry ready\n");
}

/* ==================== 核心加载流程 ==================== */

/**
 * 加载所有静态模块
 * 
 * 三个核心静态模块：
 * - fat32.hicmod: FAT32 文件系统服务
 * - module_signer.hicmod: 签名验证服务
 * - module_manager.hicmod: 模块管理器服务
 */
int static_module_load_all(void)
{
    static_module_desc_t *module;
    int loaded_count = 0;
    int failed_count = 0;
    u32 module_idx = 0;

    console_puts("[STATIC_MODULE] Loading static modules...\n");
    console_puts("[STATIC_MODULE] Scanning .static_modules segment...\n");

    /* 遍历所有静态模块 */
    for (module = &__static_modules_start; module < &__static_modules_end; module++, module_idx++) {
        /* 检查模块描述符是否有效 */
        if (module->name[0] == '\0') {
            continue;  /* 空描述符，跳过 */
        }

        console_puts("\n[STATIC_MODULE] ========================================\n");
        console_puts("[STATIC_MODULE] Found module: ");
        console_puts(module->name);
        console_puts("\n");
        console_puts("[STATIC_MODULE]   Type: ");
        console_putu64(module->type);
        console_puts(", Version: ");
        console_putu64(module->version);
        console_puts("\n");

        /* 检查是否需要自动启动 */
        if (!(module->flags & STATIC_MODULE_FLAG_AUTO_START)) {
            console_puts("[STATIC_MODULE]   Skipped (auto_start not set)\n");
            continue;
        }

        /* 步骤1: 创建沙箱（分配内存、配置页表） */
        console_puts("[STATIC_MODULE]   Step 1: Creating sandbox...\n");
        if (static_module_create_sandbox_ex(module, module_idx) != 0) {
            console_puts("[STATIC_MODULE]   FAILED to create sandbox\n");
            failed_count++;
            continue;
        }

        /* 步骤2: 创建初始能力空间 */
        console_puts("[STATIC_MODULE]   Step 2: Creating capability space...\n");
        if (static_module_setup_capabilities(module, module_idx) != 0) {
            console_puts("[STATIC_MODULE]   FAILED to setup capabilities\n");
            failed_count++;
            continue;
        }

        /* 步骤3: 注册服务端点 */
        console_puts("[STATIC_MODULE]   Step 3: Registering service endpoint...\n");
        if (static_module_register_service(module, module_idx) != 0) {
            console_puts("[STATIC_MODULE]   WARNING: Failed to register service endpoint\n");
            /* 非致命错误，继续 */
        }

        /* 步骤4: 启动模块主线程 */
        console_puts("[STATIC_MODULE]   Step 4: Starting module main thread...\n");
        if (static_module_start_ex(module, module_idx) != 0) {
            console_puts("[STATIC_MODULE]   FAILED to start module\n");
            failed_count++;
            continue;
        }

        g_module_runtime[module_idx].running = true;
        loaded_count++;
        
        console_puts("[STATIC_MODULE]   >>> Module loaded successfully <<<\n");
    }

    console_puts("\n[STATIC_MODULE] ========================================\n");
    console_puts("[STATIC_MODULE] Static module loading complete\n");
    console_puts("[STATIC_MODULE]   Loaded: ");
    console_putu32((u32)loaded_count);
    console_puts("\n");
    if (failed_count > 0) {
        console_puts("[STATIC_MODULE]   Failed: ");
        console_putu32((u32)failed_count);
        console_puts("\n");
    }
    console_puts("[STATIC_MODULE] ========================================\n");
    
    return loaded_count;
}

/* ==================== 沙箱创建 ==================== */

/**
 * 创建静态模块的沙箱（扩展版本）
 */
int static_module_create_sandbox_ex(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain;
    u64 code_size, data_size, total_size;
    phys_addr_t code_phys, data_phys;
    domain_quota_t quota;
    hic_status_t status;

    /* 计算模块大小 */
    code_size = (u64)((u8*)module->code_end - (u8*)module->code_start);
    data_size = 0;
    if (module->data_start && module->data_end) {
        data_size = (u64)((u8*)module->data_end - (u8*)module->data_start);
    }
    total_size = code_size + data_size;
    
    /* 对齐到页边界 */
    total_size = (total_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    console_puts("[STATIC_MODULE]     Code size: ");
    console_putu64(code_size);
    console_puts(" bytes\n");
    console_puts("[STATIC_MODULE]     Data size: ");
    console_putu64(data_size);
    console_puts(" bytes\n");
    console_puts("[STATIC_MODULE]     Total (aligned): ");
    console_putu64(total_size);
    console_puts(" bytes\n");

    /* 设置资源配额 */
    quota.max_memory = total_size + PAGE_SIZE;  /* 加一页栈 */
    quota.max_threads = 4;                       /* 最多4个线程 */
    quota.max_caps = 32;                         /* 最多32个能力 */
    quota.cpu_quota_percent = 25;                /* 25% CPU 时间 */

    /* 创建域（沙箱） */
    domain_type_t domain_type = DOMAIN_TYPE_PRIVILEGED;
    if (module->type == STATIC_MODULE_TYPE_SYSTEM) {
        domain_type = DOMAIN_TYPE_PRIVILEGED;
    } else if (module->type == STATIC_MODULE_TYPE_USER) {
        domain_type = DOMAIN_TYPE_APPLICATION;
    }

    status = domain_create(domain_type, HIC_INVALID_DOMAIN, &quota, &domain);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to create domain (status=");
        console_putu64(status);
        console_puts(")\n");
        return -1;
    }

    console_puts("[STATIC_MODULE]     Domain created: ");
    console_putu64(domain);
    console_puts("\n");

    /* 分配代码段内存 */
    status = module_memory_alloc(domain, code_size, MODULE_PAGE_CODE, (u64*)&code_phys);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to allocate code memory\n");
        domain_destroy(domain);
        return -1;
    }

    /* 分配数据段内存 */
    if (data_size > 0) {
        status = module_memory_alloc(domain, data_size, MODULE_PAGE_DATA, (u64*)&data_phys);
        if (status != HIC_SUCCESS) {
            console_puts("[STATIC_MODULE]     ERROR: Failed to allocate data memory\n");
            module_memory_free(domain, code_phys, code_size);
            domain_destroy(domain);
            return -1;
        }
    } else {
        data_phys = 0;
    }

    /* 复制代码段到沙箱内存 */
    void *code_virt = (void*)code_phys;  /* 恒等映射 */
    memcopy(code_virt, module->code_start, (size_t)code_size);
    
    console_puts("[STATIC_MODULE]     Code copied to 0x");
    console_puthex64(code_phys);
    console_puts("\n");

    /* 复制数据段到沙箱内存 */
    if (data_size > 0 && data_phys != 0) {
        void *data_virt = (void*)data_phys;
        memcopy(data_virt, module->data_start, (size_t)data_size);
        
        console_puts("[STATIC_MODULE]     Data copied to 0x");
        console_puthex64(data_phys);
        console_puts("\n");
    }

    /* 保存运行时状态 */
    g_module_runtime[runtime_idx].domain_id = domain;

    /* 记录审计日志 */
    u64 audit_data[4] = { domain, code_size, data_size, total_size };
    audit_log_event(AUDIT_EVENT_MODULE_LOAD, domain, 0, 0, audit_data, 4, true);

    return 0;
}

/* ==================== 能力设置 ==================== */

/**
 * 为模块创建初始能力空间
 */
int static_module_setup_capabilities(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain = g_module_runtime[runtime_idx].domain_id;
    cap_id_t endpoint_cap;
    cap_handle_t endpoint_handle;
    hic_status_t status;

    /* 创建服务端点能力 */
    status = cap_create_endpoint(domain, domain, &endpoint_cap);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to create endpoint capability\n");
        return -1;
    }

    /* 为模块域生成端点句柄 */
    status = cap_grant(domain, endpoint_cap, &endpoint_handle);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to grant endpoint handle\n");
        return -1;
    }

    console_puts("[STATIC_MODULE]     Endpoint cap: ");
    console_putu64(endpoint_cap);
    console_puts(", handle: 0x");
    console_puthex64(endpoint_handle);
    console_puts("\n");

    /* 保存端点能力 */
    g_module_runtime[runtime_idx].endpoint_cap = endpoint_cap;

    /* 处理模块需要的额外能力 */
    for (int i = 0; i < 8 && module->capabilities[i] != 0; i++) {
        cap_id_t req_cap = (cap_id_t)module->capabilities[i];
        cap_handle_t handle;
        
        /* 授予模块所请求的能力 */
        status = cap_grant(domain, req_cap, &handle);
        if (status == HIC_SUCCESS) {
            console_puts("[STATIC_MODULE]     Granted capability ");
            console_putu64(req_cap);
            console_puts(" -> handle 0x");
            console_puthex64(handle);
            console_puts("\n");
        }
    }

    return 0;
}

/* ==================== 服务注册 ==================== */

/**
 * 注册模块的服务端点
 */
int static_module_register_service(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain = g_module_runtime[runtime_idx].domain_id;
    cap_id_t endpoint_cap = g_module_runtime[runtime_idx].endpoint_cap;
    
    /* 根据模块名称确定端点类型 */
    endpoint_type_t type = ENDPOINT_TYPE_GENERIC;
    
    if (strcmp(module->name, "fat32") == 0 || 
        strcmp(module->name, "fat32_service") == 0) {
        type = ENDPOINT_TYPE_FILESYSTEM;
    } else if (strcmp(module->name, "module_signer") == 0 ||
               strcmp(module->name, "signer") == 0) {
        type = ENDPOINT_TYPE_SIGNER;
    } else if (strcmp(module->name, "module_manager") == 0 ||
               strcmp(module->name, "module_manager_service") == 0) {
        type = ENDPOINT_TYPE_MODULE_MGR;
    }

    /* 生成服务 UUID（简化版） */
    u8 uuid[16];
    memzero(uuid, 16);
    for (u32 i = 0; module->name[i] && i < 16; i++) {
        uuid[i % 16] ^= (u8)module->name[i];
    }

    /* 注册服务 */
    hic_status_t status = service_register_endpoint(
        module->name,
        uuid,
        domain,
        endpoint_cap,
        type,
        module->version
    );

    if (status == HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     Service registered: ");
        console_puts(module->name);
        console_puts("\n");
        return 0;
    } else {
        console_puts("[STATIC_MODULE]     ERROR: Service registration failed (status=");
        console_putu64(status);
        console_puts(")\n");
        return -1;
    }
}

/* ==================== 模块启动 ==================== */

/**
 * 启动模块主线程（扩展版本）
 */
int static_module_start_ex(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain = g_module_runtime[runtime_idx].domain_id;
    
    console_puts("[STATIC_MODULE]     Starting module in domain ");
    console_putu64(domain);
    console_puts("\n");

    /* 计算入口点地址 */
    void *entry_point = NULL;
    if (module->entry_offset != 0 && module->code_start != NULL) {
        entry_point = (void*)((u8*)module->code_start + module->entry_offset);
    } else if (module->code_start != NULL) {
        /* 默认入口点在代码段开始 */
        entry_point = module->code_start;
    }

    if (entry_point == NULL) {
        console_puts("[STATIC_MODULE]     ERROR: No entry point\n");
        return -1;
    }

    console_puts("[STATIC_MODULE]     Entry point: 0x");
    console_puthex64((u64)entry_point);
    console_puts("\n");

    /* 
     * TODO: 在实际的线程上下文中启动模块
     * 当前实现：直接调用（仅用于测试）
     */
    typedef void (*module_entry_t)(void);
    module_entry_t entry = (module_entry_t)entry_point;

    console_puts("[STATIC_MODULE]     Calling module entry point...\n");
    
    /* 暂时直接调用 - 生产环境应该在独立线程中执行 */
    entry();
    
    console_puts("[STATIC_MODULE]     Module entry point returned\n");

    /* 记录审计日志 */
    audit_log_event(AUDIT_EVENT_SERVICE_START, domain, 0, 0, NULL, 0, true);

    return 0;
}

/* ==================== 兼容旧接口 ==================== */

/**
 * 创建静态模块的沙箱（旧接口）
 */
int static_module_create_sandbox(static_module_desc_t *module)
{
    u32 runtime_idx = 0;
    /* 查找空闲槽位 */
    for (u32 i = 0; i < MAX_STATIC_MODULES; i++) {
        if (g_module_runtime[i].domain_id == 0) {
            runtime_idx = i;
            break;
        }
    }
    return static_module_create_sandbox_ex(module, runtime_idx);
}

/**
 * 启动静态模块（旧接口）
 */
int static_module_start(static_module_desc_t *module)
{
    /* 查找模块运行时 */
    for (u32 i = 0; i < MAX_STATIC_MODULES; i++) {
        if (g_module_runtime[i].running == false &&
            g_module_runtime[i].domain_id != 0) {
            return static_module_start_ex(module, i);
        }
    }
    return -1;
}

/* ==================== 查找 ==================== */

/**
 * 查找静态模块
 */
static_module_desc_t* static_module_find(const char *name)
{
    static_module_desc_t *module;

    for (module = &__static_modules_start; module < &__static_modules_end; module++) {
        if (strcmp(module->name, name) == 0) {
            return module;
        }
    }

    return NULL;
}

/* ==================== 运行时状态查询 ==================== */

/**
 * 获取模块的域 ID
 */
domain_id_t static_module_get_domain(const char *name)
{
    static_module_desc_t *module = static_module_find(name);
    if (!module) {
        return HIC_INVALID_DOMAIN;
    }
    
    /* 计算模块索引 */
    u32 idx = (u32)(module - &__static_modules_start);
    if (idx < MAX_STATIC_MODULES) {
        return g_module_runtime[idx].domain_id;
    }
    
    return HIC_INVALID_DOMAIN;
}

/**
 * 检查模块是否正在运行
 */
bool static_module_is_running(const char *name)
{
    static_module_desc_t *module = static_module_find(name);
    if (!module) {
        return false;
    }
    
    u32 idx = (u32)(module - &__static_modules_start);
    if (idx < MAX_STATIC_MODULES) {
        return g_module_runtime[idx].running;
    }
    
    return false;
}

/**
 * 获取模块的服务端点
 */
service_endpoint_t* static_module_get_service(const char *name)
{
    return service_find_by_name(name);
}

/**
 * 获取模块端点句柄
 */
hic_status_t static_module_get_endpoint_handle(const char *name, cap_handle_t *handle)
{
    return service_get_endpoint_handle(name, handle);
}