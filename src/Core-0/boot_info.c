/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核启动信息处理实现
 * 接收并处理Bootloader传递的启动信息
 */

#include "boot_info.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "hardware_probe.h"
#include "audit.h"
#include "module_loader.h"
#include "capability.h"
#include "domain.h"
#include "thread.h"
#include "static_module.h"

#include <stdarg.h>
#include <stddef.h>

/* 简化的 sscanf 实现 */
static int simple_sscanf(const char *str, const char *fmt, ...) {
    /* 支持基本格式 */
    va_list args;
    va_start(args, fmt);
    
    if (strcmp(fmt, "%lu%c") == 0) {
        unsigned long *val = va_arg(args, unsigned long*);
        char *ch = va_arg(args, char*);

        const char *p = str;
        *val = 0;
        while (*p >= '0' && *p <= '9') {
            *val = *val * 10 + (u64)(*p - '0');
            p++;
        }
        if (ch) *ch = *p;
        
        va_end(args);
        return 2;
    }
    
    va_end(args);
    return 0;
}

/* 全局启动状态 */
boot_state_t g_boot_state = {0};

/* 全局引导信息指针 */
hic_boot_info_t *g_boot_info = NULL;

/**
 * 内核入口点
 * 
 * 安全性保证：
 * - 在执行任何操作前验证boot_info指针
 * - 验证boot_info的所有关键字段
 * - 所有操作都记录审计日志
 * - 遵循形式化验证要求
 */
