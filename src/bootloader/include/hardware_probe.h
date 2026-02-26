#ifndef HIC_BOOTLOADER_HARDWARE_PROBE_H
#define HIC_BOOTLOADER_HARDWARE_PROBE_H

#include <stdint.h>
#include "boot_info.h"

// CPU信息结构（架构无关）
typedef struct {
    uint32_t vendor_id[3];           // 厂商ID
    uint32_t version;               // 版本信息
    uint32_t feature_flags[4];      // 特性标志（架构相关）
    uint8_t family;                 // CPU家族
    uint8_t model;                  // CPU型号
    uint8_t stepping;               // 步进
    uint8_t cache_line_size;        // 缓存行大小
    uint32_t logical_cores;         // 逻辑核心数
    uint32_t physical_cores;        // 物理核心数
    uint64_t clock_frequency;       // 时钟频率(Hz)
    char brand_string[49];          // 品牌字符串
    uint32_t arch_type;             // 架构类型（枚举值）
} cpu_info_t;

// 设备类型（架构无关）
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

// PCI设备类型
typedef enum {
    PCI_CLASS_STORAGE,
    PCI_CLASS_NETWORK,
    PCI_CLASS_DISPLAY,
    PCI_CLASS_MULTIMEDIA,
    PCI_CLASS_BRIDGE,
    PCI_CLASS_SERIAL,
    PCI_CLASS_SYSTEM,
    PCI_CLASS_INPUT,
    PCI_CLASS_UNKNOWN
} pci_class_t;

// 设备信息（架构无关）
typedef struct {
    device_type_t type;        // 设备类型
    uint16_t vendor_id;        // 厂商ID
    uint16_t device_id;        // 设备ID
    uint16_t class_code;       // 类代码
    uint8_t revision;          // 版本号
    uint64_t base_address;     // 基地址
    uint64_t address_size;     // 地址空间大小
    uint8_t irq;               // IRQ号
    char name[64];             // 设备名称
    
    // 架构特定数据
    union {
        struct {
            uint8_t bus;            // 总线号
            uint8_t device;         // 设备号
            uint8_t function;       // 功能号
            uint32_t bar[6];        // 基址寄存器
            uint32_t bar_size[6];   // BAR大小
        } pci;
        struct {
            uint64_t address;       // 平台设备地址
            uint32_t id;            // 设备ID
        } platform;
    };
} device_t;

// 设备列表
typedef struct {
    uint32_t device_count;          // 设备数量
    uint32_t pci_count;             // PCI设备数量
    device_t devices[64];           // 设备数组
} device_list_t;

// 中断控制器信息（架构无关）
typedef struct {
    uint64_t base_address;          // 控制器基地址
    uint32_t irq_base;              // IRQ基地址
    uint32_t num_irqs;              // IRQ数量
    uint8_t enabled;                // 是否启用
    char name[32];                  // 控制器名称
} interrupt_controller_t;

// 硬件探测结果
typedef struct {
    cpu_info_t cpu;                // CPU信息
    memory_map_entry_t *memory;     // 内存拓扑
    uint32_t memory_count;          // 内存区域数量
    device_list_t devices;          // 设备列表
    interrupt_controller_t local_irq;  // 本地中断控制器
    interrupt_controller_t io_irq;     // I/O中断控制器
    uint8_t smp_enabled;            // SMP是否启用
} hardware_probe_result_t;

// 探测函数声明
void hardware_probe_init(hic_boot_info_t *boot_info);
void probe_cpu(cpu_info_t *result);
void probe_memory_topology(hic_boot_info_t *boot_info);
void probe_pci_devices(device_list_t *result);
void probe_interrupt_controller(interrupt_controller_t *local, 
                                 interrupt_controller_t *io);
void hardware_probe_all(hardware_probe_result_t *result);

#endif // HIC_BOOTLOADER_HARDWARE_PROBE_H