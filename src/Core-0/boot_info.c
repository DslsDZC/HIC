/**
 * HIK内核启动信息处理实现
 * 接收并处理Bootloader传递的启动信息
 */

#include "boot_info.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "hardware_probe.h"
#include "module_loader.h"

/* 全局启动状态 */
static boot_state_t g_boot_state = {0};

/**
 * 内核入口点
 */
void kernel_entry(hik_boot_info_t* boot_info) {
    // 保存启动信息
    g_boot_state.boot_info = boot_info;
    
    // 初始化控制台（如果未初始化）
    console_init();
    
    log_info("========== HIK内核启动 ==========\n");
    log_info("版本: %s\n", HIK_VERSION);
    
    // 验证启动信息
    if (!boot_info_validate(boot_info)) {
        log_error("启动信息验证失败！\n");
        goto panic;
    }
    
    log_info("启动信息验证成功\n");
    
    // 处理启动信息
    boot_info_process(boot_info);
    
    // 运行时硬件探测
    log_info("开始运行时硬件探测...\n");
    probe_all_hardware(&g_boot_state.hw);
    
    // 初始化内存管理器
    log_info("初始化内存管理器...\n");
    boot_info_init_memory(boot_info);
    
    // 初始化ACPI
    if (boot_info->flags & HIK_BOOT_FLAG_ACPI_ENABLED) {
        log_info("初始化ACPI...\n");
        boot_info_init_acpi(boot_info);
    }
    
    // 解析命令行
    if (boot_info->cmdline[0] != '\0') {
        log_info("命令行: %s\n", boot_info->cmdline);
        boot_info_parse_cmdline(boot_info->cmdline);
    }
    
    // 初始化模块加载器
    log_info("初始化模块加载器...\n");
    module_loader_init();
    
    // 自动加载驱动
    log_info("自动加载硬件驱动...\n");
    module_auto_load_drivers(&g_boot_state.hw.devices);
    
    // 打印启动信息摘要
    boot_info_print_summary();
    
    // 标记启动完成
    g_boot_state.valid = 1;
    
    log_info("========== 内核初始化完成 ==========\n");
    
    /* 进入主循环 */
    kernel_main_loop();
    
panic:
    log_error("内核启动失败，进入紧急模式\n");
    while (1) {
        hal_halt();  /* 使用HAL接口 */
    }
}

/**
 * 验证启动信息
 */
bool boot_info_validate(hik_boot_info_t* boot_info) {
    if (!boot_info) {
        log_error("启动信息指针为空\n");
        return false;
    }
    
    /* 验证魔数 */
    if (boot_info->magic != HIK_BOOT_INFO_MAGIC) {
        log_error("启动信息魔数错误: 0x%08x\n", boot_info->magic);
        return false;
    }
    
    /* 验证版本 */
    if (boot_info->version != HIK_BOOT_INFO_VERSION) {
        log_error("启动信息版本不匹配: %u\n", boot_info->version);
        return false;
    }
    
    // 验证内存映射
    if (!boot_info->mem_map || boot_info->mem_map_entry_count == 0) {
        log_error("内存映射无效\n");
        return false;
    }
    
    // 验证内核映像
    if (!boot_info->kernel_base || boot_info->kernel_size == 0) {
        log_error("内核映像信息无效\n");
        return false;
    }
    
    return true;
}

/**
 * 处理启动信息
 */
