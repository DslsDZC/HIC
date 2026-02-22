/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/*
 * HIC内核静态硬件探测实现
 * 提供跨架构的静态硬件探测接口
 */

#include "hardware_probe.h"
#include "string.h"
#include "console.h"
#include "hal.h"
#include "boot_info.h"

/* 前向声明 */
static inline u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset);
static inline u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset);
static inline void pci_write_config(u8 bus, u8 device, u8 function, u8 offset, u32 value);

/* PCI配置空间访问函数 */
static inline u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (1U << 31) | (bus << 16) | (device << 11) |
                  (function << 8) | (offset & 0xFC);
    hal_outb(0xCF8, (u8)(address >> 8));
    hal_outb(0xCF9, (u8)address);
    return hal_inb(0xCFC) | (hal_inb((u16)(0xCFC + 1)) << 8) |
           (hal_inb((u16)(0xCFC + 2)) << 16) | (hal_inb((u16)(0xCFC + 3)) << 24);
}

static inline u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset) {
    u32 value = pci_read_config(bus, device, function, offset & 0xFC);
    return (value >> ((offset & 3) * 8)) & 0xFF;
}

static inline void pci_write_config(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    u32 address = (1U << 31) | (bus << 16) | (device << 11) |
                  (function << 8) | (offset & 0xFC);
    hal_outb(0xCF8, (u8)(address >> 8));
    hal_outb(0xCF9, (u8)address);
    hal_outb(0xCFC, (u8)(value & 0xFF));
    hal_outb((u16)(0xCFC + 1), (u8)((value >> 8) & 0xFF));
    hal_outb((u16)(0xCFC + 2), (u8)((value >> 16) & 0xFF));
    hal_outb((u16)(0xCFC + 3), (u8)((value >> 24) & 0xFF));
}

/* 外部变量 */
extern boot_state_t g_boot_state;

/* 辅助函数：数字转字符串 */
static void u16_to_str(u16 value, char* buf, u64 buf_size) {
    if (buf_size == 0) return;
    
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    char temp[8];
    u64 i = 0;
    while (value > 0 && i < sizeof(temp) - 1) {
        temp[i++] = '0' + (value % 16);
        value /= 16;
    }
    
    u64 len = i;
    for (u64 j = 0; j < len && j < buf_size - 1; j++) {
        buf[j] = temp[len - 1 - j];
    }
    buf[len < buf_size ? len : buf_size - 1] = '\0';
}

/* 辅助函数：字符串追加 */
static u64 append_str(char* dest, u64 dest_size, u64 offset, const char* src) {
    if (!dest || !src || offset >= dest_size) return 0;
    
    u64 i = 0;
    while (src[i] && offset + i < dest_size - 1) {
        dest[offset + i] = src[i];
        i++;
    }
    dest[offset + i] = '\0';
    return i;
}

/* 辅助函数：x86_64 CPUID指令 */
static inline void cpuid(u32 leaf, u32 subleaf, u32* eax, u32* ebx, u32* ecx, u32* edx) {
    __asm__ volatile (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf), "c" (subleaf)
    );
}

/* 辅助函数：格式化设备名称 */
static void format_device_name(char* dest, u64 dest_size, const char* vendor, const char* device, u8 bus, u8 dev, u8 func, u16 vid, u16 did) {
    if (!dest || dest_size == 0) return;
    
    u64 offset = 0;
    
    if (vendor && device) {
        offset += append_str(dest, dest_size, offset, vendor);
        offset += append_str(dest, dest_size, offset, " ");
        offset += append_str(dest, dest_size, offset, device);
        offset += append_str(dest, dest_size, offset, " (PCI ");
        
        char buf[4];
        u16_to_str(bus, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ":");
        
        u16_to_str(dev, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ".");
        
        u16_to_str(func, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ")");
    } else {
        offset += append_str(dest, dest_size, offset, "PCI ");
        
        char buf[4];
        u16_to_str(bus, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ":");
        
        u16_to_str(dev, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ".");
        
        u16_to_str(func, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, " (");
        
        u16_to_str(vid, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ":");
        
        u16_to_str(did, buf, sizeof(buf));
        offset += append_str(dest, dest_size, offset, buf);
        offset += append_str(dest, dest_size, offset, ")");
    }
}