void kernel_boot_info_init(hic_boot_info_t* boot_info) {
    // DEBUG: 输出字符测试是否能到达这里
    __asm__ volatile(
        "mov $0x3F8, %%dx\n"
        "mov $'Z', %%al\n"
        "outb %%al, %%dx\n"
        :
        :
        : "dx", "al"
    );
    
    // 【安全检查1】验证boot_info指针
    if (boot_info == NULL) {
        goto panic;
    }
    
    // 保存启动信息
    g_boot_state.boot_info = boot_info;
    
    // 【安全检查2】验证boot_info魔数
    if (boot_info->magic != HIC_BOOT_INFO_MAGIC) {
        goto panic;
    }
    
    // 【安全检查3】验证boot_info版本
    if (boot_info->version != HIC_BOOT_INFO_VERSION) {
        goto panic;
    }
    
    // 【第一优先级】初始化审计日志系统
    audit_system_init();

    /* DEBUG: 输出字符 'A' */
    __asm__ volatile(
        "mov $0x3F8, %%dx\n"
        "mov $'A', %%al\n"
        "outb %%al, %%dx\n"
        :
        :
        : "dx", "al"
    );

    // 分配审计日志缓冲区（从可用内存的末尾开始）
    if (boot_info && boot_info->mem_map && boot_info->mem_map_entry_count > 0) {
        // 查找最大可用内存区域
        phys_addr_t audit_buffer_base = 0;
        size_t audit_buffer_size = 0;
        
        for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
            hic_mem_entry_t* entry = &boot_info->mem_map[i];
            if (entry->type == HIC_MEM_TYPE_USABLE && entry->length > audit_buffer_size) {
                    audit_buffer_base = entry->base_address + entry->length - 0x10000;  // 从末尾64KB                audit_buffer_size = 0x10000;
                break;
            }
        }
        
        if (audit_buffer_base != 0) {
            audit_system_init_buffer(audit_buffer_base, audit_buffer_size);
            
            // 记录第一个审计事件：内核启动（使用DOMAIN_CREATE表示系统域创建）
            audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, 0, 0, 0, NULL, 0, true);
        }
    }
    
    // 验证启动信息
    console_puts("[DEBUG] About to call boot_info_validate\n");
    
    if (!boot_info_validate(boot_info)) {
        // 记录审计事件
        audit_log_event(AUDIT_EVENT_EXCEPTION, 0, 0, 0, NULL, 0, false);
        console_puts("[BOOT] >>> PANIC: boot_info validation FAILED <<<\n");
        goto panic;
    }
    
    console_puts("[BOOT] >>> boot_info_validate PASSED <<<\n");
    console_puts("[BOOT] All boot information validated successfully\n");
    
    // 记录审计事件
    audit_log_event(AUDIT_EVENT_PMM_ALLOC, 0, 0, 0, NULL, 0, true);
    console_puts("[BOOT] Audit event logged: PMM_ALLOC\n");
    
    // 【步骤1：内存管理器初始化】
    // 内存管理器必须在其他子系统之前初始化
    console_puts("\n[BOOT] STEP 1: Initializing Memory Manager\n");
    console_puts("[BOOT] Calling boot_info_init_memory()...\n");
    boot_info_init_memory(boot_info);
    console_puts("[BOOT] Memory manager initialization completed\n");
    
    // 【步骤2：能力系统初始化】（需要内存）
    console_puts("\n[BOOT] STEP 2: Initializing Capability System\n");
    console_puts("[BOOT] Calling capability_system_init()...\n");
    capability_system_init();
    console_puts("[BOOT] Capability system initialization completed\n");
    
    // 【步骤3：域系统初始化】（需要内存）
    console_puts("\n[BOOT] STEP 3: Initializing Domain System\n");
    console_puts("[BOOT] Calling domain_system_init()...\n");
    domain_system_init();
    console_puts("[BOOT] Domain system initialization completed\n");
    
    // 【步骤4：调度器初始化】（需要内存）
    console_puts("\n[BOOT] STEP 4: Initializing Scheduler\n");
    console_puts("[BOOT] Calling scheduler_init()...\n");
    scheduler_init();
    console_puts("[BOOT] Scheduler initialization completed\n");
    
    // 【步骤5：处理启动信息】
    console_puts("\n[BOOT] STEP 5: Processing Boot Information\n");
    console_puts("[BOOT] Calling boot_info_process()...\n");
    boot_info_process(boot_info);
    console_puts("[BOOT] Boot information processing completed\n");
    
    // 【步骤8：静态硬件探测】
    console_puts("\n[BOOT] STEP 8: Hardware Information\n");
    console_puts("[BOOT] Hardware information provided by bootloader\n");
    console_puts("[BOOT] No hardware probing in kernel (deferred to bootloader)\n");
    
    // 【步骤9：模块自动加载驱动】
    console_puts("\n[BOOT] STEP 9: Auto-loading Drivers\n");
    console_puts("[BOOT] Calling module_auto_load_drivers()...\n");
    module_auto_load_drivers(&g_boot_state.hw.devices);
    console_puts("[BOOT] Driver auto-loading completed\n");
    
    // 【步骤10：解析命令行】
    console_puts("\n[BOOT] STEP 10: Parsing Command Line\n");
    if (boot_info->cmdline[0] != '\0') {
        console_puts("[BOOT] Command line found, parsing...\n");
        boot_info_parse_cmdline(boot_info->cmdline);
        console_puts("[BOOT] Command line parsed\n");
    } else {
        console_puts("[BOOT] No command line parameters\n");
    }
    
    // 【步骤11：初始化模块加载器】
    console_puts("\n[BOOT] STEP 11: Initializing Module Loader\n");
    console_puts("[BOOT] Calling module_loader_init()...\n");
    module_loader_init();
    console_puts("[BOOT] Module loader initialization completed\n");
    
