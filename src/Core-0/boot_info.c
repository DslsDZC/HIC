/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核启动信息处理实现
 * 
 * 本文件只负责处理 Bootloader 传递的启动信息：
 * - 验证 boot_info 结构
 * - 解析内存映射
 * - 解析 ACPI 表
 * - 解析命令行参数
 * 
 * 注意：启动流程代码应该在 main.c 中，而不是这里！
 */

#include "boot_info.h"
#include "console.h"
#include "pmm.h"
#include "string.h"
#include "hardware_probe.h"
#include "audit.h"
#include "hal.h"

#include <stdarg.h>
#include <stddef.h>

/* 全局启动状态 */
boot_state_t g_boot_state = {0};

/* 全局引导信息指针 */
hic_boot_info_t *g_boot_info = NULL;

/* ==================== 辅助函数 ==================== */

/* 简化的 sscanf 实现 */
static int simple_sscanf(const char *str, const char *fmt, ...) {
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

/* ==================== 验证接口 ==================== */

/**
 * 验证启动信息
 */
bool boot_info_validate(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Validating boot_info...\n");
    
    if (!boot_info) {
        console_puts("[BOOT] ERROR: boot_info pointer is NULL\n");
        return false;
    }

    if (boot_info->magic != HIC_BOOT_INFO_MAGIC) {
        console_puts("[BOOT] ERROR: Invalid magic: 0x");
        console_puthex32(boot_info->magic);
        console_puts(" (expected: 0x");
        console_puthex32(HIC_BOOT_INFO_MAGIC);
        console_puts(")\n");
        return false;
    }
    
    if (boot_info->version != HIC_BOOT_INFO_VERSION) {
        console_puts("[BOOT] ERROR: Version mismatch: ");
        console_putu32(boot_info->version);
        console_puts(" (expected: ");
        console_putu32(HIC_BOOT_INFO_VERSION);
        console_puts(")\n");
        return false;
    }
    
    if (!boot_info->mem_map) {
        console_puts("[BOOT] ERROR: mem_map pointer is NULL\n");
        return false;
    }
    
    if (boot_info->mem_map_entry_count == 0) {
        console_puts("[BOOT] ERROR: mem_map_entry_count is 0\n");
        return false;
    }
    
    if (!boot_info->kernel_base) {
        console_puts("[BOOT] ERROR: kernel_base is NULL\n");
        return false;
    }
    
    if (boot_info->kernel_size == 0) {
        console_puts("[BOOT] ERROR: kernel_size is 0\n");
        return false;
    }
    
    if (boot_info->entry_point == 0) {
        console_puts("[BOOT] ERROR: entry_point is 0\n");
        return false;
    }
    
    console_puts("[BOOT] boot_info validation PASSED\n");
    return true;
}

/* ==================== 处理接口 ==================== */

/**
 * 处理启动信息（打印摘要）
 */
void boot_info_process(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Processing boot information...\n");
    
    /* 打印固件信息 */
    console_puts("[BOOT] Firmware: ");
    console_puts(boot_info->firmware_type == 0 ? "UEFI" : "BIOS");
    console_puts("\n");
    
    /* 打印架构信息 */
    console_puts("[BOOT] Architecture: ");
    switch (boot_info->system.architecture) {
        case 1: console_puts("x86_64"); break;
        case 2: console_puts("ARM64"); break;
        case 3: console_puts("RISC-V64"); break;
        default: console_puts("Unknown"); break;
    }
    console_puts("\n");
    
    /* 打印内存信息 */
    console_puts("[BOOT] Memory reported by bootloader: ");
    console_putu32(boot_info->system.memory_size_mb);
    console_puts(" MB\n");
    
    /* 计算可用内存 */
    u64 total_usable = 0;
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            total_usable += entry->length;
        }
    }
    console_puts("[BOOT] Usable memory: ");
    console_putu64(total_usable / (1024 * 1024));
    console_puts(" MB\n");
    
    /* 打印 ACPI 信息 */
    if (boot_info->flags & HIC_BOOT_FLAG_ACPI_ENABLED) {
        console_puts("[BOOT] ACPI RSDP: 0x");
        console_puthex64((u64)boot_info->rsdp);
        console_puts("\n");
    }
    
    /* 打印模块信息 */
    if (boot_info->module_count > 0) {
        console_puts("[BOOT] Preloaded modules: ");
        console_putu64(boot_info->module_count);
        console_puts("\n");
        for (u64 i = 0; i < boot_info->module_count; i++) {
            console_puts("[BOOT]   Module ");
            console_putu64(i);
            console_puts(": ");
            console_puts(boot_info->modules[i].name);
            console_puts(" (");
            console_putu64(boot_info->modules[i].size);
            console_puts(" bytes)\n");
        }
    }
}

