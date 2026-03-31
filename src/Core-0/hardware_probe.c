/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件探测 - 机制层实现
 * 
 * 本文件仅包含机制层原语实现，不包含策略决策。
 * 策略层实现已移动到 Privileged-1/hardware_probe_service/
 * 
 * 设计原则：
 * - 机制层只提供内存拓扑探测（PMM初始化必需）
 * - 机制层提供PCI/CPUID访问原语
 * - 完整硬件探测由策略层执行
 */

#include "hardware_probe.h"
#include "hal.h"
#include "boot_info.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 全局硬件信息变量 */
cpu_info_t g_cpu_info;

/* FSGSBASE 支持标志（用于 syscall 快速路径） */
bool g_use_fsgsbase = false;

/* 全局内存拓扑缓存 */
static memory_topology_t g_memory_topology;
static bool g_memory_topology_valid = false;

/* ==================== 机制层：PCI配置空间访问原语 ==================== */

/**
 * 读取PCI配置空间（机制层）
 */
u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (1U << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                  ((u32)function << 8) | (offset & 0xFC);
    
    /* 使用I/O端口访问PCI配置空间 */
    hal_outb(0xCF8, (u8)(address >> 8));
    hal_outb(0xCF9, (u8)address);
    
    return (u32)hal_inb(0xCFC) | 
           ((u32)hal_inb((u16)(0xCFC + 1)) << 8) |
           ((u32)hal_inb((u16)(0xCFC + 2)) << 16) | 
           ((u32)hal_inb((u16)(0xCFC + 3)) << 24);
}

/**
 * 写入PCI配置空间（机制层）
 */
void pci_write_config(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    u32 address = (1U << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                  ((u32)function << 8) | (offset & 0xFC);
    
    hal_outb(0xCF8, (u8)(address >> 8));
    hal_outb(0xCF9, (u8)address);
    
    hal_outb(0xCFC, (u8)(value & 0xFF));
    hal_outb((u16)(0xCFC + 1), (u8)((value >> 8) & 0xFF));
    hal_outb((u16)(0xCFC + 2), (u8)((value >> 16) & 0xFF));
    hal_outb((u16)(0xCFC + 3), (u8)((value >> 24) & 0xFF));
}

/**
 * 读取PCI配置字节（机制层）
 */
u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset) {
    u32 value = pci_read_config(bus, device, function, offset & 0xFC);
    return (u8)((value >> ((offset & 3) * 8)) & 0xFF);
}

/* ==================== 机制层：CPUID访问原语 ==================== */

/**
 * 执行CPUID指令（机制层）
 */
void cpuid_execute(u32 leaf, u32 subleaf, u32* eax, u32* ebx, u32* ecx, u32* edx) {
    __asm__ volatile (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf), "c" (subleaf)
    );
}

/* ==================== 机制层：内存拓扑探测 ==================== */

/**
 * 探测内存拓扑（机制层）
 * 
 * 这是PMM初始化唯一必需的探测功能
 */
void detect_memory_topology(memory_topology_t* topo) {
    extern boot_state_t g_boot_state;
    hic_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!topo) {
        return;
    }
    
    /* 清零输出 */
    memzero(topo, sizeof(memory_topology_t));
    
    if (!boot_info || !boot_info->mem_map || boot_info->mem_map_entry_count == 0) {
        console_puts("[HW_PROBE] WARNING: No memory map available, using defaults\n");
        
        /* 使用默认值 */
        topo->region_count = 1;
        topo->regions[0].base = 0x100000;
        topo->regions[0].size = 0x3FF00000;  /* 假设1GB */
        topo->total_usable = topo->regions[0].size;
        topo->total_physical = topo->regions[0].size + 0x100000;
        return;
    }
    
    /* 处理Bootloader提供的内存映射 */
    topo->region_count = 0;
    topo->total_usable = 0;
    topo->total_physical = 0;
    
    u64 max_region_count = sizeof(topo->regions) / sizeof(topo->regions[0]);
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count && i < max_region_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        
        if (topo->region_count >= max_region_count) {
            console_puts("[HW_PROBE] WARNING: Memory region count exceeded\n");
            break;
        }
        
        mem_region_t* region = &topo->regions[topo->region_count];
        region->base = entry->base_address;
        region->size = entry->length;
        
        /* 统计可用内存 */
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            topo->total_usable += entry->length;
        }
        
        /* 计算总物理内存 */
        u64 region_end = entry->base_address + entry->length;
        if (region_end > topo->total_physical) {
            topo->total_physical = region_end;
        }
        
        topo->region_count++;
    }
    
    console_puts("[HW_PROBE] Memory: total=");
    console_putu64(topo->total_physical / (1024 * 1024));
    console_puts("MB, usable=");
    console_putu64(topo->total_usable / (1024 * 1024));
    console_puts("MB, regions=");
    console_putu32(topo->region_count);
    console_puts("\n");
}