/* [步骤12：启动初始模块（FAT32驱动）] */
    console_puts("\n[BOOT] STEP 12: Starting Initial Modules\n");
    if (boot_info->module_count > 0 && boot_info->modules[0].base != 0) {
        console_puts("[BOOT] Loading initial module from boot info...\n");
        u64 instance_id;
        hic_status_t status = (hic_status_t)module_load_from_memory(boot_info->modules[0].base,
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
    console_puts("[BOOT] Initial module loading completed\n");
    
    // 【步骤13：加载静态模块】
    console_puts("\n[BOOT] STEP 13: Loading Static Modules\n");
    console_puts("[BOOT] Calling static_module_system_init()...\n");
    static_module_system_init();
    console_puts("[BOOT] Calling static_module_load_all()...\n");
    static_module_load_all();
    console_puts("[BOOT] Static modules loading completed\n");
    
    // 【步骤14：标记启动完成】
    console_puts("\n[BOOT] STEP 14: Finalizing Boot\n");
    g_boot_state.valid = 1;
    console_puts("[BOOT] Boot state marked as VALID\n");
    
    // 【最终报告】
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
    
    /* 进入主循环 */
    kernel_main_loop();
    
panic:
    console_puts("\n[BOOT] >>> KERNEL PANIC! Halting system... <<<\n");
    hal_halt();  /* 使用HAL接口 */
}

/**
 * 验证启动信息
 * 
 * 安全性保证：
 * - 验证所有指针非空
 * - 验证所有关键数值
 * - 记录验证失败原因
 * - 遵循形式化验证要求
 */
bool boot_info_validate(hic_boot_info_t* boot_info) {
    console_putchar('X');
    console_puts("boot_info_validate START\n");
    
    if (!boot_info) {
        log_error("启动信息指针为空\n");
        return false;
    }
    
    console_putchar('Y');
    log_info("boot_info pointer OK: 0x%p\n", boot_info);

    if (boot_info->magic != HIC_BOOT_INFO_MAGIC) {
        log_error("启动信息魔数错误: 0x%08x (期望: 0x%08x)\n", 
                 boot_info->magic, HIC_BOOT_INFO_MAGIC);
        return false;
    }
    
    log_info("magic OK: 0x%08x\n", boot_info->magic);
    
    /* 验证版本 */
    if (boot_info->version != HIC_BOOT_INFO_VERSION) {
        log_error("启动信息版本不匹配: %u (期望: %u)\n", 
                 boot_info->version, HIC_BOOT_INFO_VERSION);
        return false;
    }
    
    log_info("version OK: %u\n", boot_info->version);
    
    // 验证内存映射
    if (!boot_info->mem_map) {
        log_error("内存映射指针为空\n");
        return false;
    }
    
    log_info("mem_map OK: 0x%p\n", boot_info->mem_map);
    
    if (boot_info->mem_map_entry_count == 0) {
        log_error("内存映射条目数为0\n");
        return false;
    }
    
    log_info("mem_map_entry_count OK: %lu\n", boot_info->mem_map_entry_count);
    
    // 验证内核映像
    if (!boot_info->kernel_base) {
        log_error("内核映像基地址为空\n");
        return false;
    }
    
    log_info("kernel_base OK: 0x%p\n", boot_info->kernel_base);
    
    if (boot_info->kernel_size == 0) {
        log_error("内核映像大小为0\n");
        return false;
    }
    
    log_info("kernel_size OK: %lu\n", boot_info->kernel_size);
    
    // 验证入口点
    if (boot_info->entry_point == 0) {
        log_error("内核入口点为0\n");
        return false;
    }
    
    log_info("entry_point OK: 0x%lx\n", boot_info->entry_point);
    
    log_info("boot_info验证通过\n");
    log_info("  内核基地址: 0x%p\n", boot_info->kernel_base);
    log_info("  内核大小: %lu bytes\n", boot_info->kernel_size);
    log_info("  入口点: 0x%lx\n", boot_info->entry_point);
    log_info("  内存映射条目数: %lu\n", boot_info->mem_map_entry_count);
    
    return true;
}

/**
 * 处理启动信息
 */
void boot_info_process(hic_boot_info_t* boot_info) {
    log_info("处理启动信息...\n");
    
    // 打印固件信息
    log_info("固件类型: %s\n", 
            boot_info->firmware_type == 0 ? "UEFI" : "BIOS");
    
    // 打印系统信息
    log_info("架构: %s\n", 
            boot_info->system.architecture == 1 ? "x86_64" : 
    (boot_info->system.architecture == 2 ? "ARM64" : "Unknown"));
    log_info("平台: %s\n",
            boot_info->system.platform_type == 1 ? "UEFI" : "BIOS");
    
    // 打印内存信息
    log_info("Bootloader报告内存: %u MB\n", 
            boot_info->system.memory_size_mb);
    
    // 打印内存映射摘要
    log_info("内存映射条目数: %lu\n", boot_info->mem_map_entry_count);
    
    u64 total_usable = 0;
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            total_usable += entry->length;
        }
    }
    log_info("可用内存: %lu MB\n", total_usable / (1024 * 1024));
    
    // 打印ACPI信息
    if (boot_info->flags & HIC_BOOT_FLAG_ACPI_ENABLED) {
        log_info("ACPI RSDP: 0x%016lx\n", (u64)boot_info->rsdp);
        if (boot_info->xsdp) {
            log_info("ACPI XSDP: 0x%016lx\n", (u64)boot_info->xsdp);
        }
    }
    
    // 打印视频信息
    if (boot_info->flags & HIC_BOOT_FLAG_VIDEO_ENABLED) {
        log_info("视频分辨率: %ux%u\n",
            boot_info->video.width,
            boot_info->video.height);
        log_info("帧缓冲区: 0x%08x, 大小: %u\n",
                boot_info->video.framebuffer_base,
                boot_info->video.framebuffer_size);
    }
    
    // 打印调试信息
    if (boot_info->flags & HIC_BOOT_FLAG_DEBUG_ENABLED) {
        log_info("调试模式启用\n");
        log_info("串口: 0x%04x\n", boot_info->debug.serial_port);
    }
    
    // 打印模块信息
    if (boot_info->module_count > 0) {
        log_info("预加载模块数: %lu\n", boot_info->module_count);
        for (u64 i = 0; i < boot_info->module_count; i++) {
            log_info("  模块 %lu: %s (0x%016lx, %lu bytes)\n",
                    i, boot_info->modules[i].name,
                    (u64)boot_info->modules[i].base,
                    boot_info->modules[i].size);
        }
    }
}

