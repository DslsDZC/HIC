/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统实现
 * 
 * 静态模块特性：
 * - 固定地址段，在内核内存之外 (0x200000)
 * - 代码在原位置执行（execute in place）
 * - 支持卸载，内存可重用
 * - init_launcher 用完后可释放，纳入动态池
 * 
 * 内存布局：
 * ┌─────────────────┐ 0x100000
 * │ 内核 (Core-0)    │
 * └─────────────────┘ _kernel_end
 * ┌─────────────────┐ 0x200000 (STATIC_MODULES_BASE)
 * │ 静态模块段       │
 * │ - verifier      │
 * │ - ide_driver    │
 * │ - fat32_service │
 * │ - memory_service│
 * │ - device_manager│
 * │ - security_mon  │
 * │ - init_launcher │ ← 最后加载，用完可卸载
 * └─────────────────┘ _static_modules_region_end
 *         ↓
 *   卸载后内存 → 动态池
 * 
 * 加载流程：
 * 1. 解析内核映像的 .static_modules 段
 * 2. 按优先级排序加载
 * 3. 为每个模块创建域（身份隔离）
 * 4. 分配逻辑核心
 * 5. 创建服务端点能力
 * 6. 注册到服务注册表
 * 7. 启动模块主线程
 * 
 * 卸载流程：
 * 1. 停止模块线程
 * 2. 撤销域和能力
 * 3. 从服务注册表移除
 * 4. 标记内存可重用
 * 5. 通知 PMM 回收内存
 * 
 * 信任链：
 * 内核 → verifier → ide_driver → fat32_service → init_launcher
 *      → module_manager_service（动态）→ 其他动态模块
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
#include "thread.h"
#include "lib/string.h"
#include "lib/mem.h"
#include "logical_core.h"

extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/* 模块运行时状态 */
typedef struct static_module_runtime {
    domain_id_t domain_id;              /* 分配的域 ID */
    cap_id_t endpoint_cap;              /* 服务端点能力 */
    bool running;                       /* 是否正在运行 */
    phys_addr_t code_phys;              /* 代码段加载地址（物理） */
    u32 logical_core_id;                /* 分配的逻辑核心ID */
    cap_handle_t logical_core_handle;   /* 逻辑核心能力句柄 */
    static_module_state_t state;        /* 模块状态 */
    u64 memory_size;                    /* 模块占用内存大小 */
    bool memory_freed;                  /* 内存是否已释放 */
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
 * 按优先级排序的模块索引表
 * 用于实现优先级加载顺序
 */
typedef struct {
    static_module_desc_t *module;
    u32 original_idx;       /* 原始索引，用于运行时状态映射 */
} module_sort_entry_t;

static module_sort_entry_t g_sorted_modules[MAX_STATIC_MODULES];
static u32 g_sorted_count = 0;

/**
 * 简单插入排序（按优先级升序）
 * 优先级数值越小，越先加载
 */
static void sort_modules_by_priority(void)
{
    static_module_desc_t *module;
    g_sorted_count = 0;
    
    /* 收集所有模块 */
    for (module = &__static_modules_start; module < &__static_modules_end; module++) {
        if (module->name[0] == '\0') {
            continue;
        }
        if (g_sorted_count >= MAX_STATIC_MODULES) {
            break;
        }
        g_sorted_modules[g_sorted_count].module = module;
        g_sorted_modules[g_sorted_count].original_idx = g_sorted_count;
        g_sorted_count++;
    }
    
    /* 插入排序：按优先级升序 (0=最高优先级先加载) */
    for (u32 i = 1; i < g_sorted_count; i++) {
        module_sort_entry_t key = g_sorted_modules[i];
        u32 priority = key.module->priority;
        s32 j = (s32)i - 1;
        
        /* 向后移动优先级大于 key 的元素 */
        while (j >= 0 && g_sorted_modules[j].module->priority > priority) {
            g_sorted_modules[j + 1] = g_sorted_modules[j];
            j--;
        }
        g_sorted_modules[j + 1] = key;
    }
}

/**
 * 加载所有静态模块
 * 
 * 按优先级顺序加载：
 * - CRITICAL (0): fat32, signer 等核心服务
 * - HIGH (1): 驱动服务
 * - NORMAL (2): 一般服务
 * - LOW (3): 后台服务
 */
int static_module_load_all(void)
{
    int loaded_count = 0;
    int failed_count = 0;

    console_puts("[STATIC_MODULE] Loading static modules...\n");
    console_puts("[STATIC_MODULE] Scanning .static_modules segment...\n");

    /* 按优先级排序 */
    sort_modules_by_priority();
    
    console_puts("[STATIC_MODULE] Found ");
    console_putu32(g_sorted_count);
    console_puts(" modules, sorted by priority\n");

    /* 按排序后的顺序加载 */
    for (u32 i = 0; i < g_sorted_count; i++) {
        static_module_desc_t *module = g_sorted_modules[i].module;
        u32 module_idx = i;  /* 使用排序后的索引 */

        console_puts("\n[STATIC_MODULE] ========================================\n");
        console_puts("[STATIC_MODULE] Loading module: ");
        console_puts(module->name);
        console_puts(" (priority=");
        console_putu32(module->priority);
        console_puts(")\n");
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
 * 创建静态模块的运行环境
 * 
 * 静态模块特性：
 * - 代码嵌入内核，使用固定地址段
 * - 代码在固定地址段执行（execute in place）
 * - 支持卸载，内存可重用
 * - 创建域和能力进行身份隔离
 * 
 * @param module 模块描述符
 * @param runtime_idx 运行时索引
 * @return 成功返回 0
 */
int static_module_create_sandbox_ex(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain;
    u64 code_size;
    u64 data_size = 0;
    phys_addr_t code_phys;
    domain_quota_t quota;
    hic_status_t status;
    cap_handle_t lcore_handles[1];

    /* 计算模块大小 */
    code_size = (u64)((u8*)module->code_end - (u8*)module->code_start);
    if (module->data_start && module->data_end) {
        data_size = (u64)((u8*)module->data_end - (u8*)module->data_start);
    }

    console_puts("[STATIC_MODULE]     Code size: ");
    console_putu64(code_size);
    console_puts(" bytes at fixed address\n");
    if (data_size > 0) {
        console_puts("[STATIC_MODULE]     Data size: ");
        console_putu64(data_size);
        console_puts(" bytes\n");
    }

    /* 设置资源配额（栈空间 + 数据空间） */
    quota.max_memory = PAGE_SIZE * 4 + data_size;  /* 栈空间 + 数据 */
    quota.max_threads = 4;
    quota.max_caps = 32;
    quota.cpu_quota_percent = 25;

    /* 创建域（身份隔离） */
    domain_type_t domain_type = DOMAIN_TYPE_PRIVILEGED;
    if (module->type == STATIC_MODULE_TYPE_USER) {
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

    /* 为模块分配逻辑核心 */
    status = hic_logical_core_allocate(domain, 1, 
                                       0,     /* 无特殊标志 */
                                       10,    /* 10% CPU 配额 */
                                       NULL,  /* 无亲和性限制 */
                                       lcore_handles);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to allocate logical core (status=");
        console_putu64(status);
        console_puts(")\n");
        domain_destroy(domain);
        return -1;
    }
    
    console_puts("[STATIC_MODULE]     Logical core allocated: handle=0x");
    console_puthex64(lcore_handles[0]);
    console_puts("\n");

    /* 获取逻辑核心ID */
    logical_core_id_t lcore_id = logical_core_validate_handle(domain, lcore_handles[0]);
    if (lcore_id == INVALID_LOGICAL_CORE) {
        console_puts("[STATIC_MODULE]     ERROR: Invalid logical core handle\n");
        domain_destroy(domain);
        return -1;
    }
    
    console_puts("[STATIC_MODULE]     Logical core ID: ");
    console_putu64(lcore_id);
    console_puts("\n");

    /* 静态模块：使用固定地址段（在内核之外）
     * 代码在 STATIC_MODULES_BASE 开始的固定地址段执行
     * 支持卸载，内存可重用
     */
    code_phys = (phys_addr_t)module->code_start;
    
    console_puts("[STATIC_MODULE]     Code at fixed addr 0x");
    console_puthex64(code_phys);
    console_puts(" (execute in place, unloadable)\n");

    /* 保存运行时状态 */
    g_module_runtime[runtime_idx].domain_id = domain;
    g_module_runtime[runtime_idx].code_phys = code_phys;
    g_module_runtime[runtime_idx].logical_core_id = lcore_id;
    g_module_runtime[runtime_idx].logical_core_handle = lcore_handles[0];
    g_module_runtime[runtime_idx].state = STATIC_MODULE_STATE_LOADING;
    g_module_runtime[runtime_idx].memory_size = code_size + data_size;
    g_module_runtime[runtime_idx].memory_freed = false;

    /* 记录审计日志 */
    u64 audit_data[4] = { domain, code_size, data_size, code_phys };
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

    /* 生成服务 UUID（基于名称的 UUID） */
    u8 uuid[16];
    memzero(uuid, 16);
    
    /* 使用 FNV-1a 哈希算法生成名称哈希
     * FNV-1a 是一种快速、分布良好的非加密哈希算法
     * 注意：标准 UUID v5 要求 SHA-1，但 FNV-1a 提供足够的唯一性
     * 对于操作系统内部使用的服务标识符 */
    u64 hash1 = 0, hash2 = 0;
    const u64 FNV_OFFSET = 14695981039346656037ULL;
    const u64 FNV_PRIME = 1099511628211ULL;
    
    /* 双重哈希增强碰撞抵抗 */
    hash1 = FNV_OFFSET;
    hash2 = FNV_OFFSET ^ 0xFFFFFFFFFFFFFFFFULL;
    for (u32 i = 0; module->name[i]; i++) {
        /* 第一重：标准 FNV-1a */
        hash1 ^= (u8)module->name[i];
        hash1 *= FNV_PRIME;
        /* 第二重：加入位置相关混合 */
        hash2 ^= (u8)module->name[i] * (i + 1);
        hash2 *= FNV_PRIME;
    }
    
    /* 添加版本和类型增强唯一性 */
    hash1 ^= (u64)module->version * FNV_PRIME;
    hash1 ^= (u64)module->type * FNV_PRIME;
    
    /* 使用时间戳作为额外的盐值（如果可用） */
    extern u64 hal_get_timestamp(void);
    u64 timestamp = hal_get_timestamp();
    hash2 ^= timestamp;
    hash2 *= FNV_PRIME;
    
    /* 填充 UUID */
    memcpy(uuid, &hash1, 8);
    memcpy(uuid + 8, &hash2, 8);
    
    /* 设置 UUID 版本 5（基于命名空间）和变体 */
    uuid[6] = (uuid[6] & 0x0F) | 0x50;  /* 版本 5 */
    uuid[8] = (uuid[8] & 0x3F) | 0x80;  /* 变体 1 (RFC 4122) */

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

/* ==================== 服务入口点声明 ==================== */

/* 静态服务入口函数（由各服务实现） */
extern int verifier_start(void);
extern int ide_driver_start(void);
extern int fat32_service_start(void);
extern int init_launcher_start(void);

/* 服务入口点查找表 */
typedef struct {
    const char *name;
    int (*entry_func)(void);  /* 使用正确的函数指针类型 */
} service_entry_t;

static const service_entry_t g_service_entries[] = {
    { "verifier",        verifier_start },
    { "ide_driver",      ide_driver_start },
    { "fat32_service",   fat32_service_start },
    { "init_launcher",   init_launcher_start },
    { NULL, NULL }  /* 结束标记 */
};

/* ==================== 模块启动 ==================== */

/**
 * 启动模块主线程（扩展版本）
 * 
 * 使用 thread_create_bound 创建绑定到逻辑核心的线程
 */
int static_module_start_ex(static_module_desc_t *module, u32 runtime_idx)
{
    domain_id_t domain = g_module_runtime[runtime_idx].domain_id;
    phys_addr_t code_phys = g_module_runtime[runtime_idx].code_phys;
    u32 logical_core_id = g_module_runtime[runtime_idx].logical_core_id;
    
    console_puts("[STATIC_MODULE]     Starting module in domain ");
    console_putu64(domain);
    console_puts(" on logical core ");
    console_putu64(logical_core_id);
    console_puts("\n");

    /* 查找服务入口函数 */
    virt_addr_t entry_point = 0;
    for (int i = 0; g_service_entries[i].name != NULL; i++) {
        if (strcmp(module->name, g_service_entries[i].name) == 0) {
            /* 使用 union 进行函数指针到整数的转换（GCC 扩展） */
            union {
                int (*func)(void);
                virt_addr_t addr;
            } conv;
            conv.func = g_service_entries[i].entry_func;
            entry_point = conv.addr;
            console_puts("[STATIC_MODULE]     Found entry function: ");
            console_puts(g_service_entries[i].name);
            console_puts("_start at 0x");
            console_puthex64(entry_point);
            console_puts("\n");
            break;
        }
    }

    /* 如果没找到入口函数，使用描述符中的偏移 */
    if (entry_point == 0 && code_phys != 0) {
        entry_point = (virt_addr_t)((u8*)code_phys + module->entry_offset);
        console_puts("[STATIC_MODULE]     Using default entry offset: 0x");
        console_puthex64(entry_point);
        console_puts("\n");
    }

    if (entry_point == 0) {
        console_puts("[STATIC_MODULE]     ERROR: No entry point found for service ");
        console_puts(module->name);
        console_puts("\n");
        return -1;
    }

    console_puts("[STATIC_MODULE]     Entry point: 0x");
    console_puthex64(entry_point);
    console_puts("\n");

    /* 
     * 使用 thread_create_bound 创建绑定到逻辑核心的线程
     * 这确保模块在自己的执行上下文中运行，并绑定到特定的逻辑核心
     */
    thread_id_t module_thread;
    hic_status_t status = thread_create_bound(domain, logical_core_id,
                                              (virt_addr_t)entry_point,
                                              HIC_PRIORITY_NORMAL, &module_thread);
    
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE]     ERROR: Failed to create bound module thread (status=");
        console_putu64(status);
        console_puts(")\n");
        return -1;
    }
    
    console_puts("[STATIC_MODULE]     Module thread created: ");
    console_putu64(module_thread);
    console_puts(" (bound to core ");
    console_putu64(logical_core_id);
    console_puts(")\n");
    
    /* 标记模块为运行状态 */
    g_module_runtime[runtime_idx].running = true;
    g_module_runtime[runtime_idx].state = STATIC_MODULE_STATE_RUNNING;

    /* 记录审计日志 */
    audit_log_event(AUDIT_EVENT_SERVICE_START, domain, 0, 0, NULL, 0, true);

    console_puts("[STATIC_MODULE]     Module started successfully\n");
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

/* ==================== 卸载功能 ==================== */

/**
 * 获取模块状态
 */
static_module_state_t static_module_get_state(const char *name)
{
    static_module_desc_t *module = static_module_find(name);
    if (!module) {
        return STATIC_MODULE_STATE_UNLOADED;
    }
    
    u32 idx = (u32)(module - &__static_modules_start);
    if (idx < MAX_STATIC_MODULES) {
        return g_module_runtime[idx].state;
    }
    
    return STATIC_MODULE_STATE_UNLOADED;
}

/**
 * 卸载静态模块
 * 
 * 停止模块线程，释放域和能力，内存可纳入动态池
 */
hic_status_t static_module_unload(const char *name)
{
    static_module_desc_t *module;
    u32 idx;
    static_module_runtime_t *runtime;
    
    module = static_module_find(name);
    if (!module) {
        console_puts("[STATIC_MODULE] Module not found: ");
        console_puts(name);
        console_puts("\n");
        return HIC_ERROR_NOT_FOUND;
    }
    
    idx = (u32)(module - &__static_modules_start);
    if (idx >= MAX_STATIC_MODULES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    runtime = &g_module_runtime[idx];
    
    /* 检查模块状态 */
    if (runtime->state == STATIC_MODULE_STATE_UNLOADED ||
        runtime->state == STATIC_MODULE_STATE_UNLOADED_MEM_FREED) {
        console_puts("[STATIC_MODULE] Module already unloaded: ");
        console_puts(name);
        console_puts("\n");
        return HIC_SUCCESS;
    }
    
    /* 检查是否可卸载 */
    if (!(module->flags & STATIC_MODULE_FLAG_UNLOADABLE) &&
        !(module->flags & STATIC_MODULE_FLAG_CRITICAL)) {
        /* 只有标记为可卸载或非关键模块才能卸载 */
        console_puts("[STATIC_MODULE] Module is not unloadable: ");
        console_puts(name);
        console_puts("\n");
        return HIC_ERROR_PERMISSION_DENIED;
    }
    
    /* 关键模块不允许卸载 */
    if (module->flags & STATIC_MODULE_FLAG_CRITICAL) {
        console_puts("[STATIC_MODULE] Cannot unload critical module: ");
        console_puts(name);
        console_puts("\n");
        return HIC_ERROR_PERMISSION_DENIED;
    }
    
    console_puts("[STATIC_MODULE] Unloading module: ");
    console_puts(name);
    console_puts("\n");
    
    runtime->state = STATIC_MODULE_STATE_UNLOADING;
    
    /* 步骤1: 停止模块线程 */
    if (runtime->running) {
        console_puts("[STATIC_MODULE]   Stopping module threads...\n");
        
        /* 通过逻辑核心 ID 找到并终止线程 */
        if (runtime->logical_core_id != INVALID_LOGICAL_CORE) {
            /* 查找并终止绑定到该逻辑核心的线程 */
            extern thread_id_t thread_find_by_logical_core(u32 logical_core_id);
            thread_id_t tid = thread_find_by_logical_core(runtime->logical_core_id);
            if (tid != INVALID_THREAD) {
                thread_terminate(tid);
                console_puts("[STATIC_MODULE]   Thread terminated: ");
                console_putu64(tid);
                console_puts("\n");
            }
        }
        runtime->running = false;
    }
    
    /* 步骤2: 从服务注册表移除 */
    console_puts("[STATIC_MODULE]   Unregistering service...\n");
    service_unregister_endpoint(name);
    
    /* 步骤3: 撤销能力 */
    console_puts("[STATIC_MODULE]   Revoking capabilities...\n");
    if (runtime->endpoint_cap != 0) {
        cap_revoke(runtime->endpoint_cap);
        runtime->endpoint_cap = 0;
    }
    
    /* 步骤4: 销毁域 */
    console_puts("[STATIC_MODULE]   Destroying domain...\n");
    if (runtime->domain_id != HIC_INVALID_DOMAIN) {
        domain_destroy(runtime->domain_id);
        runtime->domain_id = HIC_INVALID_DOMAIN;
    }
    
    /* 步骤5: 标记内存可重用 */
    runtime->memory_size = (u64)module->code_end - (u64)module->code_start;
    if (module->data_start && module->data_end) {
        runtime->memory_size += (u64)module->data_end - (u64)module->data_start;
    }
    
    runtime->state = STATIC_MODULE_STATE_UNLOADED;
    runtime->memory_freed = false;
    
    console_puts("[STATIC_MODULE] Module unloaded: ");
    console_puts(name);
    console_puts(", memory size: ");
    console_putu64(runtime->memory_size);
    console_puts(" bytes available for reuse\n");
    
    /* 记录审计日志 */
    u64 audit_data[4] = { (u64)name[0], (u64)name[1], (u64)name[2], (u64)name[3] };
    audit_log_event(AUDIT_EVENT_SERVICE_STOP, 0, 0, 0, audit_data, 4, true);
    
    return HIC_SUCCESS;
}

/**
 * 卸载 init_launcher（引导完成后调用）
 * 
 * init_launcher 完成引导 module_manager 后，
 * 调用此函数释放其内存，纳入动态池
 */
hic_status_t static_module_unload_init_launcher(void)
{
    console_puts("[STATIC_MODULE] Unloading init_launcher after bootstrap...\n");
    return static_module_unload("init_launcher");
}

/**
 * 释放已卸载模块的内存到动态池
 * 
 * @return 释放的字节数
 */
u64 static_module_release_memory(void)
{
    u64 total_released = 0;
    
    console_puts("[STATIC_MODULE] Releasing unloaded module memory...\n");
    
    for (u32 i = 0; i < g_sorted_count && i < MAX_STATIC_MODULES; i++) {
        static_module_runtime_t *runtime = &g_module_runtime[i];
        
        /* 查找已卸载但内存未释放的模块 */
        if (runtime->state == STATIC_MODULE_STATE_UNLOADED && 
            !runtime->memory_freed &&
            runtime->memory_size > 0) {
            
            console_puts("[STATIC_MODULE]   Releasing memory for module index ");
            console_putu32(i);
            console_puts(": ");
            console_putu64(runtime->memory_size);
            console_puts(" bytes at 0x");
            console_puthex64(runtime->code_phys);
            console_puts("\n");
            
            /* 计算页数 */
            u32 page_count = (u32)((runtime->memory_size + PAGE_SIZE - 1) / PAGE_SIZE);
            
            /* 将静态模块内存区域标记为可用 */
            /* 静态模块内存在固定地址段，需要添加到 PMM 的可用内存池 */
            phys_addr_t mem_start = runtime->code_phys;
            phys_addr_t mem_end = mem_start + runtime->memory_size;
            
            /* 验证地址范围在静态模块区域内 */
            if (mem_start >= (phys_addr_t)_static_modules_region_start &&
                mem_end <= (phys_addr_t)_static_modules_region_end) {
                
                /* 添加到 PMM 作为动态可用内存 */
                hic_status_t status = pmm_add_region(mem_start, runtime->memory_size);
                if (status == HIC_SUCCESS) {
                    console_puts("[STATIC_MODULE]   Memory added to dynamic pool\n");
                    total_released += runtime->memory_size;
                } else {
                    console_puts("[STATIC_MODULE]   WARNING: Failed to add memory to pool (status=");
                    console_putu64(status);
                    console_puts(")\n");
                }
            } else {
                console_puts("[STATIC_MODULE]   WARNING: Memory outside static module region\n");
            }
            
            runtime->state = STATIC_MODULE_STATE_UNLOADED_MEM_FREED;
            runtime->memory_freed = true;
        }
    }
    
    if (total_released > 0) {
        console_puts("[STATIC_MODULE] Total memory released: ");
        console_putu64(total_released);
        console_puts(" bytes\n");
    } else {
        console_puts("[STATIC_MODULE] No memory to release\n");
    }
    
    return total_released;
}

/**
 * 获取可释放的静态模块内存大小
 */
u64 static_module_get_reclaimable_memory(void)
{
    u64 total = 0;
    
    for (u32 i = 0; i < g_sorted_count && i < MAX_STATIC_MODULES; i++) {
        static_module_runtime_t *runtime = &g_module_runtime[i];
        
        if (runtime->state == STATIC_MODULE_STATE_UNLOADED && 
            !runtime->memory_freed &&
            runtime->memory_size > 0) {
            total += runtime->memory_size;
        }
    }
    
    return total;
}