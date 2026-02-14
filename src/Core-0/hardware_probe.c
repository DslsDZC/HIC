/**
 * HIK内核运行时硬件探测实现
 * 实现x86_64架构的硬件发现
 */

#include "hardware_probe.h"
#include "string.h"
#include "console.h"

/* 全局硬件信息 */
static hardware_probe_result_t hw_probe_result;

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
    info->feature_flags_ecx = ecx;
    info->feature_flags_edx = edx;
    
    // 获取扩展特性
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    info->extended_features_ecx = ecx;
    info->extended_features_edx = edx;
    
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
        if (info->feature_flags_edx & (1 << 28)) { // HTT支持
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
    hik_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!boot_info || !boot_info->mem_map || boot_info->mem_map_entry_count == 0) {
        log_warning("无法获取内存映射信息，使用默认值\n");
        
        // 使用默认值
        topo->region_count = 1;
        topo->regions[0].base = 0x100000;
        topo->regions[0].length = 0x3FF00000;  // 假设1GB
        topo->regions[0].type = MEM_TYPE_USABLE;
        topo->regions[0].acpi_attr = 0;
        topo->total_usable = topo->regions[0].length;
        topo->total_physical = topo->regions[0].length + 0x100000;
        
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
        hik_mem_entry_t* entry = &boot_info->mem_map[i];
        
        if (topo->region_count >= max_region_count) {
            log_warning("内存区域数量超过上限 %lu\n", max_region_count);
            break;
        }
        
        mem_region_t* region = &topo->regions[topo->region_count];
        region->base = entry->base;
        region->length = entry->length;
        region->type = entry->type;
        region->acpi_attr = entry->flags;
        
        // 统计
        if (entry->type == HIK_MEM_TYPE_USABLE) {
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
        const char* type_str;
        
        switch (region->type) {
            case MEM_TYPE_USABLE: type_str = "可用"; break;
            case MEM_TYPE_RESERVED: type_str = "保留"; break;
            case MEM_TYPE_ACPI: type_str = "ACPI"; break;
            case MEM_TYPE_NVS: type_str = "NVS"; break;
            case MEM_TYPE_UNUSABLE: type_str = "不可用"; break;
            default: type_str = "未知"; break;
        }
        
        log_info("  区域 %u: 0x%016lx - 0x%016lx (%10lu KB) %s\n",
                 i, region->base, region->base + region->length - 1,
                 region->length / 1024, type_str);
    }
}

/**
 * 探测PCI设备
 */