void boot_info_process(hik_boot_info_t* boot_info) {
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
        hik_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIK_MEM_TYPE_USABLE) {
            total_usable += entry->length;
        }
    }
    log_info("可用内存: %lu MB\n", total_usable / (1024 * 1024));
    
    // 打印ACPI信息
    if (boot_info->flags & HIK_BOOT_FLAG_ACPI_ENABLED) {
        log_info("ACPI RSDP: 0x%016lx\n", (u64)boot_info->rsdp);
        if (boot_info->xsdp) {
            log_info("ACPI XSDP: 0x%016lx\n", (u64)boot_info->xsdp);
        }
    }
    
    // 打印视频信息
    if (boot_info->flags & HIK_BOOT_FLAG_VIDEO_ENABLED) {
        log_info("视频分辨率: %ux%u\n",
            boot_info->video.width,
            boot_info->video.height);
        log_info("帧缓冲区: 0x%08x, 大小: %u\n",
                boot_info->video.framebuffer_base,
                boot_info->video.framebuffer_size);
    }
    
    // 打印调试信息
    if (boot_info->flags & HIK_BOOT_FLAG_DEBUG_ENABLED) {
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
void boot_info_init_memory(hik_boot_info_t* boot_info) {
    // 使用Bootloader传递的内存映射初始化PMM
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hik_mem_entry_t* entry = &boot_info->mem_map[i];
        
        switch (entry->type) {
            case HIK_MEM_TYPE_USABLE:
                // 可用内存，添加到PMM
                pmm_add_region(entry->base, entry->length);
                break;
                
            case HIK_MEM_TYPE_RESERVED:
            case HIK_MEM_TYPE_ACPI:
            case HIK_MEM_TYPE_NVS:
            case HIK_MEM_TYPE_BOOTLOADER:
                // 保留内存，不添加到PMM
                break;
                
            case HIK_MEM_TYPE_KERNEL:
                // 内核内存，标记为已使用
                pmm_mark_used(entry->base, entry->length);
                break;
                
            default:
                break;
        }
    }
    
    // 初始化PMM
    pmm_init();
    
    log_info("内存管理器初始化完成\n");
}

/**
 * 初始化ACPI
 */
void boot_info_init_acpi(hik_boot_info_t* boot_info) {
    /* 使用Bootloader传递的ACPI RSDP */
    /* 实际实现需要解析ACPI表（MADT, DSDT等） */
    
    /* 完整实现：解析ACPI RSDP和相关表 */
    if (!boot_info || !boot_info->acpi_rsdp) {
        log_warning("ACPI RSDP指针无效\n");
        return;
    }
    
    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)boot_info->acpi_rsdp;
    
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
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)(phys_to_virt(rsdp->rsdt_address));
        boot_info_parse_acpi_tables(rsdt, ACPI_SIG_RSDT);
    } else {
        /* ACPI 2.0+: 使用XSDT */
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)(phys_to_virt(rsdp->xsdt_address));
        boot_info_parse_acpi_tables(xsdt, ACPI_SIG_XSDT);
    }
    
    /* 保存RSDP指针供后续使用 */
    boot_info->acpi_valid = true;
    
    log_info("ACPI初始化完成\n");
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
        u64 len = p - start;
        if (len >= sizeof(param)) len = sizeof(param) - 1;
        memcopy(param, start, len);
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
            u32 max_cpus = atoi(param + 8);
            log_info("限制CPU核心数: %u\n", max_cpus);
            if (max_cpus < g_boot_state.hw.cpu.logical_cores) {
                g_boot_state.hw.cpu.logical_cores = max_cpus;
            }
        } else if (strncmp(param, "mem=", 4) == 0) {
            /* 解析内存限制: mem=512M 或 mem=2G */
            u64 mem_limit = 0;
            const char* mem_str = param + 4;
            char unit = '\0';
            
            if (sscanf(mem_str, "%lu%c", &mem_limit, &unit) == 2) {
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
            
            if (sscanf(param + 8, "%[^,],%d", device, &baud) == 2) {
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
                
                g_boot_state.serial_port = port;
                g_boot_state.serial_baud = baud;
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
    log_info("\n========== 启动信息摘要 ==========\n");
    
    // Bootloader提供的信息
    log_info("Bootloader提供:\n");
    log_info("  内存映射: %lu 条目\n", 
            g_boot_state.boot_info->mem_map_entry_count);
    log_info("  ACPI: %s\n",
            (g_boot_state.boot_info->flags & HIK_BOOT_FLAG_ACPI_ENABLED) ? "是" : "否");
    log_info("  视频支持: %s\n",
            (g_boot_state.boot_info->flags & HIK_BOOT_FLAG_VIDEO_ENABLED) ? "是" : "否");
    
    // 运行时探测的信息
    log_info("\n运行时探测:\n");
    log_info("  CPU: %s\n", g_boot_state.hw.cpu.brand_string);
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
static void kernel_main_loop(void) {
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
