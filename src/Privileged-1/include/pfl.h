/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * PFL (Platform Features Layer) - 服务侧共享库接口
 * 
 * PFL 作为共享库（.hiclib）实现，封装平台特定硬件组件。
 * 
 * 功能：
 * - PCI/PCIe 设备
 * - ACPI 表
 * - 中断控制器
 * - 定时器
 * - 串口/显示
 * - 平台信息
 */

#ifndef HIC_SERVICE_PFL_H
#define HIC_SERVICE_PFL_H

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"
#include "hiclib.h"

/* ==================== 库标识 ==================== */

/* PFL 库 UUID */
#define PFL_LIB_UUID \
    { 0x1F, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
      0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }

#define PFL_LIB_NAME    "libpfl"
#define PFL_LIB_VERSION HICLIB_PACK_VERSION(1, 0, 0)

/* ==================== 错误码 ==================== */

typedef enum {
    PFL_OK = 0,
    PFL_ERR_INVALID_PARAM = 1,
    PFL_ERR_NOT_FOUND = 2,
    PFL_ERR_NOT_SUPPORTED = 3,
    PFL_ERR_NO_MEMORY = 4,
    PFL_ERR_IO_ERROR = 5,
    PFL_ERR_BUSY = 6,
} pfl_error_t;

/* ==================== PCI 操作 ==================== */

/**
 * PCI 设备信息
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class_code;
    uint8_t revision;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t irq;
    uint32_t bar[6];
    uint32_t bar_size[6];
} pfl_pci_device_t;

/**
 * 读取 PCI 配置空间
 */
uint32_t pfl_pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/**
 * 写入 PCI 配置空间
 */
void pfl_pci_write_config(uint8_t bus, uint8_t dev, uint8_t func, 
                           uint8_t offset, uint32_t value);

/**
 * 扫描 PCI 总线
 */
uint32_t pfl_pci_scan_bus(pfl_pci_device_t *devices, uint32_t max_devices);

/**
 * 查找 PCI 设备
 */
pfl_error_t pfl_pci_find_device(uint16_t vendor_id, uint16_t device_id,
                                  pfl_pci_device_t *device);

/**
 * 获取 BAR 信息
 */
pfl_error_t pfl_pci_get_bar(pfl_pci_device_t *device, uint8_t bar_index,
                             uint64_t *addr, uint64_t *size);

/**
 * 启用 Bus Master
 */
pfl_error_t pfl_pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func);

/* ==================== ACPI 操作 ==================== */

/**
 * ACPI 表头
 */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) pfl_acpi_header_t;

/**
 * 查找 ACPI 表
 */
pfl_error_t pfl_acpi_find_table(const char *signature, pfl_acpi_header_t **table);

/**
 * 获取 RSDP 地址
 */
uint64_t pfl_acpi_get_rsdp(void);

/**
 * MADT 信息
 */
typedef struct {
    uint64_t local_apic_addr;
    uint32_t local_apic_count;
    uint32_t io_apic_count;
    uint32_t int_override_count;
} pfl_madt_info_t;

/**
 * 获取 MADT 信息
 */
pfl_error_t pfl_acpi_get_madt(pfl_madt_info_t *info);

/* ==================== 中断控制器 ==================== */

/**
 * 中断控制器信息
 */
typedef struct {
    uint64_t base_address;
    uint32_t irq_base;
    uint32_t num_irqs;
    uint8_t enabled;
    char name[32];
} pfl_irq_controller_t;

/**
 * 获取中断控制器信息
 */
pfl_error_t pfl_irq_get_info(pfl_irq_controller_t *local, pfl_irq_controller_t *io);

/**
 * 设置中断处理程序
 */
pfl_error_t pfl_irq_set_handler(uint32_t irq, void *handler);

/**
 * 启用中断
 */
void pfl_irq_enable(uint32_t irq);

/**
 * 禁用中断
 */
void pfl_irq_disable(uint32_t irq);

/**
 * 中断结束
 */
void pfl_irq_eoi(uint32_t irq);

/* ==================== 定时器 ==================== */

/**
 * 定时器模式
 */
typedef enum {
    PFL_TIMER_MODE_PERIODIC = 0,
    PFL_TIMER_MODE_ONESHOT = 1,
    PFL_TIMER_MODE_DEADLINE = 2,
} pfl_timer_mode_t;

/**
 * 获取定时器频率
 */
uint64_t pfl_timer_get_freq(void);

/**
 * 设置定时器处理程序
 */
pfl_error_t pfl_timer_set_handler(void *handler);

/**
 * 设置定时器模式
 */
pfl_error_t pfl_timer_set_mode(pfl_timer_mode_t mode, uint64_t interval_us);

/**
 * 读取定时器值
 */
uint64_t pfl_timer_read(void);

/* ==================== 串口 ==================== */

/**
 * 串口配置
 */
typedef struct {
    uint64_t base_addr;
    uint32_t baud_rate;
    uint32_t data_bits;
    uint32_t parity;
    uint32_t stop_bits;
} pfl_serial_config_t;

/**
 * 初始化串口
 */
pfl_error_t pfl_serial_init(pfl_serial_config_t *config);

/**
 * 串口输出字符串
 */
void pfl_serial_puts(const char *str);

/**
 * 串口读取字符
 */
char pfl_serial_getc(void);

/**
 * 获取串口配置
 */
pfl_error_t pfl_serial_get_config(pfl_serial_config_t *config);

/* ==================== 显示 ==================== */

/**
 * 显示模式
 */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint64_t framebuffer;
    uint32_t flags;
} pfl_display_mode_t;

/**
 * 初始化显示
 */
pfl_error_t pfl_display_init(void);

/**
 * 获取显示模式
 */
pfl_error_t pfl_display_get_mode(pfl_display_mode_t *mode);

/* ==================== 平台信息 ==================== */

/**
 * CPU 信息
 */
typedef struct {
    uint32_t vendor_id[3];
    uint32_t version;
    uint32_t feature_flags[4];
    uint8_t family;
    uint8_t model;
    uint8_t stepping;
    uint8_t cache_line_size;
    uint32_t logical_cores;
    uint32_t physical_cores;
    uint64_t clock_frequency;
    char brand_string[49];
} pfl_cpu_info_t;

/**
 * 获取 CPU 信息
 */
pfl_error_t pfl_get_cpu_info(pfl_cpu_info_t *info);

/**
 * 内存区域
 */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;    /* 1=可用, 2=保留, 3=ACPI, 4=NVS, 5=损坏 */
    uint32_t flags;
} pfl_mem_region_t;

/**
 * 内存映射
 */
typedef struct {
    uint32_t region_count;
    pfl_mem_region_t regions[32];
} pfl_mem_map_t;

/**
 * 获取内存映射
 */
pfl_error_t pfl_get_mem_map(pfl_mem_map_t *map);

/**
 * 获取平台标识
 */
pfl_error_t pfl_get_platform_id(char *buffer, uint32_t size);

/* ==================== 错误检查辅助 ==================== */

static inline bool pfl_ok(pfl_error_t err) { return err == PFL_OK; }
static inline bool pfl_fail(pfl_error_t err) { return err != PFL_OK; }

/* ==================== 库加载辅助 ==================== */

typedef struct pfl_lib_handle pfl_lib_handle_t;

pfl_lib_handle_t *pfl_lib_load(void);
void pfl_lib_unload(pfl_lib_handle_t *handle);
uint32_t pfl_lib_get_version(pfl_lib_handle_t *handle);

#endif /* HIC_SERVICE_PFL_H */