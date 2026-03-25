/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 设备管理服务实现
 * 
 * 从 Core-0 hardware_probe.c 迁移的硬件探测功能。
 * 提供策略层的设备管理服务。
 */

#include "service.h"

/* 简化的内核接口 */
#define NULL ((void*)0)

/* 串口输出接口 */
extern void serial_print(const char *msg);

/* 内联函数：输出端口 */
static inline void outb(u16 port, u8 value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* ==================== 服务全局状态 ==================== */

static struct {
    u8 initialized;
    device_list_t pci_devices;
    acpi_info_t acpi;
    u32 probe_complete;
} g_dev_mgr = {0};

/* ==================== PCI 配置空间访问 ==================== */

static u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (1U << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                  ((u32)function << 8) | (offset & 0xFC);
    outb(0xCF8, (u8)(address >> 8));
    outb(0xCF9, (u8)address);
    return (u32)inb(0xCFC) | 
           ((u32)inb((u16)(0xCFC + 1)) << 8) |
           ((u32)inb((u16)(0xCFC + 2)) << 16) | 
           ((u32)inb((u16)(0xCFC + 3)) << 24);
}

static u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset) {
    u32 value = pci_read_config(bus, device, function, offset & 0xFC);
    return (u8)((value >> ((offset & 3) * 8)) & 0xFF);
}

static void pci_write_config(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    u32 address = (1U << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                  ((u32)function << 8) | (offset & 0xFC);
    outb(0xCF8, (u8)(address >> 8));
    outb(0xCF9, (u8)address);
    outb(0xCFC, (u8)(value & 0xFF));
    outb((u16)(0xCFC + 1), (u8)((value >> 8) & 0xFF));
    outb((u16)(0xCFC + 2), (u8)((value >> 16) & 0xFF));
    outb((u16)(0xCFC + 3), (u8)((value >> 24) & 0xFF));
}

/* ==================== 辅助函数 ==================== */

static void u16_to_hex(u16 value, char* buf) {
    const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(value >> 12) & 0xF];
    buf[1] = hex[(value >> 8) & 0xF];
    buf[2] = hex[(value >> 4) & 0xF];
    buf[3] = hex[value & 0xF];
    buf[4] = '\0';
}

static void u8_to_dec(u8 value, char* buf) {
    if (value >= 100) {
        buf[0] = '0' + (value / 100);
        buf[1] = '0' + ((value / 10) % 10);
        buf[2] = '0' + (value % 10);
        buf[3] = '\0';
    } else if (value >= 10) {
        buf[0] = '0' + (value / 10);
        buf[1] = '0' + (value % 10);
        buf[2] = '\0';
    } else {
        buf[0] = '0' + value;
        buf[1] = '\0';
    }
}

static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void str_append(char* dest, const char* src, int max_len) {
    int dest_len = str_len(dest);
    int src_len = str_len(src);
    int i;
    for (i = 0; i < src_len && dest_len + i < max_len - 1; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + i] = '\0';
}

static void format_pci_name(char* dest, int max_len, u8 bus, u8 dev, u8 func, u16 vid, u16 did) {
    char buf[8];
    
    dest[0] = '\0';
    str_append(dest, "PCI ", max_len);
    
    u8_to_dec(bus, buf);
    str_append(dest, buf, max_len);
    str_append(dest, ":", max_len);
    
    u8_to_dec(dev, buf);
    str_append(dest, buf, max_len);
    str_append(dest, ".", max_len);
    
    u8_to_dec(func, buf);
    str_append(dest, buf, max_len);
    str_append(dest, " [", max_len);
    
    u16_to_hex(vid, buf);
    str_append(dest, buf, max_len);
    str_append(dest, ":", max_len);
    
    u16_to_hex(did, buf);
    str_append(dest, buf, max_len);
    str_append(dest, "]", max_len);
}

/* ==================== PCI 设备探测 ==================== */