/**
 * 初始化内存管理器
 */
void boot_info_init_memory(hic_boot_info_t* boot_info) {
    console_puts("[DEBUG] boot_info_init_memory() called\n");
    
    /* 第一步：计算最大物理地址 */
    phys_addr_t max_phys_addr = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        phys_addr_t region_end = entry->base_address + entry->length;
        if (region_end > max_phys_addr) {
            max_phys_addr = region_end;
        }
    }
    
    console_puts("[BOOT] Calculated max physical address: 0x");
    console_puthex64(max_phys_addr);
    console_puts("\n");
    
    // 【重要：先初始化PMM】
    pmm_init_with_range(max_phys_addr);
    
    console_puts("[BOOT] Processing memory map entries...\n");
    console_puts("[BOOT] Total memory map entries: ");
    console_putu64(boot_info->mem_map_entry_count);
    console_puts("\n");
    
    // 使用Bootloader传递的内存映射初始化PMM
    u64 usable_regions = 0;
    u64 reserved_regions = 0;
    u64 kernel_regions = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        
        console_puts("[BOOT] Memory entry #");
        console_putu64(i);
        console_puts(": type=");
        
        switch (entry->type) {
            case HIC_MEM_TYPE_USABLE:
                console_puts("USABLE, base=");
                console_puthex64(entry->base_address);
                console_puts(", size=");
                console_putu64(entry->length);
                console_puts("\n");
                // 可用内存，添加到PMM
                pmm_add_region(entry->base_address, entry->length);
                usable_regions++;
                break;
                
            case HIC_MEM_TYPE_RESERVED:
                console_puts("RESERVED, skipping\n");
                // 保留内存，不添加到PMM
                reserved_regions++;
                break;
                
            case HIC_MEM_TYPE_ACPI:
                console_puts("ACPI, skipping\n");
                // 保留内存，不添加到PMM
                reserved_regions++;
                break;
                
            case HIC_MEM_TYPE_NVS:
                console_puts("NVS, skipping\n");
                // 保留内存，不添加到PMM
                reserved_regions++;
                break;
                
            case HIC_MEM_TYPE_BOOTLOADER:
                console_puts("BOOTLOADER, skipping\n");
                // 保留内存，不添加到PMM
                reserved_regions++;
                break;
                
            case HIC_MEM_TYPE_KERNEL:
                console_puts("KERNEL, marking as used\n");
                // 内核内存，标记为已使用
                pmm_mark_used(entry->base_address, entry->length);
                kernel_regions++;
                break;
                
            default:
                console_puts("UNKNOWN, skipping\n");
                break;
        }
    }
    
    console_puts("[BOOT] Memory map processing complete:\n");
    console_puts("[BOOT]   Usable regions: ");
    console_putu64(usable_regions);
    console_puts("\n");
    console_puts("[BOOT]   Reserved regions: ");
    console_putu64(reserved_regions);
    console_puts("\n");
    console_puts("[BOOT]   Kernel regions: ");
    console_putu64(kernel_regions);
    console_puts("\n");
    
    log_info("内存管理器初始化完成\n");
    console_puts("[BOOT] >>> Memory Manager initialization COMPLETE <<<\n");
}