/* ==================== 内存初始化 ==================== */

/**
 * 初始化内存管理器
 */
void boot_info_init_memory(hic_boot_info_t* boot_info) {
    console_puts("[BOOT] Initializing memory manager...\n");
    
    /* 计算最大物理地址 */
    phys_addr_t max_phys_addr = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            phys_addr_t region_end = entry->base_address + entry->length;
            if (region_end > max_phys_addr) {
                max_phys_addr = region_end;
            }
        }
    }
    
    if (max_phys_addr == 0) {
        max_phys_addr = 256 * 1024 * 1024;  /* 默认 256MB */
    }
    
    console_puts("[BOOT] Max physical address: 0x");
    console_puthex64(max_phys_addr);
    console_puts("\n");
    
    /* 初始化 PMM */
    pmm_init_with_range(max_phys_addr);
    
    /* 处理内存映射 */
    u64 usable_regions = 0;
    u64 kernel_regions = 0;
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        
        switch (entry->type) {
            case HIC_MEM_TYPE_USABLE:
                pmm_add_region(entry->base_address, entry->length);
                usable_regions++;
                break;
                
            case HIC_MEM_TYPE_KERNEL:
                pmm_mark_used(entry->base_address, entry->length);
                kernel_regions++;
                break;
                
            default:
                /* 其他类型跳过 */
                break;
        }
    }
    
    console_puts("[BOOT] Memory initialized: ");
    console_putu64(usable_regions);
    console_puts(" usable regions, ");
    console_putu64(kernel_regions);
    console_puts(" kernel regions\n");
}

/* ==================== ACPI 接口 ==================== */

/**
 * 初始化 ACPI
 */
void boot_info_init_acpi(hic_boot_info_t* boot_info) {
    if (!boot_info || !boot_info->rsdp) {
        console_puts("[BOOT] No ACPI RSDP available\n");
        return;
    }
    
    acpi_rsdp_t* rsdp = (acpi_rsdp_t*)boot_info->rsdp;
    
    /* 验证 RSDP 签名 */
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        console_puts("[BOOT] Invalid RSDP signature\n");
        return;
    }
    
    console_puts("[BOOT] ACPI RSDP found at 0x");
    console_puthex64((u64)rsdp);
    console_puts(", revision ");
    console_putu32(rsdp->revision);
    console_puts("\n");
    
    /* 解析 RSDT/XSDT */
    if (rsdp->revision == 0) {
        acpi_rsdt_t* rsdt = (acpi_rsdt_t*)(hal_phys_to_virt(rsdp->rsdt_address));
        boot_info_parse_acpi_tables(rsdt, ACPI_SIG_RSDT);
    } else {
        acpi_xsdt_t* xsdt = (acpi_xsdt_t*)(hal_phys_to_virt(rsdp->xsdt_address));
        boot_info_parse_acpi_tables(xsdt, ACPI_SIG_XSDT);
    }
}

/**
 * 解析 ACPI 表
 */
