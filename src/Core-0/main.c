/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核核心入口点
 * 遵循三层模型文档：Core-0层作为系统仲裁者
 */

#include "types.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "thread.h"
#include "irq.h"
#include "syscall.h"
#include "build_config.h"
#include "runtime_config.h"
#include "audit.h"
#include "formal_verification.h"
#include "lib/console.h"

/* 内核启动信息 */
typedef struct {
    u64 magic;
    u64 total_memory;
    u64 free_memory;
    u64 num_cpus;
} kernel_info_t;

#define HIC_KERNEL_MAGIC 0x48494B4E /* "HICN" */

/**
 * 内核主入口点
 * 由bootloader调用
 */
void kernel_main(kernel_info_t *info)
{
    if (info->magic != HIC_KERNEL_MAGIC) {
        console_puts("ERROR: Invalid kernel info magic\n");
        return;
    }
    
    console_puts("HIC Kernel (Core-0) v0.1\n");
    console_puts("Initializing Hierarchical Isolation Core...\n");
    
    /* 0. 初始化运行时配置系统（必须在最前面） */
    console_puts("[0/11] Initializing runtime configuration...\n");
    runtime_config_init();
    runtime_config_load_from_bootinfo();
    
    /* 根据配置决定是否打印详细日志 */
    if (g_runtime_config.enable_verbose) {
        runtime_config_print();
    }
    
    /* 初始化审计日志系统（必须在最前面） */
    console_puts("[1/11] Initializing Audit System...\n");
    audit_system_init();
    
    /* 初始化形式化验证系统（在审计系统之后） */
    console_puts("[2/11] Initializing Formal Verification...\n");
    fv_init();
    
    /* 3. 初始化构建时配置 */
    console_puts("[3/11] Initializing build configuration...\n");
    build_config_init();
    build_config_load_yaml("platform.yaml");
    build_config_parse_and_validate();
    build_config_resolve_conflicts();
    build_config_generate_tables();
    
    /* 1. 初始化物理内存管理器 */
    console_puts("[4/11] Initializing Physical Memory Manager...\n");
    pmm_init();
    pmm_add_region(0x100000, info->total_memory - 0x100000);
    
    /* 初始化审计日志缓冲区 */
    phys_addr_t audit_buffer_base = 0;
    size_t audit_buffer_size = 0x100000;  /* 1MB审计日志缓冲区 */
    pmm_alloc_frames(HIC_DOMAIN_CORE, audit_buffer_size / PAGE_SIZE, 
                     PAGE_FRAME_CORE, &audit_buffer_base);
    audit_system_init_buffer(audit_buffer_base, audit_buffer_size);
    
    /* 2. 初始化能力系统 */
    console_puts("[5/11] Initializing Capability System...\n");
    capability_system_init();
    
    /* 3. 初始化中断控制器 */
    console_puts("[6/11] Initializing IRQ Controller...\n");
    irq_controller_init();
    
    /* 4. 初始化域管理器 */
    console_puts("[7/11] Initializing Domain Manager...\n");
    domain_system_init();
    
    /* 5. 初始化线程系统 */
    console_puts("[8/11] Initializing Thread System...\n");
    thread_system_init();
    
    /* 6. 初始化调度器 */
    console_puts("[9/11] Initializing Scheduler...\n");
    scheduler_init();
    
    /* 7. 创建Core-0自身域 */
    console_puts("[10/11] Creating Core-0 Domain...\n");
    domain_quota_t core_quota = {
        .max_memory = info->total_memory,
        .max_threads = g_runtime_config.max_threads,
        .max_caps = g_runtime_config.max_capabilities,
        .cpu_quota_percent = 100,
    };
    domain_id_t core_domain;
    domain_create(DOMAIN_TYPE_CORE, 0, &core_quota, &core_domain);
    
    console_puts("\n=== HIC Core-0 Initialization Complete ===\n");
    console_puts("Total Memory: ");
    console_putu64(info->total_memory);
    console_puts(" bytes\n");
    console_puts("Audit Buffer: 0x");
    console_puthex64(audit_buffer_base);
    console_puts("\n");
    
    /* 根据配置决定是否打印运行时配置 */
    if (g_runtime_config.enable_debug) {
        runtime_config_print();
    }
    
    /* 进入调度循环 */
    console_puts("[11/11] Entering scheduler loop...\n");
    while (1) {
        schedule();
        hal_halt();  /* 使用HAL接口 */
    }
}