static void probe_pci_bus(u8 bus) {
    for (u8 device = 0; device < 32; device++) {
        for (u8 function = 0; function < 8; function++) {
            /* 读取厂商ID和设备ID */
            u32 vendor_device = pci_read_config(bus, device, function, 0);
            u16 vendor_id = (u16)(vendor_device & 0xFFFF);
            u16 device_id = (u16)((vendor_device >> 16) & 0xFFFF);
            
            /* 检查设备是否存在 */
            if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
                if (function == 0) break;
                continue;
            }
            
            /* 检查容量 */
            if (g_dev_mgr.pci_devices.count >= 64) {
                serial_print("[DEV_MGR] PCI device limit reached\n");
                return;
            }
            
            device_info_t *dev = &g_dev_mgr.pci_devices.devices[g_dev_mgr.pci_devices.count];
            dev->type = DEVICE_TYPE_PCI;
            dev->vendor_id = vendor_id;
            dev->device_id = device_id;
            dev->pci.bus = bus;
            dev->pci.device = device;
            dev->pci.function = function;
            dev->pci.vendor_id = vendor_id;
            dev->pci.device_id = device_id;
            
            /* 读取类代码 */
            u32 class_rev = pci_read_config(bus, device, function, 8);
            dev->class_code = (u16)((class_rev >> 8) & 0xFFFFFF);
            dev->pci.class_code = dev->class_code;
            dev->revision = (u8)(class_rev & 0xFF);
            dev->pci.revision = dev->revision;
            
            /* 读取 BAR */
            for (int i = 0; i < 6; i++) {
                u32 bar = pci_read_config(bus, device, function, (u8)(0x10 + i * 4));
                dev->pci.bar[i] = bar;
                
                /* 确定 BAR 大小 */
                if (bar) {
                    pci_write_config(bus, device, function, (u8)(0x10 + i * 4), 0xFFFFFFFF);
                    u32 size = pci_read_config(bus, device, function, (u8)(0x10 + i * 4));
                    pci_write_config(bus, device, function, (u8)(0x10 + i * 4), bar);
                    
                    if (bar & 0x1) {
                        size = (~(size & 0xFFFC)) + 1;
                    } else {
                        size = (~(size & 0xFFFFFFF0)) + 1;
                    }
                    dev->pci.bar_size[i] = size;
                } else {
                    dev->pci.bar_size[i] = 0;
                }
            }
            
            /* 读取 IRQ */
            dev->irq = pci_read_config_byte(bus, device, function, 0x3C);
            dev->pci.irq = dev->irq;
            
            /* 格式化设备名称 */
            format_pci_name(dev->name, sizeof(dev->name), bus, device, function, vendor_id, device_id);
            
            g_dev_mgr.pci_devices.count++;
            
            /* 调试输出 */
            serial_print("[DEV_MGR] Found: ");
            serial_print(dev->name);
            serial_print("\n");
        }
    }
}

/* ==================== ACPI 探测 ==================== */

/* ACPI 表签名检查 */
static int memcmp_sig(const char* a, const char* b, int len) {
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}

/* 从 bootloader 传递的 RSDP 地址（外部引用） */
extern void* g_boot_rsdp;  /* 需要在 boot_info.c 中定义 */

static void probe_acpi_tables(void) {
    /* 使用 bootloader 提供的 RSDP 地址 */
    /* 如果没有，扫描 BIOS 区域 */
    
    u8* rsdp = (u8*)0xE0000;  /* 默认扫描起始位置 */
    int found = 0;
    
    /* 在 BIOS ROM 区域扫描 RSDP */
    for (u64 addr = 0xE0000; addr < 0x100000; addr += 16) {
        u8* ptr = (u8*)addr;
        
        /* 检查 RSDP 签名 "RSD PTR " */
        if (memcmp_sig((const char*)ptr, "RSD PTR ", 8) == 0) {
            rsdp = ptr;
            found = 1;
            serial_print("[DEV_MGR] RSDP found in BIOS area\n");
            break;
        }
    }
    
    if (!found) {
        serial_print("[DEV_MGR] RSDP not found, using defaults\n");
        g_dev_mgr.acpi.rsdp_address = 0;
        g_dev_mgr.acpi.smp_enabled = 0;
        g_dev_mgr.acpi.local_apic.base_address = 0xFEE00000;
        g_dev_mgr.acpi.io_apic.base_address = 0xFEC00000;
        return;
    }
    
    g_dev_mgr.acpi.rsdp_address = (u64)rsdp;
    g_dev_mgr.acpi.revision = rsdp[15];
    
    /* 获取 RSDT/XSDT 地址 */
    if (g_dev_mgr.acpi.revision >= 2) {
        g_dev_mgr.acpi.xsdt_address = *((u64*)&rsdp[24]);
        g_dev_mgr.acpi.rsdt_address = *((u32*)&rsdp[16]);
    } else {
        g_dev_mgr.acpi.rsdt_address = *((u32*)&rsdp[16]);
        g_dev_mgr.acpi.xsdt_address = 0;
    }
    
    serial_print("[DEV_MGR] ACPI revision: ");
    char rev_buf[4];
    u8_to_dec(g_dev_mgr.acpi.revision, rev_buf);
    serial_print(rev_buf);
    serial_print("\n");
    
    /* TODO: 解析 MADT 获取 APIC 信息 */
    g_dev_mgr.acpi.local_apic.base_address = 0xFEE00000;
    g_dev_mgr.acpi.io_apic.base_address = 0xFEC00000;
    g_dev_mgr.acpi.smp_enabled = 1;  /* 假设 SMP 可用 */
}

/* ==================== 服务接口实现 ==================== */

