/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件探测服务 - 策略层实现
 * 
 * 本服务实现硬件探测策略，使用 Core-0 机制层原语。
 */

#include "service.h"
#include <hardware_probe.h>
#include <hal.h>
#include <string.h>
#include <console.h>

/* 最大设备数量 */
#define MAX_DEVICES 64

/* ==================== 策略层状态 ==================== */

/* 缓存的探测结果 */
static hardware_probe_result_t g_cached_result;
static bool g_cache_valid = false;

/* 探测配置 */
static probe_config_t g_probe_config = {
    .probe_cpu = true,
    .probe_pci = true,
    .probe_acpi = true,
    .pci_bus_start = 0,
    .pci_bus_end = 7,
    .cache_results = true
};

/* 硬件变化回调 */
static hardware_change_handler_t g_change_handler = NULL;
static void* g_change_context = NULL;

/* ==================== 辅助函数 ==================== */

/* 数字转十六进制字符串 */
static void u16_to_hex(u16 value, char* buf) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(value >> 12) & 0xF];
    buf[1] = hex[(value >> 8) & 0xF];
    buf[2] = hex[(value >> 4) & 0xF];
    buf[3] = hex[value & 0xF];
    buf[4] = '\0';
}

/* 格式化设备名称 */
static void format_device_name(device_t* dev, u16 vid, u16 did) {
    char vid_str[5], did_str[5];
    u16_to_hex(vid, vid_str);
    u16_to_hex(did, did_str);
    
    snprintf(dev->name, sizeof(dev->name), "PCI %02x:%02x.%x [%s:%s]",
             dev->pci.bus, dev->pci.device, dev->pci.function,
             vid_str, did_str);
}

/* ==================== 策略层：初始化 ==================== */

/**
 * 初始化硬件探测服务（策略层）
 */
hic_status_t hardware_probe_service_init(void)
{
    memset(&g_cached_result, 0, sizeof(g_cached_result));
    g_cache_valid = false;
    g_change_handler = NULL;
    g_change_context = NULL;
    
    console_puts("[HW_PROBE_SVC] Hardware probe service (policy layer) initialized\n");
    
    return HIC_SUCCESS;
}

/**
 * 启动服务
 */
hic_status_t hardware_probe_service_start(void)
{
    /* 执行初始探测 */
    hic_status_t status = hardware_probe_all(&g_cached_result);
    
    if (status == HIC_SUCCESS) {
        g_cache_valid = true;
        console_puts("[HW_PROBE_SVC] Initial hardware probe completed\n");
    }
    
    return status;
}

/**
 * 停止服务
 */
hic_status_t hardware_probe_service_stop(void)
{
    g_cache_valid = false;
    console_puts("[HW_PROBE_SVC] Hardware probe service stopped\n");
    return HIC_SUCCESS;
}

/* ==================== 策略层：探测操作 ==================== */

/**
 * 执行完整硬件探测（策略层）
 */
hic_status_t hardware_probe_all(hardware_probe_result_t* result)
{
    if (!result) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(hardware_probe_result_t));
    
    /* 策略决策：按顺序执行探测 */
    
    /* 1. 获取内存拓扑（机制层已探测） */
    const memory_topology_t* mem = hardware_probe_get_memory();
    if (mem) {
        memcpy(&result->memory, mem, sizeof(memory_topology_t));
    }
    
    /* 2. 探测CPU（如果配置启用） */
    if (g_probe_config.probe_cpu) {
        probe_cpu_info(&result->cpu);
    }
    
    /* 3. 探测PCI设备（如果配置启用） */
    if (g_probe_config.probe_pci) {
        probe_pci_devices(&result->devices);
    }
    
    /* 4. 解析ACPI（如果配置启用） */
    if (g_probe_config.probe_acpi) {
        probe_acpi_info(result);
    }
    
    console_puts("[HW_PROBE_SVC] Hardware probe completed: ");
    console_putu32(result->devices.pci_count);
    console_puts(" PCI devices, ");
    console_putu64(result->memory.total_usable / (1024 * 1024));
    console_puts(" MB usable memory\n");
    
    return HIC_SUCCESS;
}