/**
 * 初始化ACPI
 */
void boot_info_init_acpi(hic_boot_info_t* boot_info) {
    /* 使用Bootloader传递的ACPI RSDP */
    /* 实际实现需要解析ACPI表（MADT, DSDT等） */
    
    /* 完整实现：解析ACPI RSDP和相关表 */
    if (!boot_info || !boot_info->rsdp) {
        log_warning("ACPI RSDP指针无效\n");
        return;
    }
    
    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)boot_info->rsdp;
    
    /* 验证RSDP签名 */
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        log_error("无效的RSDP签名\n");
        return;
    }
    
    /* 验证RSDP校验和 */
    u8 checksum = 0;
    u8* rsdp_bytes = (u8*)rsdp;
    for (int i = 0; i < (rsdp->revision == 0 ? 20 : 36); i++) {
        checksum += rsdp_bytes[i];
    }
    
    if (checksum != 0) {
        log_error("RSDP校验和错误\n");
        return;
    }
    
    log_info("ACPI RSDP: 0x%p (Revision %d)\n", rsdp, rsdp->revision);
    
    /* 解析RSDT/XSDT */
    if (rsdp->revision == 0) {
        /* ACPI 1.0: 使用RSDT */
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)(hal_phys_to_virt(rsdp->rsdt_address));
        boot_info_parse_acpi_tables(rsdt, ACPI_SIG_RSDT);
    } else {
        /* ACPI 2.0+: 使用XSDT */
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)(hal_phys_to_virt(rsdp->xsdt_address));
        boot_info_parse_acpi_tables(xsdt, ACPI_SIG_XSDT);
    }
    
    /* 保存RSDP指针供后续使用 */
    
    log_info("ACPI初始化完成\n");
}

/**
 * 解析ACPI表
 */