/**
 * 探测CPU信息
 */
void detect_cpu_info(cpu_info_t* info) {
    u32 eax, ebx, ecx, edx;
    
    // 获取厂商ID和版本信息
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    info->vendor_id[0] = ebx;
    info->vendor_id[1] = edx;
    info->vendor_id[2] = ecx;
    
    // 获取版本信息
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    info->version = eax;
    info->stepping = eax & 0xF;
    info->model = (eax >> 4) & 0xF;
    info->family = (eax >> 8) & 0xF;
    info->feature_flags[0] = ecx;
    info->feature_flags[1] = edx;
    
    // 获取扩展特性
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    info->feature_flags[2] = ecx;
    info->feature_flags[3] = edx;
    
    // 获取缓存行大小
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000006) {
        cpuid(0x80000006, 0, &eax, &ebx, &ecx, &edx);
        info->cache_line_size = ecx & 0xFF;
    }
    
    // 获取品牌字符串
    if (eax >= 0x80000004) {
        u32* brand = (u32*)info->brand_string;
        cpuid(0x80000002, 0, &eax, &ebx, &ecx, &edx);
        brand[0] = eax; brand[1] = ebx; brand[2] = ecx; brand[3] = edx;
        cpuid(0x80000003, 0, &eax, &ebx, &ecx, &edx);
        brand[4] = eax; brand[5] = ebx; brand[6] = ecx; brand[7] = edx;
        cpuid(0x80000004, 0, &eax, &ebx, &ecx, &edx);
        brand[8] = eax; brand[9] = ebx; brand[10] = ecx; brand[11] = edx;
    }
    
    // 获取逻辑核心数
    info->logical_cores = (ebx >> 16) & 0xFF;
    
    // 获取物理核心数（需要高级信息）
    if (eax >= 0x80000008) {
        cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);
        info->physical_cores = (ecx & 0xFF) + 1;
    } else {
        // 估算：逻辑核心数 / 超线程
        if (info->feature_flags[1] & (1 << 28)) { // HTT支持
            info->physical_cores = info->logical_cores / 2;
        } else {
            info->physical_cores = info->logical_cores;
        }
    }
    
    log_info("CPU: %s\n", info->brand_string);
    log_info("  逻辑核心: %u, 物理核心: %u\n", 
             info->logical_cores, info->physical_cores);
    log_info("  缓存行大小: %u bytes\n", info->cache_line_size);
}

/**
 * 探测内存拓扑
 */