void detect_pci_devices(device_list_t* devices) {
    devices->pci_count = 0;
    
    // 扫描所有PCI总线
    for (u8 bus = 0; bus < 256; bus++) {
        for (u8 device = 0; device < 32; device++) {
            for (u8 function = 0; function < 8; function++) {
                // 读取厂商ID和设备ID
                u16 vendor_device = pci_read_config(bus, device, function, 0);
                u16 vendor_id = vendor_device & 0xFFFF;
                u16 device_id = (vendor_device >> 16) & 0xFFFF;
                
                // 检查设备是否存在
                if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
                    if (function == 0) break; // 该设备不存在
                    continue;
                }
                
                pci_device_t* dev = &devices->pci_devices[devices->pci_count];
                dev->vendor_id = vendor_id;
                dev->device_id = device_id;
                dev->bus = bus;
                dev->device = device;
                dev->function = function;
                
                // 读取类代码
                u32 class_rev = pci_read_config(bus, device, function, 8);
                dev->class_code = (class_rev >> 8) & 0xFFFFFF;
                dev->revision = class_rev & 0xFF;
                
                // 读取BAR
                for (int i = 0; i < 6; i++) {
                    u32 bar = pci_read_config(bus, device, function, 0x10 + i * 4);
                    dev->bar[i] = bar;
                    
                    // 确定BAR大小（完整实现）
                    if (bar & 0x1) { 
                        // IO空间
                        u32 size = 0;
                        pci_write_config(bus, device, function, 0x10 + i * 4, 0xFFFF);
                        size = pci_read_config(bus, device, function, 0x10 + i * 4);
                        pci_write_config(bus, device, function, 0x10 + i * 4, bar);
                        
                        // 计算大小
                        size = (~(size & 0xFFFC)) + 1;
                        dev->bar_size[i] = size;
                    } else if (bar) { 
                        // 内存空间
                        u32 size = 0;
                        u32 bar_addr = bar & 0xFFFFFFF0;
                        pci_write_config(bus, device, function, 0x10 + i * 4, 0xFFFFFFFF);
                        size = pci_read_config(bus, device, function, 0x10 + i * 4);
                        pci_write_config(bus, device, function, 0x10 + i * 4, bar);
                        
                        // 计算大小
                        size = (~(size & 0xFFFFFFF0)) + 1;
                        dev->bar_size[i] = size;
                    } else {
                        dev->bar_size[i] = 0;
                    }
                }
                
                // 读取IRQ线
                u8 irq_line = pci_read_config_byte(bus, device, function, 0x3C);
                dev->irq_line = irq_line;
                
                // 设置设备名称（完整实现）
                const char* vendor_name = pci_vendor_to_string(vendor_id);
                const char* device_name = pci_device_to_string(vendor_id, device_id);
                
                if (vendor_name && device_name) {
                    snprintf(dev->name, sizeof(dev->name), 
                            "%s %s (PCI %02x:%02x.%x)",
                            vendor_name, device_name, bus, device, function);
                } else {
                    snprintf(dev->name, sizeof(dev->name), 
                            "PCI %02x:%02x.%x (%04x:%04x)",
                            bus, device, function, vendor_id, device_id);
                }
                
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
        pci_device_t* dev = &devices->pci_devices[i];
        log_info("  %s\n", dev->name);
    }
}

/**
 * 探测ACPI信息
 */
void detect_acpi_info(hardware_probe_result_t* result) {
    // 从Bootloader获取ACPI RSDP
    hik_boot_info_t* boot_info = g_boot_state.boot_info;
    
    if (!boot_info || !boot_info->rsdp) {
        log_warning("无法获取ACPI RSDP，使用默认值\n");
        
        // 使用默认值
        result->apic_base = 0xFEE00000;
        result->ioapic_base = 0xFEC00000;
        result->smp_enabled = false;
        
        log_info("APIC基地址: 0x%016lx (默认)\n", result->apic_base);
        log_info("IOAPIC基地址: 0x%016lx (默认)\n", result->ioapic_base);
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
        u32* entry = (u32*)&rsdt[offset];
        u8* table = (u8*)(*entry);
        
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
                    
                    result->ioapic_base = ioapic_address;
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
    
    log_info("SMP: %s\n", result->smp_enabled ? "启用" : "禁用");
}

/**
 * 综合探测
 */
void probe_all_hardware(hardware_probe_result_t* result) {
    log_info("========== 硬件探测开始 ==========\n");
    
    detect_cpu_info(&result->cpu);
    detect_memory_topology(&result->memory);
    detect_pci_devices(&result->devices);
    detect_acpi_info(result);
    
    log_info("========== 硬件探测完成 ==========\n");
}

/**
 * 获取硬件探测结果
 */
hardware_probe_result_t* get_hardware_probe_result(void) {
    return &hw_probe_result;
}

/* PCI配置空间访问辅助函数 */
static inline u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (1 << 31) | (bus << 16) | (device << 11) | 
                  (function << 8) | (offset & 0xFC);
    outb(0xCF8, address >> 8);
    outb(0xCF9, address);
    return inb(0xCFC) | (inb(0xCFC + 1) << 8) | 
           (inb(0xCFC + 2) << 16) | (inb(0xCFC + 3) << 24);
}

static inline u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset) {
    u32 value = pci_read_config(bus, device, function, offset & 0xFC);
    return (value >> ((offset & 3) * 8)) & 0xFF;
}