void boot_info_parse_acpi_tables(void *sdt, const char *signature) {
    if (!sdt || !signature) {
        return;
    }
    
    acpi_sdt_header_t *header = (acpi_sdt_header_t *)sdt;
    
    if (memcmp(header->signature, signature, 4) != 0) {
        console_puts("[BOOT] ACPI table signature mismatch\n");
        return;
    }
    
    console_puts("[BOOT] ACPI table ");
    console_puts(signature);
    console_puts(": length=");
    console_putu32(header->length);
    console_puts("\n");
    
    u32 entry_count = 0;
    u32 entry_size = 0;
    
    /* 根据表类型确定条目大小 */
    if (memcmp(signature, ACPI_SIG_RSDT, 4) == 0) {
        entry_size = 4;  /* RSDT条目为32位指针 */
    } else if (memcmp(signature, ACPI_SIG_XSDT, 4) == 0) {
        entry_size = 8;  /* XSDT条目为64位指针 */
    }
    
    /* 计算条目数量 */
    if (entry_size > 0) {
        entry_count = (u32)((header->length - sizeof(acpi_sdt_header_t)) / entry_size);
    }
    
    console_puts("[BOOT] Found ");
    console_putu32(entry_count);
    console_puts(" ACPI tables\n");
    
    /* 遍历并解析子表 */
    for (u32 i = 0; i < entry_count; i++) {
        acpi_sdt_header_t *sub_table = NULL;
        
        if (entry_size == 4) {
            u32 *entries = (u32 *)((u8 *)sdt + sizeof(acpi_sdt_header_t));
            sub_table = (acpi_sdt_header_t *)hal_phys_to_virt(entries[i]);
        } else {
            u64 *entries = (u64 *)((u8 *)sdt + sizeof(acpi_sdt_header_t));
            sub_table = (acpi_sdt_header_t *)hal_phys_to_virt(entries[i]);
        }
        
        if (!sub_table) continue;
        
        /* 解析已知的ACPI表 */
        if (memcmp(sub_table->signature, "APIC", 4) == 0) {
            /* MADT - 多APIC描述表 */
            console_puts("[BOOT]  Found MADT (APIC) table\n");
            /* 解析MADT获取CPU和IO-APIC信息 */
            extern cpu_info_t g_cpu_info;
            u8 *madt_entries = (u8 *)sub_table + sizeof(acpi_sdt_header_t) + 8;
            u8 *madt_end = (u8 *)sub_table + sub_table->length;
            
            while (madt_entries < madt_end) {
                u8 type = madt_entries[0];
                u8 len = madt_entries[1];
                
                if (len == 0) break;
                
                if (type == 0) {
                    /* 处理器本地APIC */
                    g_cpu_info.logical_cores++;
                    console_puts("[BOOT]   CPU core detected (total: ");
                    console_putu32(g_cpu_info.logical_cores);
                    console_puts(")\n");
                } else if (type == 1) {
                    /* I/O APIC */
                    console_puts("[BOOT]   I/O APIC found\n");
                } else if (type == 2) {
                    /* 中断源覆盖 */
                    console_puts("[BOOT]   Interrupt source override\n");
                }
                
                madt_entries += len;
            }
        }
        else if (memcmp(sub_table->signature, "MCFG", 4) == 0) {
            /* MCFG - PCIe配置空间 */
            console_puts("[BOOT]  Found MCFG (PCIe config) table\n");
        }
        else if (memcmp(sub_table->signature, "HPET", 4) == 0) {
            /* HPET - 高精度事件定时器 */
            console_puts("[BOOT]  Found HPET table\n");
        }
        else if (memcmp(sub_table->signature, "FACP", 4) == 0) {
            /* FADT - 固定ACPI描述表 */
            console_puts("[BOOT]  Found FADT (FACP) table\n");
        }
        else if (memcmp(sub_table->signature, "DSDT", 4) == 0 ||
                 memcmp(sub_table->signature, "SSDT", 4) == 0) {
            /* DSDT/SSDT - 不同系统描述表 */
            console_puts("[BOOT]  Found ");
            console_puts(sub_table->signature);
            console_puts(" (ACPI definition block)\n");
        }
        else {
            /* 未知的表类型 */
            console_puts("[BOOT]  Found unknown table: ");
            console_puts(sub_table->signature);
            console_puts("\n");
        }
    }
}

/* ==================== 命令行解析 ==================== */

/**
 * 解析命令行参数
 */