void detect_memory_topology(memory_topology_t* topo) {
    // 从Bootloader获取内存映射信息
    hic_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!boot_info || !boot_info->mem_map || boot_info->mem_map_entry_count == 0) {
        log_warning("无法获取内存映射信息，使用默认值\n");
        
        // 使用默认值
        topo->region_count = 1;
        topo->regions[0].base = 0x100000;
        topo->regions[0].size = 0x3FF00000;  // 假设1GB
        topo->total_usable = topo->regions[0].size;
        topo->total_physical = topo->regions[0].size + 0x100000;
        
        log_info("内存: 总计 %lu MB, 可用 %lu MB\n",
                 topo->total_physical / (1024 * 1024),
                 topo->total_usable / (1024 * 1024));
        return;
    }
    
    // 处理Bootloader提供的内存映射
    topo->region_count = 0;
    topo->total_usable = 0;
    topo->total_physical = 0;
    
    u64 max_region_count = sizeof(topo->regions) / sizeof(topo->regions[0]);
    
    for (u64 i = 0; i < boot_info->mem_map_entry_count && i < max_region_count; i++) {
        hic_mem_entry_t* entry = &boot_info->mem_map[i];
        
        if (topo->region_count >= max_region_count) {
            log_warning("内存区域数量超过上限 %lu\n", max_region_count);
            break;
        }
        
        mem_region_t* region = &topo->regions[topo->region_count];
        region->base = entry->base;
        region->size = entry->length;
        
        // 统计
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            topo->total_usable += entry->length;
        }
        
        // 计算总物理内存（最大地址 + 区域大小）
        u64 region_end = entry->base + entry->length;
        if (region_end > topo->total_physical) {
            topo->total_physical = region_end;
        }
        
        topo->region_count++;
    }
    
    log_info("内存: 总计 %lu MB, 可用 %lu MB, 区域数 %lu\n",
             topo->total_physical / (1024 * 1024),
             topo->total_usable / (1024 * 1024),
             topo->region_count);
    
    for (u32 i = 0; i < topo->region_count; i++) {
        mem_region_t* region = &topo->regions[i];
        
        log_info("  区域 %u: 0x%016lx - 0x%016lx (%10lu KB)\n",
                 i, region->base, region->base + region->size - 1,
                 region->size / 1024);
    }
}

/**
 * 探测PCI设备
 */
void detect_pci_devices(device_list_t* devices) {
    devices->pci_count = 0;
    
    // 扫描PCI总线（限制为前8条总线，避免过长的扫描）
    for (u8 bus = 0; bus < 8; bus++) {
        for (u8 device = 0; device < 32; device++) {
            for (u8 function = 0; function < 8; function++) {
                // 读取厂商ID和设备ID
                u32 vendor_device = pci_read_config(bus, device, function, 0);
                u16 vendor_id = (u16)(vendor_device & 0xFFFF);
                u16 device_id = (u16)((vendor_device >> 16) & 0xFFFF);
                
                // 检查设备是否存在
                if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
                    if (function == 0) break; // 该设备不存在
                    continue;
                }
                
                device_t* dev = &devices->devices[devices->pci_count];
                dev->vendor_id = vendor_id;
                dev->device_id = device_id;
                dev->pci.bus = bus;
                dev->pci.device = device;
                dev->pci.function = function;
                
                // 读取类代码
                u32 class_rev = pci_read_config(bus, device, function, 8);
                dev->class_code = (u16)((class_rev >> 8) & 0xFFFFFF);
                dev->revision = class_rev & 0xFF;
                
                // 读取BAR
                for (int i = 0; i < 6; i++) {
                    u32 bar = pci_read_config(bus, device, function, (u8)(0x10 + i * 4));
                    dev->pci.bar[i] = bar;
                    
                    // 确定BAR大小（完整实现）
                    if (bar & 0x1) { 
                        // IO空间
                        u32 size = 0;
                        pci_write_config(bus, device, function, (u8)(0x10 + i * 4), 0xFFFF);
                        size = pci_read_config(bus, device, function, (u8)(0x10 + i * 4));
                        pci_write_config(bus, device, function, (u8)(0x10 + i * 4), bar);
                        
                        // 计算大小
                        size = (~(size & 0xFFFC)) + 1;
                        dev->pci.bar_size[i] = size;
                    } else if (bar) { 
                        // 内存空间
                        u32 size = 0;
                        pci_write_config(bus, device, function, (u8)(0x10 + i * 4), 0xFFFFFFFF);
                        size = pci_read_config(bus, device, function, (u8)(0x10 + i * 4));
                        pci_write_config(bus, device, function, (u8)(0x10 + i * 4), bar);
                        
                        // 计算大小
                        size = (~(size & 0xFFFFFFF0)) + 1;
                        dev->pci.bar_size[i] = size;
                    } else {
                        dev->pci.bar_size[i] = 0;
                    }
                }
                
                // 读取IRQ线
                u8 irq_line = pci_read_config_byte(bus, device, function, 0x3C);
                dev->irq = irq_line;
                
                // 设置设备名称（完整实现）
                format_device_name(dev->name, sizeof(dev->name), 0, 0, bus, device, function, vendor_id, device_id);
                
                devices->pci_count++;
                
                if (devices->pci_count >= 256) {
                    log_info("PCI设备达到上限\n");
                    return;
                }
            }
        }
    }
    
    log_info("发现 %u 个PCI设备\n", devices->pci_count);
    for (u32 i = 0; i < devices->pci_count; i++) {
        device_t* dev = &devices->devices[i];
        log_info("  %s\n", dev->name);
    }
}

