/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC硬件探测 - 机制层接口
 * 
 * 本文件仅包含机制层原语，不包含策略决策。
 * 策略层实现已移动到 Privileged-1/hardware_probe_service/
 * 
 * 机制层职责：
 * - 提供内存拓扑探测（PMM初始化必需）
 * - 提供PCI配置空间访问原语
 * - 提供CPUID访问原语
 * 
 * 策略层职责（Privileged-1/hardware_probe_service）：
 * - CPU详细信息探测
 * - PCI设备扫描策略
 * - ACPI信息解析
 * - 硬件信息缓存和管理
 * - 硬件变化通知
 */

#ifndef HIC_HARDWARE_PROBE_H
#define HIC_HARDWARE_PROBE_H

#include "types.h"
#include "formal_verification.h"

/* ==================== 机制层：内存拓扑探测 ==================== */

/* 内存拓扑信息（PMM初始化必需） */
typedef struct memory_topology {
    u64 total_usable;          /* 总可用内存 */
    u64 total_physical;        /* 总物理内存 */
    u32 region_count;          /* 区域数量 */
    mem_region_t regions[32];  /* 内存区域表 */
} memory_topology_t;

/* CPU信息结构（机制层和策略层共用） */
typedef struct cpu_info {
    u32 vendor_id[3];           /* 厂商ID */
    u32 version;               /* 版本信息 */
    u32 feature_flags[4];      /* 特性标志 */
    u8 family;                 /* CPU家族 */
    u8 model;                  /* CPU型号 */
    u8 stepping;               /* 步进 */
    u8 cache_line_size;        /* 缓存行大小 */
    u32 logical_cores;         /* 逻辑核心数 */
    u32 physical_cores;        /* 物理核心数 */
    u64 clock_frequency;       /* 时钟频率(Hz) */
    char brand_string[49];     /* 品牌字符串 */
    u32 arch_type;             /* 架构类型 */
} cpu_info_t;

/**
 * 探测内存拓扑（机制层）
 * 
 * 功能：获取物理内存布局
 * 这是PMM初始化的唯一必需探测
 * 
 * @param topo 输出拓扑信息
 */
void detect_memory_topology(memory_topology_t* topo);

/**
 * 探测最小CPU信息（机制层）
 * 
 * 仅获取启动必需的信息：核心数、品牌字符串
 * 完整探测由策略层完成
 * 
 * @param cpu 输出CPU信息
 */
void detect_cpu_info_minimal(cpu_info_t* cpu);

/* ==================== 机制层：PCI配置空间访问原语 ==================== */

/**
 * 读取PCI配置空间（机制层）
 * 
 * @param bus 总线号
 * @param device 设备号
 * @param function 功能号
 * @param offset 寄存器偏移
 * @return 配置空间值
 */
u32 pci_read_config(u8 bus, u8 device, u8 function, u8 offset);

/**
 * 写入PCI配置空间（机制层）
 * 
 * @param bus 总线号
 * @param device 设备号
 * @param function 功能号
 * @param offset 寄存器偏移
 * @param value 写入值
 */
void pci_write_config(u8 bus, u8 device, u8 function, u8 offset, u32 value);

/**
 * 读取PCI配置字节（机制层）
 */
u8 pci_read_config_byte(u8 bus, u8 device, u8 function, u8 offset);

/* ==================== 机制层：CPUID访问原语 ==================== */

/**
 * 执行CPUID指令（机制层）
 * 
 * @param leaf 功能号
 * @param subleaf 子功能号
 * @param eax 输出EAX
 * @param ebx 输出EBX
 * @param ecx 输出ECX
 * @param edx 输出EDX
 */
void cpuid_execute(u32 leaf, u32 subleaf, u32* eax, u32* ebx, u32* ecx, u32* edx);

/* ==================== 机制层：初始化 ==================== */

/**
 * 初始化硬件探测机制层
 * 
 * 功能：执行最小探测（仅内存拓扑）
 * 不执行：完整硬件探测（由策略层决定）
 */
void hardware_probe_mechanism_init(void);

/* ==================== 策略层使用的类型定义 ==================== */

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

/* 设备信息 */
typedef struct device {
    device_type_t type;
    u16 vendor_id;
    u16 device_id;
    u16 class_code;
    u8 revision;
    u64 base_address;
    u64 address_size;
    u8 irq;
    char name[64];
    
    union {
        struct {
            u8 bus;
            u8 device;
            u8 function;
            u32 bar[6];
            u32 bar_size[6];
        } pci;
        struct {
            u64 address;
            u32 id;
        } platform;
    };
} device_t;

/* 设备列表 */
typedef struct device_list {
    u32 device_count;
    u32 pci_count;
    device_t devices[64];
} device_list_t;

/* 中断控制器信息 */
typedef struct interrupt_controller {
    u64 base_address;
    u32 irq_base;
    u32 num_irqs;
    u8 enabled;
    char name[32];
} interrupt_controller_t;

/* 硬件探测完整结果（供策略层使用） */
typedef struct hardware_probe_result {
    cpu_info_t cpu;
    memory_topology_t memory;
    device_list_t devices;
    interrupt_controller_t local_irq;
    interrupt_controller_t io_irq;
    u8 smp_enabled;
} hardware_probe_result_t;

/* ==================== 供策略层调用的接口 ==================== */

/**
 * 获取已探测的内存拓扑（策略层调用）
 */
const memory_topology_t* hardware_probe_get_memory(void);

/**
 * 获取全局CPU信息缓存（策略层调用）
 */
cpu_info_t* hardware_probe_get_cpu_info(void);

/* 全局硬件信息变量 */
extern cpu_info_t g_cpu_info;

#endif /* HIC_HARDWARE_PROBE_H */
