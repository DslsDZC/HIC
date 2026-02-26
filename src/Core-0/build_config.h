/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC构建时配置数据结构
 * 遵循文档第4节：构建时硬件合成系统
 */

#ifndef HIC_BUILD_CONFIG_H
#define HIC_BUILD_CONFIG_H

#include "types.h"
#include "capability.h"
#include "hardware_probe.h"  /* 包含 cpu_info_t 定义 */

/* 内存拓扑 */
typedef struct mem_topology {
    u64 total_memory;
    u64 num_memory_regions;
    struct {
        phys_addr_t base;
        size_t      size;
        u32         type;  /* DDR, HBM, etc */
    } regions[16];
} mem_topology_t;

/* 设备信息 */
typedef struct device_info {
    char   name[64];
    char   type[32];        /* "pci", "mmio", "irq" */
    phys_addr_t mmio_base;
    size_t      mmio_size;
    u32         irq_vector;
    u64         dma_range_base;
    u64         dma_range_size;
} device_info_t;

/* 服务依赖 */
typedef struct service_dependency {
    char   service_uuid[37];  /* UUID字符串 */
    char   version[32];
} service_dependency_t;

/* 系统服务配置 */
typedef struct service_config {
    char   name[64];
    char   uuid[37];
    char   version[32];
    char   type[32];          /* "driver", "protocol", "fs" */
    
    /* 资源需求 */
    size_t max_memory;
    u32    max_threads;
    u32    max_caps;
    u32    cpu_quota_percent;
    
    /* 依赖 */
    service_dependency_t dependencies[8];
    u32    num_dependencies;
    
    /* 设备绑定 */
    char   device_name[64];   /* 绑定的设备名称 */
    
    /* 初始化顺序 */
    u32    init_order;
} service_config_t;

/* 内存布局表项 */
typedef struct memory_layout_entry {
    domain_id_t   domain_id;
    char         domain_name[64];
    phys_addr_t   phys_base;
    size_t        phys_size;
    virt_addr_t   virt_base;  /* 用于Application的虚拟地址 */
    u32           flags;
#define MEM_LAYOUT_FLAG_CODE    (1 << 0)
#define MEM_LAYOUT_FLAG_DATA    (1 << 1)
#define MEM_LAYOUT_FLAG_SHARED  (1 << 2)
} memory_layout_entry_t;

/* 中断路由表项 */
typedef struct interrupt_route {
    u32           irq_vector;
    domain_id_t   target_domain;
    u64           handler_address;  /* 服务内的处理函数地址 */
    u32           flags;
#define IRQ_FLAG_EDGE   (1 << 0)
#define IRQ_FLAG_LEVEL  (1 << 1)
#define IRQ_FLAG_SHARED (1 << 2)
} interrupt_route_t;

/* 能力初始分配表项 */
typedef struct capability_allocation {
    domain_id_t   domain_id;
    cap_type_t    cap_type;
    cap_rights_t  rights;
    phys_addr_t   base;      /* 对于内存/MMIO */
    size_t        size;      /* 对于内存/MMIO */
    irq_vector_t  irq_vector; /* 对于IRQ */
} capability_allocation_t;

/* 设备初始化序列 */
typedef struct device_init_sequence {
    char   device_name[64];
    u32    init_order;
    char   dependency[64];   /* 必须先初始化的设备 */
} device_init_sequence_t;

/* 构建时配置 */
typedef struct build_config {
    /* 构建选项 */
    u32 target_architecture;
    
    u32 build_mode;
#define BUILD_MODE_STATIC  0
#define BUILD_MODE_DYNAMIC 1
    
    /* 硬件信息 */
    cpu_info_t          cpu_info;
    mem_topology_t      mem_topology;
    device_info_t       devices[32];
    u32                 num_devices;
    
    /* 内存区域 */
    mem_region_t        memory_regions[32];   /* 从128减小到32 */
    u32                 num_memory_regions;
    
    /* 服务配置 */
    service_config_t    services[8];         /* 从16减小到8 */
    u32                 num_services;
    
    /* 生成的静态配置 */
    memory_layout_entry_t  memory_layout[16]; /* 从64减小到16 */
    u32                    num_memory_layouts;
    
    interrupt_route_t     interrupt_routes[64]; /* 从256减小到64 */
    u32                   num_interrupt_routes;
    
    capability_allocation_t capability_allocs[64]; /* 从256减小到64 */
    u32                    num_capability_allocs;
    
    device_init_sequence_t device_init_seq[32];
    u32                    num_device_init_seq;
    
    /* 安全策略 */
    u32 security_policy;
#define SECURITY_POLICY_STRICT    (1 << 0)
#define SECURITY_POLICY_PERMISSIVE (1 << 1)
} build_config_t;

/* 构建配置全局变量 */
extern build_config_t g_build_config;

/* 初始化构建配置 */
void build_config_init(void);

/* 加载platform.yaml */
hic_status_t build_config_load_yaml(const char *filename);

/* 解析和验证配置 */
hic_status_t build_config_parse_and_validate(void);

/* 生成静态配置表 */
hic_status_t build_config_generate_tables(void);

/* 解决资源冲突 */
hic_status_t build_config_resolve_conflicts(void);

#endif /* HIC_BUILD_CONFIG_H */