/**
 * 探测CPU详细信息（策略层）
 */
hic_status_t probe_cpu_info(cpu_info_t* info)
{
    if (!info) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(cpu_info_t));
    
    u32 eax, ebx, ecx, edx;
    
    /* 策略决策：使用 CPUID 探测 CPU 信息 */
    
    /* 获取厂商ID */
    cpuid_execute(0, 0, &eax, &ebx, &ecx, &edx);
    info->vendor_id[0] = ebx;
    info->vendor_id[1] = edx;
    info->vendor_id[2] = ecx;
    
    /* 获取版本信息 */
    cpuid_execute(1, 0, &eax, &ebx, &ecx, &edx);
    info->version = eax;
    info->stepping = eax & 0xF;
    info->model = (eax >> 4) & 0xF;
    info->family = (eax >> 8) & 0xF;
    info->feature_flags[0] = ecx;
    info->feature_flags[1] = edx;
    
    /* 获取扩展特性 */
    cpuid_execute(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    info->feature_flags[2] = ecx;
    info->feature_flags[3] = edx;
    
    /* 获取缓存行大小 */
    cpuid_execute(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000006) {
        cpuid_execute(0x80000006, 0, &eax, &ebx, &ecx, &edx);
        info->cache_line_size = ecx & 0xFF;
    }
    
    /* 获取品牌字符串 */
    if (eax >= 0x80000004) {
        u32* brand = (u32*)info->brand_string;
        cpuid_execute(0x80000002, 0, &eax, &ebx, &ecx, &edx);
        brand[0] = eax; brand[1] = ebx; brand[2] = ecx; brand[3] = edx;
        cpuid_execute(0x80000003, 0, &eax, &ebx, &ecx, &edx);
        brand[4] = eax; brand[5] = ebx; brand[6] = ecx; brand[7] = edx;
        cpuid_execute(0x80000004, 0, &eax, &ebx, &ecx, &edx);
        brand[8] = eax; brand[9] = ebx; brand[10] = ecx; brand[11] = edx;
    }
    
    /* 获取核心数 */
    info->logical_cores = (ebx >> 16) & 0xFF;
    
    if (eax >= 0x80000008) {
        cpuid_execute(0x80000008, 0, &eax, &ebx, &ecx, &edx);
        info->physical_cores = (ecx & 0xFF) + 1;
    } else {
        /* 估算 */
        if (info->feature_flags[1] & (1 << 28)) {
            info->physical_cores = info->logical_cores / 2;
        } else {
            info->physical_cores = info->logical_cores;
        }
    }
    
    console_puts("[HW_PROBE_SVC] CPU: ");
    console_puts(info->brand_string);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/**
 * 探测PCI设备（策略层）
 */
hic_status_t probe_pci_devices(device_list_t* devices)
{
    if (!devices) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memset(devices, 0, sizeof(device_list_t));
    
    /* 策略决策：扫描配置的总线范围 */
    for (u8 bus = g_probe_config.pci_bus_start; bus <= g_probe_config.pci_bus_end; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            for (u8 func = 0; func < 8; func++) {
                /* 使用机制层原语读取配置 */
                u32 vendor_device = pci_read_config(bus, dev, func, 0);
                u16 vendor_id = (u16)(vendor_device & 0xFFFF);
                u16 device_id = (u16)((vendor_device >> 16) & 0xFFFF);
                
                /* 检查设备是否存在 */
                if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
                    if (func == 0) break;
                    continue;
                }
                
                /* 策略决策：添加到设备列表 */
                if (devices->pci_count >= MAX_DEVICES) {
                    console_puts("[HW_PROBE_SVC] PCI device limit reached\n");
                    return HIC_SUCCESS;
                }
                
                device_t* device = &devices->devices[devices->pci_count];
                device->type = DEVICE_TYPE_PCI;
                device->vendor_id = vendor_id;
                device->device_id = device_id;
                device->pci.bus = bus;
                device->pci.device = dev;
                device->pci.function = func;
                
                /* 读取类代码 */
                u32 class_rev = pci_read_config(bus, dev, func, 8);
                device->class_code = (u16)((class_rev >> 16) & 0xFFFF);
                device->revision = class_rev & 0xFF;
                
                /* 读取BAR */
                for (int i = 0; i < 6; i++) {
                    u32 bar = pci_read_config(bus, dev, func, (u8)(0x10 + i * 4));
                    device->pci.bar[i] = bar;
                    
                    /* 策略决策：计算BAR大小 */
                    if (bar) {
                        pci_write_config(bus, dev, func, (u8)(0x10 + i * 4), 0xFFFFFFFF);
                        u32 size = pci_read_config(bus, dev, func, (u8)(0x10 + i * 4));
                        pci_write_config(bus, dev, func, (u8)(0x10 + i * 4), bar);
                        
                        if (bar & 1) {
                            size = (~(size & 0xFFFC)) + 1;
                        } else {
                            size = (~(size & 0xFFFFFFF0)) + 1;
                        }
                        device->pci.bar_size[i] = size;
                    }
                }
                
                /* 读取IRQ */
                device->irq = pci_read_config_byte(bus, dev, func, 0x3C);
                
                /* 设置设备名称 */
                format_device_name(device, vendor_id, device_id);
                
                devices->pci_count++;
                devices->device_count++;
            }
        }
    }
    
    console_puts("[HW_PROBE_SVC] Found ");
    console_putu32(devices->pci_count);
    console_puts(" PCI devices\n");
    
    return HIC_SUCCESS;
}

