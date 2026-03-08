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
#include "boot_info.h"
#include "minimal_uart.h"
#include "module_loader.h"
#include "static_module.h"

/* 外部全局变量 */
extern boot_state_t g_boot_state;
extern hic_boot_info_t *g_boot_info;

/**
 * 内核主入口点
 * 由kernel_start调用，完成所有初始化后进入主循环
 */
void kernel_main(void *info)
{
    /* 第一条指令：调试输出 'A'，表示进入了 kernel_main */
    __asm__ volatile("outb %%al, %%dx" : : "a"('A'), "d"(0x3F8));
    
    /* ==================== 第一阶段：串口初始化（最先执行） ==================== */
    
    /* 直接用内联汇编初始化串口，避免函数调用问题 */
    __asm__ volatile(
        /* 调试：输出 '1' */
        "mov $0x31, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        
        /* 禁用中断 */
        "mov $0x3F9, %%dx\n"
        "mov $0x00, %%al\n"
        "outb %%al, %%dx\n"
        
        /* 调试：输出 '2' */
        "mov $0x32, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        
        /* 启用DLAB，设置波特率 115200 */
        "mov $0x3FB, %%dx\n"
        "mov $0x80, %%al\n"
        "outb %%al, %%dx\n"
        "mov $0x3F8, %%dx\n"
        "mov $0x01, %%al\n"
        "outb %%al, %%dx\n"
        "mov $0x3F9, %%dx\n"
        "mov $0x00, %%al\n"
        "outb %%al, %%dx\n"
        
        /* 调试：输出 '3' */
        "mov $0x33, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        
        /* 8N1配置 */
        "mov $0x3FB, %%dx\n"
        "mov $0x03, %%al\n"
        "outb %%al, %%dx\n"
        
        /* 调试：输出 '4' */
        "mov $0x34, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        
        /* 禁用FIFO */
        "mov $0x3FA, %%dx\n"
        "mov $0x00, %%al\n"
        "outb %%al, %%dx\n"
        
        /* 调试：输出 '5' */
        "mov $0x35, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        
        /* 禁用RTS/DTR */
        "mov $0x3FC, %%dx\n"
        "mov $0x00, %%al\n"
        "outb %%al, %%dx\n"
        
        /* 调试：输出 '6' */
        "mov $0x36, %%al\n"
        "mov $0x3F8, %%dx\n"
        "outb %%al, %%dx\n"
        :
        :
        : "ax", "dx", "memory"
    );
    
    /* ==================== 第二阶段：安全验证 ==================== */

    /* 转换参数类型 */
    hic_boot_info_t *boot_info = (hic_boot_info_t *)info;

    /* 【安全检查1】验证boot_info指针 */
    if (boot_info == NULL) {
        goto panic;
    }

    /* 保存启动信息（必须在最前面） */
    g_boot_state.boot_info = boot_info;
    g_boot_info = boot_info;

    /* 【安全检查2】验证boot_info魔数 */
    if (boot_info->magic != HIC_BOOT_INFO_MAGIC) {
        goto panic;
    }

    /* 【安全检查3】验证boot_info版本 */
    if (boot_info->version != HIC_BOOT_INFO_VERSION) {
        goto panic;
    }

    /* ==================== 第三阶段：串口输出 ==================== */

    console_puts("hello\n");

    /* ==================== 第三阶段：审计日志系统初始化 ==================== */
    
    /* 初始化审计日志系统（此时串口已经初始化） */
    audit_system_init();
    
    /* 分配审计日志缓冲区（从可用内存的末尾开始） */
    if (boot_info && boot_info->mem_map && boot_info->mem_map_entry_count > 0) {
        phys_addr_t audit_buffer_base = 0;
        size_t audit_buffer_size = 0;
        
        for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
            hic_mem_entry_t* entry = &boot_info->mem_map[i];
            if (entry->type == HIC_MEM_TYPE_USABLE && entry->length > audit_buffer_size) {
                audit_buffer_base = entry->base_address + entry->length - 0x10000;
                audit_buffer_size = 0x10000;
                break;
            }
        }
        
        if (audit_buffer_base != 0) {
            audit_system_init_buffer(audit_buffer_base, audit_buffer_size);
            audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, 0, 0, 0, NULL, 0, true);
        }
    }
    
    /* ==================== 第四阶段：验证启动信息 ==================== */
    
    if (!boot_info_validate(boot_info)) {
        audit_log_event(AUDIT_EVENT_EXCEPTION, 0, 0, 0, NULL, 0, false);
        console_puts("[BOOT] >>> PANIC: boot_info validation FAILED <<<\n");
        goto panic;
    }
    
    console_puts("[BOOT] >>> boot_info_validate PASSED <<<\n");
    console_puts("[BOOT] All boot information validated successfully\n");
    
    audit_log_event(AUDIT_EVENT_PMM_ALLOC, 0, 0, 0, NULL, 0, true);
    
    /* ==================== 第四阶段：核心子系统初始化 ==================== */
    
    /* 【步骤1：内存管理器初始化】 */
    console_puts("\n[BOOT] STEP 1: Initializing Memory Manager\n");
    boot_info_init_memory(boot_info);
    console_puts("[BOOT] Memory manager initialization completed\n");
    
    /* 【步骤2：能力系统初始化】 */
    console_puts("\n[BOOT] STEP 2: Initializing Capability System\n");
    capability_system_init();
    console_puts("[BOOT] Capability system initialization completed\n");
    
    /* 【步骤3：域系统初始化】 */
    console_puts("\n[BOOT] STEP 3: Initializing Domain System\n");
    domain_system_init();
    console_puts("[BOOT] Domain system initialization completed\n");
    
    /* 【步骤4：调度器初始化】 */
    console_puts("\n[BOOT] STEP 4: Initializing Scheduler\n");
    scheduler_init();
    console_puts("[BOOT] Scheduler initialization completed\n");
    
    /* 【步骤5：处理启动信息】 */
    console_puts("\n[BOOT] STEP 5: Processing Boot Information\n");
    boot_info_process(boot_info);
    console_puts("[BOOT] Boot information processing completed\n");
    
    /* ==================== 第五阶段：硬件和驱动初始化 ==================== */
    
    /* 【步骤6：硬件信息】 */
    console_puts("\n[BOOT] STEP 6: Hardware Information\n");
    console_puts("[BOOT] Hardware information provided by bootloader\n");
    
    /* 【步骤7：模块自动加载驱动】 */
    console_puts("\n[BOOT] STEP 7: Auto-loading Drivers\n");
    module_auto_load_drivers(&g_boot_state.hw.devices);
    console_puts("[BOOT] Driver auto-loading completed\n");
    
    /* ==================== 第六阶段：命令行和模块 ==================== */
    
    /* 【步骤8：解析命令行】 */
    console_puts("\n[BOOT] STEP 8: Parsing Command Line\n");
    if (boot_info->cmdline[0] != '\0') {
        console_puts("[BOOT] Command line found, parsing...\n");
        boot_info_parse_cmdline(boot_info->cmdline);
        console_puts("[BOOT] Command line parsed\n");
    } else {
        console_puts("[BOOT] No command line parameters\n");
    }
    
    /* 【步骤9：初始化模块加载器】 */
    console_puts("\n[BOOT] STEP 9: Initializing Module Loader\n");
    module_loader_init();
    console_puts("[BOOT] Module loader initialization completed\n");
    
    /* 【步骤10：启动初始模块】 */
    console_puts("\n[BOOT] STEP 10: Starting Initial Modules\n");
    if (boot_info->module_count > 0 && boot_info->modules[0].base != 0) {
        console_puts("[BOOT] Loading initial module from boot info...\n");
        u64 instance_id;
        hic_status_t status = (hic_status_t)module_load_from_memory(
            boot_info->modules[0].base,
            (u32)boot_info->modules[0].size, &instance_id);
        if (status == HIC_SUCCESS) {
            console_puts("[BOOT] Initial module loaded successfully (instance_id=");
            console_puthex64(instance_id);
            console_puts(")\n");
        } else {
            console_puts("[BOOT] Failed to load initial module\n");
        }
    } else {
        console_puts("[BOOT] No initial modules in boot info\n");
    }
    
    /* 【步骤11：加载静态模块】 */
    console_puts("\n[BOOT] STEP 11: Loading Static Modules\n");
    static_module_system_init();
    static_module_load_all();
    console_puts("[BOOT] Static modules loading completed\n");
    
    /* ==================== 第七阶段：最终化 ==================== */
    
    /* 【步骤12：标记启动完成】 */
    console_puts("\n[BOOT] STEP 12: Finalizing Boot\n");
    g_boot_state.valid = 1;
    console_puts("[BOOT] Boot state marked as VALID\n");
    
    /* 【最终报告】 */
    console_puts("\n[BOOT]========================================\n");
    console_puts("[BOOT] >>> HIC KERNEL BOOT SEQUENCE COMPLETE <<<\n");
    console_puts("[BOOT]========================================\n");
    console_puts("[BOOT] All subsystems initialized successfully:\n");
    console_puts("[BOOT]   - Boot information: VALID\n");
    console_puts("[BOOT]   - Memory manager: READY\n");
    console_puts("[BOOT]   - Capability system: READY\n");
    console_puts("[BOOT]   - Domain system: READY\n");
    console_puts("[BOOT]   - Scheduler: READY\n");
    console_puts("[BOOT]   - Privileged services: ACTIVE\n");
    console_puts("[BOOT]========================================\n");
    console_puts("[BOOT] Entering kernel main loop...\n");
    console_puts("\n");
    
    /* ==================== 第八阶段：主循环 ==================== */
    
    while (1) {
        /* 1. 检查待处理的中断 */
        if (interrupts_pending()) {
            handle_pending_interrupts();
        }
        
        /* 2. 调度器：选择下一个要运行的线程 */
        thread_id_t next_thread = scheduler_pick_next();
        if (next_thread != INVALID_THREAD) {
            context_switch_to(next_thread);
        }
        
        /* 3. 处理定时器 */
        timer_update();
        
        /* 4. 处理待处理的系统调用 */
        if (syscalls_pending()) {
            handle_pending_syscalls();
        }
        
        /* 5. 执行内核维护任务 */
        kernel_maintenance_tasks();
        
        /* 6. 进入低功耗状态等待 */
        hal_halt();
    }
    
panic:
    console_puts("\n[BOOT] >>> KERNEL PANIC! Halting system... <<<\n");
    hal_halt();
}
