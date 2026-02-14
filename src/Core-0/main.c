/**
 * HIK内核核心入口点
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
#include "lib/console.h"

/* 内核启动信息 */
typedef struct {
    u64 magic;
    u64 total_memory;
    u64 free_memory;
    u64 num_cpus;
} kernel_info_t;

#define HIK_KERNEL_MAGIC 0x48494B4E /* "HIKN" */

/**
 * 内核主入口点
 * 由bootloader调用
 */
void kernel_main(kernel_info_t *info)
{
    if (info->magic != HIK_KERNEL_MAGIC) {
        console_puts("ERROR: Invalid kernel info magic\n");
        return;
    }
    
    console_puts("HIK Kernel (Core-0) v0.1\n");
    console_puts("Initializing Hierarchical Isolation Kernel...\n");
    
    /* 0. 初始化构建时配置 */
    console_puts("[0/8] Initializing build configuration...\n");
    build_config_init();
    build_config_load_yaml("platform.yaml");
    build_config_parse_and_validate();
    build_config_resolve_conflicts();
    build_config_generate_tables();
    
    /* 1. 初始化物理内存管理器 */
    console_puts("[1/8] Initializing Physical Memory Manager...\n");
    pmm_init();
    pmm_add_region(0x100000, info->total_memory - 0x100000);
    
    /* 2. 初始化能力系统 */
    console_puts("[2/8] Initializing Capability System...\n");
    capability_system_init();
    
    /* 3. 初始化中断控制器 */
    console_puts("[3/8] Initializing IRQ Controller...\n");
    irq_controller_init();
    
    /* 4. 初始化域管理器 */
    console_puts("[4/8] Initializing Domain Manager...\n");
    domain_system_init();
    
    /* 5. 初始化线程系统 */
    console_puts("[5/8] Initializing Thread System...\n");
    thread_system_init();
    
    /* 6. 初始化调度器 */
    console_puts("[6/8] Initializing Scheduler...\n");
    scheduler_init();
    
    /* 7. 创建Core-0自身域 */
    console_puts("[7/8] Creating Core-0 Domain...\n");
    domain_quota_t core_quota = {
        .max_memory = info->total_memory,
        .max_threads = 1024,
        .max_caps = 65536,
        .cpu_quota_percent = 100,
    };
    domain_id_t core_domain;
    domain_create(DOMAIN_TYPE_CORE, 0, &core_quota, &core_domain);
    
    console_puts("\n=== HIK Core-0 Initialization Complete ===\n");
    console_puts("Total Memory: ");
    console_putu64(info->total_memory);
    console_puts(" bytes\n");
    
    /* 进入调度循环 */
    console_puts("Entering scheduler loop...\n");
    while (1) {
        schedule();
        hal_halt();  /* 使用HAL接口 */
    }
}
