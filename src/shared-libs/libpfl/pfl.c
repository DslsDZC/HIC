/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * PFL (Platform Features Layer) - 共享库实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* 系统调用号 */
#define SYSCALL_PFL_BASE           0x1300

#define SYSCALL_PFL_PCI_READ       (SYSCALL_PFL_BASE + 0)
#define SYSCALL_PFL_PCI_WRITE      (SYSCALL_PFL_BASE + 1)
#define SYSCALL_PFL_PCI_SCAN       (SYSCALL_PFL_BASE + 2)
#define SYSCALL_PFL_PCI_FIND       (SYSCALL_PFL_BASE + 3)
#define SYSCALL_PFL_PCI_GET_BAR    (SYSCALL_PFL_BASE + 4)
#define SYSCALL_PFL_PCI_BUS_MASTER (SYSCALL_PFL_BASE + 5)
#define SYSCALL_PFL_ACPI_FIND      (SYSCALL_PFL_BASE + 10)
#define SYSCALL_PFL_ACPI_GET_RSDP  (SYSCALL_PFL_BASE + 11)
#define SYSCALL_PFL_ACPI_GET_MADT  (SYSCALL_PFL_BASE + 12)
#define SYSCALL_PFL_IRQ_GET_INFO   (SYSCALL_PFL_BASE + 20)
#define SYSCALL_PFL_IRQ_SET_HANDLER (SYSCALL_PFL_BASE + 21)
#define SYSCALL_PFL_IRQ_ENABLE     (SYSCALL_PFL_BASE + 22)
#define SYSCALL_PFL_IRQ_DISABLE    (SYSCALL_PFL_BASE + 23)
#define SYSCALL_PFL_IRQ_EOI        (SYSCALL_PFL_BASE + 24)
#define SYSCALL_PFL_TIMER_GET_FREQ (SYSCALL_PFL_BASE + 30)
#define SYSCALL_PFL_TIMER_SET_HANDLER (SYSCALL_PFL_BASE + 31)
#define SYSCALL_PFL_TIMER_SET_MODE (SYSCALL_PFL_BASE + 32)
#define SYSCALL_PFL_TIMER_READ     (SYSCALL_PFL_BASE + 33)
#define SYSCALL_PFL_SERIAL_INIT    (SYSCALL_PFL_BASE + 40)
#define SYSCALL_PFL_SERIAL_PUTS    (SYSCALL_PFL_BASE + 41)
#define SYSCALL_PFL_SERIAL_GETC    (SYSCALL_PFL_BASE + 42)
#define SYSCALL_PFL_DISPLAY_INIT   (SYSCALL_PFL_BASE + 50)
#define SYSCALL_PFL_DISPLAY_GET_MODE (SYSCALL_PFL_BASE + 51)
#define SYSCALL_PFL_GET_CPU_INFO   (SYSCALL_PFL_BASE + 60)
#define SYSCALL_PFL_GET_MEM_MAP    (SYSCALL_PFL_BASE + 61)
#define SYSCALL_PFL_GET_PLATFORM_ID (SYSCALL_PFL_BASE + 62)

extern long syscall0(long num);
extern long syscall1(long num, long a1);
extern long syscall2(long num, long a1, long a2);
extern long syscall3(long num, long a1, long a2, long a3);
extern long syscall4(long num, long a1, long a2, long a3, long a4);
extern long syscall5(long num, long a1, long a2, long a3, long a4, long a5);

/* ==================== 类型定义 ==================== */

typedef enum {
    PFL_OK = 0,
    PFL_ERR_INVALID_PARAM = 1,
    PFL_ERR_NOT_FOUND = 2,
    PFL_ERR_NOT_SUPPORTED = 3,
} pfl_error_t;

/* ==================== PCI 操作 ==================== */

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

uint32_t pfl_pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    return (uint32_t)syscall4(SYSCALL_PFL_PCI_READ, bus, dev, func, offset);
}

void pfl_pci_write_config(uint8_t bus, uint8_t dev, uint8_t func,
                           uint8_t offset, uint32_t value) {
    syscall5(SYSCALL_PFL_PCI_WRITE, bus, dev, func, offset, value);
}

uint32_t pfl_pci_scan_bus(pfl_pci_device_t *devices, uint32_t max_devices) {
    return (uint32_t)syscall2(SYSCALL_PFL_PCI_SCAN, (long)devices, (long)max_devices);
}

pfl_error_t pfl_pci_find_device(uint16_t vendor_id, uint16_t device_id,
                                  pfl_pci_device_t *device) {
    return (pfl_error_t)syscall3(SYSCALL_PFL_PCI_FIND, vendor_id, device_id, (long)device);
}

pfl_error_t pfl_pci_get_bar(pfl_pci_device_t *device, uint8_t bar_index,
                             uint64_t *addr, uint64_t *size) {
    return (pfl_error_t)syscall4(SYSCALL_PFL_PCI_GET_BAR, (long)device, bar_index, (long)addr, (long)size);
}

pfl_error_t pfl_pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func) {
    return (pfl_error_t)syscall3(SYSCALL_PFL_PCI_BUS_MASTER, bus, dev, func);
}

/* ==================== ACPI 操作 ==================== */

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

