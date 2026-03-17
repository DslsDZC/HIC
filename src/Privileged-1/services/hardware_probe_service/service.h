/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件探测服务 - 策略层实现
 * 
 * 本服务实现硬件探测策略，使用 Core-0 机制层原语。
 * 
 * 策略层职责：
 * - CPU详细信息探测策略
 * - PCI设备扫描策略（扫描范围、过滤）
 * - ACPI信息解析策略
 * - 硬件信息缓存和管理
 * - 硬件变化检测和通知
 * - 驱动匹配策略
 * 
 * 使用的机制层原语（Core-0）：
 * - pci_read_config() / pci_write_config() - PCI配置访问
 * - cpuid_execute() - CPUID执行
 * - hardware_probe_get_memory() - 获取内存拓扑
 */

#ifndef HIC_HARDWARE_PROBE_SERVICE_H
#define HIC_HARDWARE_PROBE_SERVICE_H

#include "common.h"
#include <hardware_probe.h>

/* ==================== 策略层：探测配置 ==================== */

/* 探测策略配置 */
typedef struct {
    bool probe_cpu;           /* 是否探测CPU */
    bool probe_pci;           /* 是否探测PCI */
    bool probe_acpi;          /* 是否解析ACPI */
    u8 pci_bus_start;         /* PCI总线扫描起始 */
    u8 pci_bus_end;           /* PCI总线扫描结束 */
    bool cache_results;       /* 是否缓存结果 */
} probe_config_t;

/* 设备驱动匹配结果 */
typedef struct {
    device_t* device;
    char recommended_driver[64];
    u32 confidence;           /* 匹配置信度 0-100 */
} driver_match_t;

/* 硬件变化回调 */
typedef void (*hardware_change_handler_t)(device_t* device, bool added, void* context);

/* ==================== 策略层：服务接口 ==================== */

/**
 * 初始化硬件探测服务（策略层）
 */
hic_status_t hardware_probe_service_init(void);

/**
 * 启动服务
 */
hic_status_t hardware_probe_service_start(void);

/**
 * 停止服务
 */
hic_status_t hardware_probe_service_stop(void);

/* ==================== 策略层：探测操作 ==================== */

/**
 * 执行完整硬件探测（策略层）
 */
hic_status_t hardware_probe_all(hardware_probe_result_t* result);

/**
 * 探测CPU详细信息（策略层）
 */
hic_status_t probe_cpu_info(cpu_info_t* info);

/**
 * 探测PCI设备（策略层）
 */
hic_status_t probe_pci_devices(device_list_t* devices);

/**
 * 解析ACPI信息（策略层）
 */
hic_status_t probe_acpi_info(hardware_probe_result_t* result);

/**
 * 重新探测硬件（策略层）
 */
hic_status_t hardware_probe_refresh(void);

/* ==================== 策略层：缓存管理 ==================== */

/**
 * 获取缓存的探测结果
 */
const hardware_probe_result_t* hardware_probe_get_cached(void);

/**
 * 清除缓存
 */
void hardware_probe_clear_cache(void);

/**
 * 检查缓存是否有效
 */
bool hardware_probe_cache_valid(void);

/* ==================== 策略层：设备查询 ==================== */

/**
 * 按类型查找设备
 */
hic_status_t hardware_find_devices_by_type(device_type_t type, 
                                            device_t** devices, 
                                            u32* count);

/**
 * 按厂商ID查找设备
 */
hic_status_t hardware_find_devices_by_vendor(u16 vendor_id,
                                              device_t** devices,
                                              u32* count);

/**
 * 按类代码查找设备
 */
hic_status_t hardware_find_devices_by_class(u16 class_code,
                                             device_t** devices,
                                             u32* count);

/**
 * 获取设备详情
 */
const device_t* hardware_get_device(u32 index);

/* ==================== 策略层：驱动匹配 ==================== */

/**
 * 为设备匹配驱动
 */
hic_status_t hardware_match_driver(device_t* device, driver_match_t* match);

/**
 * 批量匹配驱动
 */
hic_status_t hardware_match_all_drivers(driver_match_t* matches, u32* count);

/* ==================== 策略层：变化检测 ==================== */

/**
 * 注册硬件变化回调
 */
hic_status_t hardware_register_change_handler(hardware_change_handler_t handler, 
                                                void* context);

/**
 * 检测硬件变化
 */
hic_status_t hardware_detect_changes(void);

/* ==================== 策略层：配置 ==================== */

/**
 * 设置探测配置
 */
hic_status_t hardware_probe_set_config(const probe_config_t* config);

/**
 * 获取探测配置
 */
hic_status_t hardware_probe_get_config(probe_config_t* config);

#endif /* HIC_HARDWARE_PROBE_SERVICE_H */