/* ==================== 机制层：最小CPU探测 ==================== */

/**
 * 探测最小CPU信息（机制层）
 * 
 * 仅获取启动必需的信息：
 * - 逻辑核心数
 * - 物理核心数
 * - 品牌字符串
 * 
 * 完整探测由策略层完成
 */
void detect_cpu_info_minimal(cpu_info_t* cpu) {
    if (!cpu) {
        return;
    }
    
    memzero(cpu, sizeof(cpu_info_t));
    
    u32 eax, ebx, ecx, edx;
    
    /* 获取厂商ID (Leaf 0) */
    cpuid_execute(0, 0, &eax, &ebx, &ecx, &edx);
    cpu->vendor_id[0] = ebx;
    cpu->vendor_id[1] = edx;
    cpu->vendor_id[2] = ecx;
    
    /* 获取版本信息 (Leaf 1) */
    cpuid_execute(1, 0, &eax, &ebx, &ecx, &edx);
    cpu->version = eax;
    cpu->family = (u8)((eax >> 8) & 0xF);
    cpu->model = (u8)((eax >> 4) & 0xF);
    cpu->stepping = (u8)(eax & 0xF);
    cpu->feature_flags[0] = edx;
    cpu->feature_flags[1] = ecx;
    
    /* 获取品牌字符串 (Leaf 0x80000000-0x80000004) */
    u32 max_ext;
    cpuid_execute(0x80000000, 0, &max_ext, &ebx, &ecx, &edx);
    
    if (max_ext >= 0x80000004) {
        u32* brand = (u32*)cpu->brand_string;
        cpuid_execute(0x80000002, 0, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid_execute(0x80000003, 0, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid_execute(0x80000004, 0, &brand[8], &brand[9], &brand[10], &brand[11]);
        cpu->brand_string[48] = '\0';
    }
    
    /* 获取核心数 (Leaf 0xB) */
    if (max_ext >= 0x0B) {
        cpuid_execute(0x0B, 0, &eax, &ebx, &ecx, &edx);
        cpu->logical_cores = (ebx & 0xFFFF);
        
        cpuid_execute(0x0B, 1, &eax, &ebx, &ecx, &edx);
        cpu->physical_cores = (ebx & 0xFFFF);
        
        if (cpu->physical_cores == 0) {
            cpu->physical_cores = cpu->logical_cores;
        }
    } else {
        /* 默认单核 */
        cpu->logical_cores = 1;
        cpu->physical_cores = 1;
    }
    
    console_puts("[HW_PROBE] CPU: ");
    console_puts(cpu->brand_string);
    console_puts(" (");
    console_putu32(cpu->logical_cores);
    console_puts(" logical, ");
    console_putu32(cpu->physical_cores);
    console_puts(" physical)\n");
    
    /* 检测 FSGSBASE 支持 (CPUID Leaf 7, EBX bit 0) */
    cpu->has_fsgsbase = cpu_has_fsgsbase();
    if (cpu->has_fsgsbase) {
        console_puts("[HW_PROBE] FSGSBASE: supported (fast FS/GS base access)\n");
    } else {
        console_puts("[HW_PROBE] FSGSBASE: not supported (using MSR)\n");
    }
}

/**
 * 检测 CPU 是否支持 FSGSBASE 指令
 * 
 * 架构特定实现：
 * - x86_64: CPUID Leaf 7, EBX bit 0
 * - ARM64: 不适用，返回 false
 * - RISC-V: 不适用，返回 false
 * 
 * 注意：架构特定实现在 arch/<arch>/hardware_probe_arch.c 中
 */
bool cpu_has_fsgsbase(void);

/* ==================== 机制层：初始化 ==================== */

/**
 * 初始化硬件探测机制层
 */
void hardware_probe_mechanism_init(void) {
    /* 清零CPU信息缓存 */
    memzero(&g_cpu_info, sizeof(cpu_info_t));
    
    /* 执行最小探测：仅内存拓扑 */
    detect_memory_topology(&g_memory_topology);
    g_memory_topology_valid = true;
    
    /* 检测 FSGSBASE 支持并设置全局标志 */
    g_use_fsgsbase = cpu_has_fsgsbase();
    
    console_puts("[HW_PROBE] Mechanism layer initialized\n");
}

/* ==================== 供策略层调用的接口 ==================== */

/**
 * 获取已探测的内存拓扑（策略层调用）
 */
const memory_topology_t* hardware_probe_get_memory(void) {
    if (!g_memory_topology_valid) {
        detect_memory_topology(&g_memory_topology);
        g_memory_topology_valid = true;
    }
    return &g_memory_topology;
}

/**
 * 获取全局CPU信息缓存（策略层调用）
 */
cpu_info_t* hardware_probe_get_cpu_info(void) {
    return &g_cpu_info;
}