void boot_info_parse_cmdline(const char* cmdline) {
    if (!cmdline || cmdline[0] == '\0') {
        return;
    }
    
    console_puts("[BOOT] Parsing command line: ");
    console_puts(cmdline);
    console_puts("\n");
    
    const char* p = cmdline;
    
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        
        const char* start = p;
        while (*p && *p != ' ') p++;

        char param[128];
        u64 len = (u64)(p - start);
        if (len >= sizeof(param)) len = sizeof(param) - 1;
        memmove(param, start, len);
        param[len] = '\0';
        
        /* 解析参数 */
        if (strcmp(param, "debug") == 0) {
            g_boot_state.debug_enabled = true;
        } else if (strcmp(param, "quiet") == 0) {
            g_boot_state.quiet_mode = true;
        } else if (strcmp(param, "recovery") == 0) {
            g_boot_state.recovery_mode = true;
        } else if (strcmp(param, "noapic") == 0) {
            g_boot_state.hw.local_irq.enabled = false;
        } else if (strcmp(param, "nosmp") == 0) {
            g_boot_state.hw.smp_enabled = false;
        }
        /* 更多参数解析... */
    }
}

/* ==================== 硬件信息处理 ==================== */

/**
 * 从 Bootloader 复制硬件探测结果
 */
void boot_info_copy_hardware_info(hic_boot_info_t* boot_info) {
    if (boot_info->hardware.hw_data != NULL && 
        boot_info->hardware.hw_size >= sizeof(hardware_probe_result_t)) {
        
        hardware_probe_result_t *bootloader_hw = (hardware_probe_result_t *)boot_info->hardware.hw_data;
        memcpy(&g_boot_state.hw, bootloader_hw, sizeof(hardware_probe_result_t));
        
        console_puts("[BOOT] Hardware info from bootloader:\n");
        console_puts("[BOOT]   CPU: ");
        console_puts(g_boot_state.hw.cpu.brand_string);
        console_puts("\n");
        console_puts("[BOOT]   Cores: ");
        console_putu64(g_boot_state.hw.cpu.logical_cores);
        console_puts(" logical, ");
        console_putu64(g_boot_state.hw.cpu.physical_cores);
        console_puts(" physical\n");
        console_puts("[BOOT]   Memory: ");
        console_putu64(g_boot_state.hw.memory.total_usable / (1024 * 1024));
        console_puts(" MB usable\n");
    } else {
        console_puts("[BOOT] No hardware info from bootloader, performing minimal detection...\n");
        
        /* 执行最小化硬件探测（机制层） */
        detect_cpu_info_minimal(&g_boot_state.hw.cpu);
        detect_memory_topology(&g_boot_state.hw.memory);
        
        /* 设置默认值 */
        g_boot_state.hw.devices.pci_count = 0;
        g_boot_state.hw.local_irq.base_address = 0xFEE00000;
        g_boot_state.hw.io_irq.base_address = 0xFEC00000;
        g_boot_state.hw.smp_enabled = (g_boot_state.hw.cpu.logical_cores > 1);
    }
}

/* ==================== 查询接口 ==================== */

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
    console_puts("\n[BOOT] ========== Boot Summary ==========\n");
    
    console_puts("[BOOT] Bootloader provided:\n");
    console_puts("[BOOT]   Memory map entries: ");
    console_putu64(g_boot_state.boot_info->mem_map_entry_count);
    console_puts("\n");
    
    console_puts("[BOOT] Detected hardware:\n");
    console_puts("[BOOT]   CPU: ");
    console_puts(g_boot_state.hw.cpu.brand_string);
    console_puts("\n");
    console_puts("[BOOT]   Memory: ");
    console_putu64(g_boot_state.hw.memory.total_usable / (1024 * 1024));
    console_puts(" MB\n");
    
    console_puts("[BOOT] ===================================\n\n");
}

/* ==================== 内核维护任务 ==================== */

/**
 * 内核维护任务
 * 
 * 执行周期性维护任务：
 * - 审计日志刷新
 * - 监控统计更新
 * - 能力清理
 */
void kernel_maintenance_tasks(void) {
    /* 周期性维护任务 */
    static u64 last_maintenance = 0;
    u64 now = hal_get_timestamp();
    
    /* 每 1 秒执行一次维护 */
    if (now - last_maintenance < 1000000000ULL) {
        return;
    }
    last_maintenance = now;
    
    /* TODO: 实现以下维护任务：
     * 1. 审计日志刷新到持久存储
     * 2. 监控统计信息更新
     * 3. 过期能力清理
     * 4. 内存碎片整理检查
     */
}