/**
 * 解析ACPI信息（策略层）
 */
hic_status_t probe_acpi_info(hardware_probe_result_t* result)
{
    if (!result) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 策略决策：使用默认值（完整ACPI解析需要更多代码） */
    result->local_irq.base_address = 0xFEE00000;
    result->io_irq.base_address = 0xFEC00000;
    result->smp_enabled = (result->cpu.logical_cores > 1);
    
    console_puts("[HW_PROBE_SVC] ACPI: Local APIC=0x");
    console_puthex64(result->local_irq.base_address);
    console_puts(", IOAPIC=0x");
    console_puthex64(result->io_irq.base_address);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/**
 * 重新探测硬件（策略层）
 */
hic_status_t hardware_probe_refresh(void)
{
    g_cache_valid = false;
    return hardware_probe_all(&g_cached_result);
}

/* ==================== 策略层：缓存管理 ==================== */

/**
 * 获取缓存的探测结果
 */
const hardware_probe_result_t* hardware_probe_get_cached(void)
{
    return g_cache_valid ? &g_cached_result : NULL;
}

/**
 * 清除缓存
 */
void hardware_probe_clear_cache(void)
{
    memset(&g_cached_result, 0, sizeof(g_cached_result));
    g_cache_valid = false;
}

/**
 * 检查缓存是否有效
 */
bool hardware_probe_cache_valid(void)
{
    return g_cache_valid;
}

/* ==================== 策略层：设备查询 ==================== */

/**
 * 按类型查找设备
 */
hic_status_t hardware_find_devices_by_type(device_type_t type, 
                                            device_t** devices, 
                                            u32* count)
{
    if (!g_cache_valid || !devices || !count) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    static device_t found_devices[MAX_DEVICES];
    u32 found = 0;
    
    for (u32 i = 0; i < g_cached_result.devices.device_count && found < MAX_DEVICES; i++) {
        if (g_cached_result.devices.devices[i].type == type) {
            memcpy(&found_devices[found], &g_cached_result.devices.devices[i], 
                   sizeof(device_t));
            found++;
        }
    }
    
    *devices = found_devices;
    *count = found;
    
    return HIC_SUCCESS;
}

/**
 * 获取设备详情
 */
const device_t* hardware_get_device(u32 index)
{
    if (!g_cache_valid || index >= g_cached_result.devices.device_count) {
        return NULL;
    }
    
    return &g_cached_result.devices.devices[index];
}

/* ==================== 策略层：驱动匹配 ==================== */

/**
 * 为设备匹配驱动
 */
hic_status_t hardware_match_driver(device_t* device, driver_match_t* match)
{
    if (!device || !match) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 策略决策：基于厂商/设备ID匹配驱动 */
    /* 这里使用简单的硬编码匹配表 */
    
    memset(match, 0, sizeof(driver_match_t));
    match->device = device;
    match->confidence = 50;  /* 默认置信度 */
    
    /* 根据类代码推荐驱动 */
    u8 base_class = (device->class_code >> 8) & 0xFF;
    
    switch (base_class) {
        case 0x01:  /* 大容量存储 */
            strncpy(match->recommended_driver, "storage_driver", 
                    sizeof(match->recommended_driver));
            match->confidence = 80;
            break;
        case 0x02:  /* 网络 */
            strncpy(match->recommended_driver, "network_driver",
                    sizeof(match->recommended_driver));
            match->confidence = 80;
            break;
        case 0x03:  /* 显示 */
            strncpy(match->recommended_driver, "display_driver",
                    sizeof(match->recommended_driver));
            match->confidence = 75;
            break;
        case 0x0C:  /* 串行总线 */
            strncpy(match->recommended_driver, "serial_bus_driver",
                    sizeof(match->recommended_driver));
            match->confidence = 70;
            break;
        default:
            strncpy(match->recommended_driver, "generic_driver",
                    sizeof(match->recommended_driver));
            match->confidence = 30;
            break;
    }
    
    return HIC_SUCCESS;
}

/* ==================== 策略层：变化检测 ==================== */

/**
 * 注册硬件变化回调
 */
hic_status_t hardware_register_change_handler(hardware_change_handler_t handler, 
                                                void* context)
{
    g_change_handler = handler;
    g_change_context = context;
    return HIC_SUCCESS;
}

/**
 * 检测硬件变化
 */
hic_status_t hardware_detect_changes(void)
{
    /* 保存旧设备列表 */
    device_list_t old_devices;
    memcpy(&old_devices, &g_cached_result.devices, sizeof(device_list_t));
    
    /* 重新探测 */
    hardware_probe_refresh();
    
    /* 策略决策：比较新旧设备列表 */
    if (g_change_handler) {
        /* 检测新增设备 */
        for (u32 i = 0; i < g_cached_result.devices.device_count; i++) {
            bool found = false;
            for (u32 j = 0; j < old_devices.device_count; j++) {
                if (memcmp(&g_cached_result.devices.devices[i], 
                          &old_devices.devices[j], 
                          sizeof(device_t)) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                g_change_handler(&g_cached_result.devices.devices[i], true, 
                                 g_change_context);
            }
        }
        
        /* 检测移除设备 */
        for (u32 i = 0; i < old_devices.device_count; i++) {
            bool found = false;
            for (u32 j = 0; j < g_cached_result.devices.device_count; j++) {
                if (memcmp(&old_devices.devices[i],
                          &g_cached_result.devices.devices[j],
                          sizeof(device_t)) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                g_change_handler(&old_devices.devices[i], false, g_change_context);
            }
        }
    }
    
    return HIC_SUCCESS;
}

/* ==================== 策略层：配置 ==================== */

/**
 * 设置探测配置
 */
hic_status_t hardware_probe_set_config(const probe_config_t* config)
{
    if (!config) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memcpy(&g_probe_config, config, sizeof(probe_config_t));
    return HIC_SUCCESS;
}

/**
 * 获取探测配置
 */
hic_status_t hardware_probe_get_config(probe_config_t* config)
{
    if (!config) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    memcpy(config, &g_probe_config, sizeof(probe_config_t));
    return HIC_SUCCESS;
}