/**
 * 探测ACPI信息
 */
void detect_acpi_info(hardware_probe_result_t* result) {
    // 从Bootloader获取ACPI RSDP
    hic_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!boot_info || !boot_info->rsdp) {
        log_warning("无法获取ACPI RSDP，使用默认值\n");
        
        // 使用默认值
        result->local_irq.base_address = 0xFEE00000;
        result->io_irq.base_address = 0xFEC00000;
        result->smp_enabled = false;
        
        log_info("APIC基地址: 0x%016lx (默认)\n", result->local_irq.base_address);
        log_info("IOAPIC基地址: 0x%016lx (默认)\n", result->io_irq.base_address);
        log_info("SMP: 禁用 (默认)\n");
        return;
    }
    
    // 解析RSDP
    u8* rsdp = (u8*)boot_info->rsdp;
    
    // 验证校验和（ACPI 1.0）
    u8 checksum = 0;
    for (int i = 0; i < 20; i++) {
        checksum += rsdp[i];
    }
    
    if (checksum != 0) {
        log_error("ACPI RSDP校验和错误\n");
        return;
    }
    
    // 检查ACPI版本
    u8 revision = rsdp[15];
    u32* rsdt_address = (u32*)&rsdp[16];
    u64* xsdt_address = (u64*)&rsdp[24];
    
    log_info("ACPI版本: %s\n", revision >= 2 ? "2.0+" : "1.0");
    log_info("RSDT地址: 0x%08x\n", *rsdt_address);
    
    if (revision >= 2) {
        log_info("XSDT地址: 0x%016llx\n", *xsdt_address);
    }
    
    // 解析MADT查找APIC信息
    // MADT签名: "APIC"
    u8* rsdt = (u8*)(revision >= 2 ? *xsdt_address : *rsdt_address);
    u32 rsdt_length = *((u32*)&rsdt[4]);
    
    for (u32 offset = 36; offset < rsdt_length; offset += 4) {
        u32 entry_addr = *((u32*)&rsdt[offset]);
        u8* table = (u8*)(u64)entry_addr;
        
        // 检查签名
        if (memcmp(table, "APIC", 4) == 0) {
            // 找到MADT
            u32 madt_length = *((u32*)&table[4]);
            
            // 扫描MADT中的APIC条目
            for (u32 madt_offset = 44; madt_offset < madt_length; ) {
                u8 entry_type = table[madt_offset];
                u8 entry_length = table[madt_offset + 1];
                
                if (entry_type == 0) {  // 处理器本地APIC
                    // 本地APIC信息
                    u8 apic_id = table[madt_offset + 3];
                    u8 apic_version = table[madt_offset + 4];
                    
                    log_info("本地APIC: ID=%u, 版本=%u\n", apic_id, apic_version);
                } else if (entry_type == 1) {  // I/O APIC
                    // I/O APIC信息
                    u64 ioapic_address = *((u64*)&table[madt_offset + 4]);
                    u32 gsi_base = *((u32*)&table[madt_offset + 12]);
                    
                    result->io_irq.base_address = ioapic_address;
                    log_info("IOAPIC: 地址=0x%016llx, GSI基地=%u\n", 
                             ioapic_address, gsi_base);
                }
                
                madt_offset += entry_length;
            }
            
            // 检测SMP
            result->smp_enabled = (result->cpu.logical_cores > 1);
            break;
        }
    }
}