device_manager_status_t device_manager_init(void) {
    serial_print("[DEV_MGR] Initializing device manager service...\n");
    
    /* 初始化设备列表 */
    g_dev_mgr.pci_devices.count = 0;
    g_dev_mgr.probe_complete = 0;
    
    g_dev_mgr.initialized = 1;
    
    serial_print("[DEV_MGR] Initialization complete\n");
    return DEV_MGR_OK;
}

device_manager_status_t device_manager_start(void) {
    if (!g_dev_mgr.initialized) {
        return DEV_MGR_NOT_INITIALIZED;
    }
    
    serial_print("[DEV_MGR] Starting device enumeration...\n");
    
    /* 探测 PCI 总线 */
    serial_print("[DEV_MGR] Probing PCI buses...\n");
    for (u8 bus = 0; bus < 8; bus++) {
        probe_pci_bus(bus);
    }
    
    serial_print("[DEV_MGR] Found ");
    char count_buf[8];
    u8_to_dec((u8)g_dev_mgr.pci_devices.count, count_buf);
    serial_print(count_buf);
    serial_print(" PCI devices\n");
    
    /* 探测 ACPI */
    serial_print("[DEV_MGR] Probing ACPI tables...\n");
    probe_acpi_tables();
    
    g_dev_mgr.probe_complete = 1;
    
    serial_print("[DEV_MGR] Device enumeration complete\n");
    return DEV_MGR_OK;
}

device_manager_status_t device_manager_probe_pci(device_list_t *devices) {
    if (!g_dev_mgr.initialized) {
        return DEV_MGR_NOT_INITIALIZED;
    }
    
    if (devices == NULL) {
        return DEV_MGR_INVALID_PARAM;
    }
    
    /* 复制设备列表 */
    int i;
    for (i = 0; i < (int)g_dev_mgr.pci_devices.count && i < 64; i++) {
        devices->devices[i] = g_dev_mgr.pci_devices.devices[i];
    }
    devices->count = g_dev_mgr.pci_devices.count;
    
    return DEV_MGR_OK;
}

device_manager_status_t device_manager_get_devices(device_list_t *devices) {
    return device_manager_probe_pci(devices);
}

device_manager_status_t device_manager_find_device(u16 vendor_id, u16 device_id,
                                                    device_info_t *device) {
    if (!g_dev_mgr.initialized || device == NULL) {
        return DEV_MGR_INVALID_PARAM;
    }
    
    for (u32 i = 0; i < g_dev_mgr.pci_devices.count; i++) {
        device_info_t *dev = &g_dev_mgr.pci_devices.devices[i];
        if (dev->vendor_id == vendor_id && dev->device_id == device_id) {
            *device = *dev;
            return DEV_MGR_OK;
        }
    }
    
    return DEV_MGR_NOT_FOUND;
}

device_manager_status_t device_manager_find_pci_device(u8 bus, u8 device, u8 function,
                                                        device_info_t *info) {
    if (!g_dev_mgr.initialized || info == NULL) {
        return DEV_MGR_INVALID_PARAM;
    }
    
    for (u32 i = 0; i < g_dev_mgr.pci_devices.count; i++) {
        device_info_t *dev = &g_dev_mgr.pci_devices.devices[i];
        if (dev->type == DEVICE_TYPE_PCI &&
            dev->pci.bus == bus &&
            dev->pci.device == device &&
            dev->pci.function == function) {
            *info = *dev;
            return DEV_MGR_OK;
        }
    }
    
    return DEV_MGR_NOT_FOUND;
}

device_manager_status_t device_manager_probe_acpi(acpi_info_t *info) {
    if (!g_dev_mgr.initialized || info == NULL) {
        return DEV_MGR_INVALID_PARAM;
    }
    
    *info = g_dev_mgr.acpi;
    return DEV_MGR_OK;
}

device_manager_status_t device_manager_get_irq_controllers(irq_controller_t *local,
                                                            irq_controller_t *io) {
    if (!g_dev_mgr.initialized) {
        return DEV_MGR_NOT_INITIALIZED;
    }
    
    if (local) {
        *local = g_dev_mgr.acpi.local_apic;
    }
    if (io) {
        *io = g_dev_mgr.acpi.io_apic;
    }
    
    return DEV_MGR_OK;
}

/* ==================== 模块入口点 ==================== */

int device_manager_module_init(void) {
    return (device_manager_init() == DEV_MGR_OK) ? 0 : -1;
}

int device_manager_module_start(void) {
    return (device_manager_start() == DEV_MGR_OK) ? 0 : -1;
}

/* 服务入口点（静态模块） */
__attribute__((section(".static_svc.device_manager.text"), used, noinline))
int _device_manager_entry(void) {
    serial_print("[DEV_MGR] Entry point reached\n");
    
    if (device_manager_init() != DEV_MGR_OK) {
        return -1;
    }
    
    device_manager_start();
    
    /* 服务初始化完成，返回让调度器继续 */
    serial_print("[DEV_MGR] Service ready\n");
    return 0;
}