pfl_error_t pfl_acpi_find_table(const char *signature, pfl_acpi_header_t **table) {
    return (pfl_error_t)syscall2(SYSCALL_PFL_ACPI_FIND, (long)signature, (long)table);
}

uint64_t pfl_acpi_get_rsdp(void) {
    return (uint64_t)syscall0(SYSCALL_PFL_ACPI_GET_RSDP);
}

typedef struct {
    uint64_t local_apic_addr;
    uint32_t local_apic_count;
    uint32_t io_apic_count;
    uint32_t int_override_count;
} pfl_madt_info_t;

pfl_error_t pfl_acpi_get_madt(pfl_madt_info_t *info) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_ACPI_GET_MADT, (long)info);
}

/* ==================== 中断控制器 ==================== */

typedef struct {
    uint64_t base_address;
    uint32_t irq_base;
    uint32_t num_irqs;
    uint8_t enabled;
    char name[32];
} pfl_irq_controller_t;

pfl_error_t pfl_irq_get_info(pfl_irq_controller_t *local, pfl_irq_controller_t *io) {
    return (pfl_error_t)syscall2(SYSCALL_PFL_IRQ_GET_INFO, (long)local, (long)io);
}

pfl_error_t pfl_irq_set_handler(uint32_t irq, void *handler) {
    return (pfl_error_t)syscall2(SYSCALL_PFL_IRQ_SET_HANDLER, irq, (long)handler);
}

void pfl_irq_enable(uint32_t irq) {
    syscall1(SYSCALL_PFL_IRQ_ENABLE, irq);
}

void pfl_irq_disable(uint32_t irq) {
    syscall1(SYSCALL_PFL_IRQ_DISABLE, irq);
}

void pfl_irq_eoi(uint32_t irq) {
    syscall1(SYSCALL_PFL_IRQ_EOI, irq);
}

/* ==================== 定时器 ==================== */

typedef enum {
    PFL_TIMER_MODE_PERIODIC = 0,
    PFL_TIMER_MODE_ONESHOT = 1,
} pfl_timer_mode_t;

uint64_t pfl_timer_get_freq(void) {
    return (uint64_t)syscall0(SYSCALL_PFL_TIMER_GET_FREQ);
}

pfl_error_t pfl_timer_set_handler(void *handler) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_TIMER_SET_HANDLER, (long)handler);
}

pfl_error_t pfl_timer_set_mode(pfl_timer_mode_t mode, uint64_t interval_us) {
    return (pfl_error_t)syscall2(SYSCALL_PFL_TIMER_SET_MODE, mode, interval_us);
}

uint64_t pfl_timer_read(void) {
    return (uint64_t)syscall0(SYSCALL_PFL_TIMER_READ);
}

/* ==================== 串口 ==================== */

typedef struct {
    uint64_t base_addr;
    uint32_t baud_rate;
    uint32_t data_bits;
    uint32_t parity;
    uint32_t stop_bits;
} pfl_serial_config_t;

pfl_error_t pfl_serial_init(pfl_serial_config_t *config) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_SERIAL_INIT, (long)config);
}

void pfl_serial_puts(const char *str) {
    syscall1(SYSCALL_PFL_SERIAL_PUTS, (long)str);
}

char pfl_serial_getc(void) {
    return (char)syscall0(SYSCALL_PFL_SERIAL_GETC);
}

pfl_error_t pfl_serial_get_config(pfl_serial_config_t *config) {
    (void)config;
    return PFL_ERR_NOT_SUPPORTED;
}

/* ==================== 显示 ==================== */

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint64_t framebuffer;
    uint32_t flags;
} pfl_display_mode_t;

pfl_error_t pfl_display_init(void) {
    return (pfl_error_t)syscall0(SYSCALL_PFL_DISPLAY_INIT);
}

pfl_error_t pfl_display_get_mode(pfl_display_mode_t *mode) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_DISPLAY_GET_MODE, (long)mode);
}

/* ==================== 平台信息 ==================== */

typedef struct {
    uint32_t vendor_id[3];
    uint32_t version;
    uint32_t feature_flags[4];
    uint8_t family;
    uint8_t model;
    uint8_t stepping;
    char brand_string[49];
} pfl_cpu_info_t;

pfl_error_t pfl_get_cpu_info(pfl_cpu_info_t *info) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_GET_CPU_INFO, (long)info);
}

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t flags;
} pfl_mem_region_t;

typedef struct {
    uint32_t region_count;
    pfl_mem_region_t regions[32];
} pfl_mem_map_t;

pfl_error_t pfl_get_mem_map(pfl_mem_map_t *map) {
    return (pfl_error_t)syscall1(SYSCALL_PFL_GET_MEM_MAP, (long)map);
}

pfl_error_t pfl_get_platform_id(char *buffer, uint32_t size) {
    return (pfl_error_t)syscall2(SYSCALL_PFL_GET_PLATFORM_ID, (long)buffer, size);
}

/* ==================== 库加载辅助 ==================== */

typedef struct pfl_lib_handle pfl_lib_handle_t;

pfl_lib_handle_t *pfl_lib_load(void) { return 0; }
void pfl_lib_unload(pfl_lib_handle_t *handle) { (void)handle; }
uint32_t pfl_lib_get_version(pfl_lib_handle_t *handle) { (void)handle; return 0; }