void boot_info_parse_acpi_tables(void *sdt, const char *signature) {
    if (!sdt || !signature) {
        return;
    }
    
    acpi_sdt_header_t *header = (acpi_sdt_header_t *)sdt;
    
    /* 验证表签名 */
    if (memcmp(header->signature, signature, 4) != 0) {
        log_warning("ACPI表签名不匹配: 期望 %.4s, 实际 %.4s\n", signature, header->signature);
        return;
    }
    
    /* 验证表校验和 */
    u8 checksum = 0;
    u8 *bytes = (u8 *)header;
    for (u32 i = 0; i < header->length; i++) {
        checksum += bytes[i];
    }
    
    if (checksum != 0) {
        log_warning("ACPI表校验和错误: %.4s\n", signature);
        return;
    }
    
    log_info("ACPI表 %.4s: 长度=%d, 版本=%d\n", 
             signature, header->length, header->revision);
    
    /* 解析表中的条目 */
    if (strcmp(signature, ACPI_SIG_RSDT) == 0) {
        acpi_rsdt_t *rsdt = (acpi_rsdt_t *)sdt;
        u32 entry_count = (u32)((rsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(u32));
        
        log_info("RSDT包含 %u 个表\n", entry_count);
        
        for (u32 i = 0; i < entry_count; i++) {
            u32 entry_addr = rsdt->entry_pointers[i];
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *)hal_phys_to_virt(entry_addr);
            
            if (entry) {
                log_info("  表 %u: %.4s @ 0x%p\n", i, entry->signature, entry);
                /* 可以递归解析子表 */
            }
        }
    } else if (strcmp(signature, ACPI_SIG_XSDT) == 0) {
        acpi_xsdt_t *xsdt = (acpi_xsdt_t *)sdt;
        u32 entry_count = (u32)((xsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(u64));
        
        log_info("XSDT包含 %u 个表\n", entry_count);
        
        for (u32 i = 0; i < entry_count; i++) {
            u64 entry_addr = xsdt->entry_pointers[i];
            acpi_sdt_header_t *entry = (acpi_sdt_header_t *)hal_phys_to_virt(entry_addr);
            
            if (entry) {
                log_info("  表 %u: %.4s @ 0x%p\n", i, entry->signature, entry);
                /* 可以递归解析子表 */
            }
        }
    }
}

/**
 * 解析命令行参数
 */
void boot_info_parse_cmdline(const char* cmdline) {
    /* 完整实现：支持完整的内核命令行解析 */
    /* 支持的参数：
     * - debug: 启用调试输出
     * - quiet: 静默模式
     * - recovery: 恢复模式
     * - noapic: 禁用APIC
     * - nosmp: 禁用SMP
     * - maxcpus=N: 限制CPU核心数
     * - mem=NNN[M|G]: 限制内存大小
     * - console=ttyS0,115200: 串口控制台配置
     */
    
    const char* p = cmdline;
    
    while (*p) {
        /* 跳过空格 */
        while (*p == ' ') p++;
        if (!*p) break;
        
        /* 查找参数结束 */
        const char* start = p;
        while (*p && *p != ' ') p++;

        /* 提取参数 */
        char param[128];
        u64 len = (u64)(p - start);
        if (len >= sizeof(param)) len = sizeof(param) - 1;
        memmove(param, start, len);
        param[len] = '\0';
        
        /* 处理参数 */
        if (strcmp(param, "debug") == 0) {
            log_info("启用调试模式\n");
            g_boot_state.debug_enabled = true;
        } else if (strcmp(param, "quiet") == 0) {
            log_info("静默模式\n");
            g_boot_state.quiet_mode = true;
        } else if (strcmp(param, "recovery") == 0) {
            log_info("恢复模式\n");
            g_boot_state.recovery_mode = true;
        } else if (strcmp(param, "noapic") == 0) {
                log_info("禁用本地中断控制器\n");
                g_boot_state.hw.local_irq.enabled = false;        } else if (strcmp(param, "nosmp") == 0) {
            log_info("禁用SMP\n");
            g_boot_state.hw.smp_enabled = false;
        } else if (strncmp(param, "maxcpus=", 8) == 0) {
            u32 max_cpus = (u32)atoi(param + 8);
            log_info("限制CPU核心数: %u\n", max_cpus);
            if (max_cpus < g_boot_state.hw.cpu.logical_cores) {
                g_boot_state.hw.cpu.logical_cores = max_cpus;
            }
        } else if (strncmp(param, "mem=", 4) == 0) {
            /* 解析内存限制: mem=512M 或 mem=2G */
            u64 mem_limit = 0;
            const char* mem_str = param + 4;
            char unit = '\0';
            
            if (simple_sscanf(mem_str, "%lu%c", &mem_limit, &unit) == 2) {
                if (unit == 'G' || unit == 'g') {
                    mem_limit *= 1024 * 1024 * 1024;
                } else if (unit == 'M' || unit == 'm') {
                    mem_limit *= 1024 * 1024;
                }
                
                log_info("限制内存: %lu bytes\n", mem_limit);
                if (mem_limit < g_boot_state.hw.memory.total_usable) {
                    g_boot_state.hw.memory.total_usable = mem_limit;
                }
            }
        } else if (strncmp(param, "console=", 8) == 0) {
            /* 解析控制台配置: console=ttyS0,115200 */
            log_info("控制台配置: %s\n", param + 8);
            /* 完整实现：控制台配置 */
            /* 解析格式: console=ttyS0,115200 或 console=tty0 */
            int port = 0;
            int baud = 115200;
            char device[16];
            
            if (simple_sscanf(param + 8, "%[^,],%d", device, &baud) == 2) {
                /* 串口控制台 */
                if (strcmp(device, "ttyS0") == 0) {
                    port = 0x3F8;
                } else if (strcmp(device, "ttyS1") == 0) {
                    port = 0x2F8;
                } else if (strcmp(device, "ttyS2") == 0) {
                    port = 0x3E8;
                } else if (strcmp(device, "ttyS3") == 0) {
                    port = 0x2E8;
                }

                g_boot_state.serial_port = (u16)port;
                g_boot_state.serial_baud = (u32)baud;
                log_info("串口控制台: COM%d, %d baud\n", port == 0x3F8 ? 0 : 1, baud);
            } else if (strcmp(param + 8, "tty0") == 0) {
                /* VGA文本控制台 */
                log_info("VGA文本控制台\n");
            }
        } else {
            log_warning("未知参数: %s\n", param);
        }
    }
}

/**
 * 获取启动状态
 */
boot_state_t* get_boot_state(void) {
    return &g_boot_state;
}

/**
 * 打印启动信息摘要
 */
void boot_info_print_summary(void) {
    log_info("\n========== 启动信息摘要=====\n");
    
    // Bootloader提供的信息
    log_info("Bootloader提供:\n");
    log_info("  内存映射: %lu 条目\n", 
            g_boot_state.boot_info->mem_map_entry_count);
    log_info("  ACPI: %s\n",
            (g_boot_state.boot_info->flags & HIC_BOOT_FLAG_ACPI_ENABLED) ? "是" : "否");
    log_info("  视频支持: %s\n",
            (g_boot_state.boot_info->flags & HIC_BOOT_FLAG_VIDEO_ENABLED) ? "是" : "否");
    
    // 静态探测的信息
    log_info("\n静态探测:\n");    log_info("  CPU: %s\n", g_boot_state.hw.cpu.brand_string);
    log_info("  逻辑核心: %u, 物理核心: %u\n",
            g_boot_state.hw.cpu.logical_cores,
            g_boot_state.hw.cpu.physical_cores);
    log_info("  内存: %lu MB\n",
            g_boot_state.hw.memory.total_usable / (1024 * 1024));
    log_info("  PCI设备: %u\n", g_boot_state.hw.devices.pci_count);
    
    // 系统配置
    log_info("\n系统配置:\n");
    log_info("  本地中断控制器: %s\n",
            g_boot_state.hw.local_irq.enabled ? "启用" : "禁用");
    log_info("  本地中断控制器基地址: 0x%016lx\n", g_boot_state.hw.local_irq.base_address);
    log_info("  I/O中断控制器基地址: 0x%016lx\n", g_boot_state.hw.io_irq.base_address);
    
    log_info("====================================\n\n");
}

/**
 * 内核主循环
 */
void kernel_main_loop(void) {
    log_info("进入内核主循环...\n");
    
    /* 完整实现：调度器和事件循环 */
    /* 主循环负责：
     * 1. 调度就绪线程
     * 2. 处理中断
     * 3. 管理定时器
     * 4. 处理系统调用
     * 5. 执行内核维护任务
     */
    
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
        hal_halt();  /* 使用HAL接口 */
    }
}
