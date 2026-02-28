/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核静态模块系统实现
 */

#include "static_module.h"
#include "domain.h"
#include "capability.h"
#include "console.h"
#include "lib/string.h"
#include "lib/mem.h"
#include "pmm.h"

extern static_module_desc_t __static_modules_start;
extern static_module_desc_t __static_modules_end;

/**
 * 初始化静态模块系统
 */
void static_module_system_init(void)
{
    console_puts("[STATIC_MODULE] Static module system initialized\n");
}

/**
 * 加载所有静态模块
 */
int static_module_load_all(void)
{
    static_module_desc_t *module;
    int loaded_count = 0;
    int failed_count = 0;

    console_puts("[STATIC_MODULE] Loading static modules...\n");

    /* 遍历所有静态模块 */
    for (module = &__static_modules_start; module < &__static_modules_end; module++) {
        /* 检查模块描述符是否有效 */
        if (module->name[0] == '\0') {
            continue;  /* 空描述符，跳过 */
        }

        console_puts("[STATIC_MODULE] Found module: ");
        console_puts(module->name);
        console_puts("\n");

        /* 检查是否需要自动启动 */
        if (!(module->flags & STATIC_MODULE_FLAG_AUTO_START)) {
            console_puts("[STATIC_MODULE]   Skipped (auto_start not set)\n");
            continue;
        }

        /* 创建沙箱 */
        if (static_module_create_sandbox(module) != 0) {
            console_puts("[STATIC_MODULE]   Failed to create sandbox\n");
            failed_count++;
            continue;
        }

        /* 启动模块 */
        if (static_module_start(module) != 0) {
            console_puts("[STATIC_MODULE]   Failed to start\n");
            failed_count++;
            continue;
        }

        loaded_count++;
    }

    if (loaded_count == 0) {
        console_puts("[STATIC_MODULE] No static modules to load\n");
    } else {
        console_puts("[STATIC_MODULE] Loaded ");
        console_putu32((u32)loaded_count);
        console_puts(" modules\n");
    }
    
    return loaded_count;
}

/**
 * 创建静态模块的沙箱
 */
int static_module_create_sandbox(static_module_desc_t *module)
{
    domain_id_t domain;
    u64 module_size;
    phys_addr_t module_phys;
    void *module_base;
    cap_id_t caps[8];
    int cap_count = 0;
    int i;
    domain_quota_t quota;
    hic_status_t status;

    console_puts("[STATIC_MODULE] Creating sandbox for ");
    console_puts(module->name);
    console_puts("...\n");

    /* 计算模块大小 */
    module_size = (u64)((u8*)module->code_end - (u8*)module->code_start);
    if (module->data_start && module->data_end) {
        module_size += (u64)((u8*)module->data_end - (u8*)module->data_start);
    }

    /* 设置资源配额 */
    quota.max_memory = PAGE_ALIGN(module_size) + PAGE_SIZE;  /* 加一个页作为栈 */
    quota.max_threads = 1;
    quota.max_caps = 8;
    quota.cpu_quota_percent = 100;

    /* 创建域 */
    status = domain_create(DOMAIN_TYPE_PRIVILEGED, HIC_INVALID_DOMAIN, 
                           &quota, &domain);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE] Failed to create domain\n");
        return -1;
    }

    /* 为模块分配物理内存 */
    status = pmm_alloc_frames(domain, 
                               (u32)((PAGE_ALIGN(module_size) + PAGE_SIZE) / PAGE_SIZE),
                               PAGE_FRAME_PRIVILEGED, &module_phys);
    if (status != HIC_SUCCESS) {
        console_puts("[STATIC_MODULE] Failed to allocate memory\n");
        return -1;
    }

    /* 映射物理内存到虚拟地址 */
    /* TODO: 实现虚拟内存映射 */
    module_base = (void*)module_phys;

    /* 复制代码段 */
    memcopy(module_base, module->code_start, 
            (size_t)((u8*)module->code_end - (u8*)module->code_start));

    /* 复制数据段（如果有） */
    if (module->data_start && module->data_end) {
        void *data_base = (u8*)module_base + ((u8*)module->code_end - (u8*)module->code_start);
        memcopy(data_base, module->data_start,
                (size_t)((u8*)module->data_end - (u8*)module->data_start));
    }

    /* 转换能力 */
    for (i = 0; i < 8; i++) {
        if (module->capabilities[i] == 0) {
            break;
        }
        /* 简化实现：直接使用能力ID */
        caps[cap_count++] = (cap_id_t)module->capabilities[i];
    }

    /* 授予能力 */
    for (i = 0; i < cap_count; i++) {
        cap_handle_t handle;
        cap_grant(domain, caps[i], &handle);
    }

    /* 设置模块入口点 */
    /* TODO: 将入口点信息存储到域中 */

    console_puts("[STATIC_MODULE] Sandbox created successfully\n");
    return 0;
}

/**
 * 启动静态模块
 */
int static_module_start(static_module_desc_t *module)
{
    console_puts("[STATIC_MODULE] Starting module: ");
    console_puts(module->name);
    console_puts("\n");
    
    /* 调用模块的入口点函数 */
    if (module->code_start != NULL) {
        typedef void (*module_entry_t)(void);
        module_entry_t entry;
        memcopy(&entry, &module->code_start, sizeof(entry));
        
        console_puts("[STATIC_MODULE] Calling entry point at 0x");
        console_puthex64((u64)module->code_start);
        console_puts("\n");
        
        /* TODO: 在独立的线程/域上下文中调用 */
        /* 暂时直接调用（不安全，仅用于测试） */
        entry();
        
        console_puts("[STATIC_MODULE] Module entry point returned\n");
    } else {
        console_puts("[STATIC_MODULE] No entry point\n");
        return -1;
    }
    
    return 0;
}

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
