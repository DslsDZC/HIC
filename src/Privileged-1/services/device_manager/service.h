/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 设备管理服务接口
 * 
 * 提供硬件设备探测、管理和查询功能。
 * 从 Core-0 hardware_probe 迁移的策略层实现。
 */

#ifndef HIC_DEVICE_MANAGER_SERVICE_H
#define HIC_DEVICE_MANAGER_SERVICE_H

#include <stdint.h>

/* 从 Core-0 类型定义简化而来 */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* 设备类型 */
typedef enum {
    DEVICE_TYPE_PCI,
    DEVICE_TYPE_PLATFORM,
    DEVICE_TYPE_IRQ_CONTROLLER,
    DEVICE_TYPE_TIMER,
    DEVICE_TYPE_SERIAL,
    DEVICE_TYPE_DISPLAY,
    DEVICE_TYPE_STORAGE,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_UNKNOWN
} device_type_t;

/* PCI 设备信息 */
typedef struct {
    u8 bus;
    u8 device;
    u8 function;
    u16 vendor_id;
    u16 device_id;
    u16 class_code;
    u8 revision;
    u32 bar[6];
    u32 bar_size[6];
    u8 irq;
    char name[64];
} pci_device_t;

/* 设备信息 */
typedef struct {
    device_type_t type;
    u16 vendor_id;
    u16 device_id;
    u16 class_code;
    u8 revision;
    u8 irq;
    char name[64];
    
    union {
        pci_device_t pci;
        struct {
            u64 address;
            u32 id;
        } platform;
    };
} device_info_t;

/* 设备列表 */
typedef struct {
    u32 count;
    device_info_t devices[64];
} device_list_t;

/* 中断控制器信息 */
typedef struct {
    u64 base_address;
    u32 irq_base;
    u32 num_irqs;
    u8 enabled;
    char name[32];
} irq_controller_t;

/* ACPI 信息 */
typedef struct {
    u64 rsdp_address;
    u64 rsdt_address;
    u64 xsdt_address;
    u8 revision;
    u8 smp_enabled;
    irq_controller_t local_apic;
    irq_controller_t io_apic;
} acpi_info_t;

/* 服务状态 */
typedef enum {
    DEV_MGR_OK = 0,
    DEV_MGR_ERROR = -1,
    DEV_MGR_NOT_INITIALIZED = -2,
    DEV_MGR_NOT_FOUND = -3,
    DEV_MGR_INVALID_PARAM = -4,
} device_manager_status_t;

/* 服务接口 */

/**
 * 初始化设备管理服务
 */
device_manager_status_t device_manager_init(void);

/**
 * 启动设备管理服务
 */
device_manager_status_t device_manager_start(void);

/**
 * 探测所有 PCI 设备
 */
device_manager_status_t device_manager_probe_pci(device_list_t *devices);

/**
 * 获取设备列表
 */
device_manager_status_t device_manager_get_devices(device_list_t *devices);

/**
 * 根据 vendor_id 和 device_id 查找设备
 */
device_manager_status_t device_manager_find_device(u16 vendor_id, u16 device_id,
                                                    device_info_t *device);

/**
 * 根据 PCI 地址查找设备
 */
device_manager_status_t device_manager_find_pci_device(u8 bus, u8 device, u8 function,
                                                        device_info_t *info);

/**
 * 探测 ACPI 信息
 */
device_manager_status_t device_manager_probe_acpi(acpi_info_t *info);

/**
 * 获取中断控制器信息
 */
device_manager_status_t device_manager_get_irq_controllers(irq_controller_t *local,
                                                            irq_controller_t *io);

/* 模块入口点 */
int device_manager_module_init(void);
int device_manager_module_start(void);

#endif /* HIC_DEVICE_MANAGER_SERVICE_H */
