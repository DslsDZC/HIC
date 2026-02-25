/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC无MMU架构内存管理
 * 遵循TD文档第12节：无MMU架构设计
 *
 * 核心设计：
 * 1. 物理空间扁平映射：虚拟地址 = 物理地址
 * 2. 静态内存布局：编译时确定内存区域分配
 * 3. MPU保护：使用MPU（如果支持）或软件能力系统保护
 * 4. 无地址转换：无TLB、无页表、无地址转换开销
 */

#ifndef HIC_KERNEL_NOMMU_H
#define HIC_KERNEL_NOMMU_H

#include "types.h"
#include "build_config.h"

#if CONFIG_MMU == 0

/* ===== 无MMU架构核心特性 ===== */

/**
 * 地址转换：无转换
 * 虚拟地址直接等于物理地址
 */
#define NOMMU_VIRT_TO_PHYS(vaddr)  ((phys_addr_t)(vaddr))
#define NOMMU_PHYS_TO_VIRT(paddr)  ((void *)(paddr))

/**
 * 内存映射：恒等映射
 * 所有地址都是物理地址，无需映射
 */
#define NOMMU_IS_MAPPED(addr)       (1)
#define NOMMU_MAP_PAGE(paddr, vaddr) ((void)(paddr), (void)(vaddr))
#define NOMMU_UNMAP_PAGE(addr)      ((void)(addr))

/* ===== 静态内存布局 ===== */

/**
 * 内存布局定义（编译时静态分配）
 * 地址范围说明：
 * - 0x00000000 - 0x000FFFFF: 保留（BIOS、IVT等）
 * - 0x00100000 - 0x003FFFFF: Core-0内核代码（2MB）
 * - 0x00400000 - 0x005FFFFF: Core-0内核数据（2MB）
 * - 0x00600000 - 0x007FFFFF: Core-0内核栈（2MB）
 * - 0x00800000 - 0x00FFFFFF: Privileged-1服务区域（8MB）
 * - 0x01000000 - 0x01FFFFFF: Application-3区域（16MB）
 * - 0x02000000 - 0x0FFFFFFF: 共享内存区域（224MB）
 * - 0x10000000 - 0xFFFFFFFF: 设备映射区域（3.75GB）
 */

/* Core-0内核区域 */
#define NOMMU_CORE0_CODE_BASE    0x00100000
#define NOMMU_CORE0_CODE_SIZE    (2 * 1024 * 1024)  /* 2MB */
#define NOMMU_CORE0_CODE_END     (NOMMU_CORE0_CODE_BASE + NOMMU_CORE0_CODE_SIZE - 1)

#define NOMMU_CORE0_DATA_BASE    0x00400000
#define NOMMU_CORE0_DATA_SIZE    (2 * 1024 * 1024)  /* 2MB */
#define NOMMU_CORE0_DATA_END     (NOMMU_CORE0_DATA_BASE + NOMMU_CORE0_DATA_SIZE - 1)

#define NOMMU_CORE0_STACK_BASE   0x00600000
#define NOMMU_CORE0_STACK_SIZE   (2 * 1024 * 1024)  /* 2MB */
#define NOMMU_CORE0_STACK_END    (NOMMU_CORE0_STACK_BASE + NOMMU_CORE0_STACK_SIZE - 1)

/* Privileged-1服务区域 */
#define NOMMU_PRIVILEGED_BASE    0x00800000
#define NOMMU_PRIVILEGED_SIZE    (8 * 1024 * 1024)  /* 8MB */
#define NOMMU_PRIVILEGED_END     (NOMMU_PRIVILEGED_BASE + NOMMU_PRIVILEGED_SIZE - 1)

/* Application-3区域 */
#define NOMMU_APPLICATION_BASE   0x01000000
#define NOMMU_APPLICATION_SIZE   (16 * 1024 * 1024) /* 16MB */
#define NOMMU_APPLICATION_END    (NOMMU_APPLICATION_BASE + NOMMU_APPLICATION_SIZE - 1)

/* 共享内存区域 */
#define NOMMU_SHARED_BASE        0x02000000
#define NOMMU_SHARED_SIZE        (224 * 1024 * 1024) /* 224MB */
#define NOMMU_SHARED_END         (NOMMU_SHARED_BASE + NOMMU_SHARED_SIZE - 1)

/* 设备映射区域 */
#define NOMMU_DEVICE_BASE        0x10000000
#define NOMMU_DEVICE_SIZE        (0xFFFFFFFF - NOMMU_DEVICE_BASE + 1) /* ~3.75GB */
#define NOMMU_DEVICE_END         0xFFFFFFFF

/* ===== 内存区域检查 ===== */

/**
 * 检查地址是否在Core-0区域
 */
static inline bool nommu_is_core0_region(phys_addr_t addr, size_t size)
{
    phys_addr_t end = addr + size - 1;
    return (addr >= NOMMU_CORE0_CODE_BASE && end <= NOMMU_CORE0_CODE_END) ||
           (addr >= NOMMU_CORE0_DATA_BASE && end <= NOMMU_CORE0_DATA_END) ||
           (addr >= NOMMU_CORE0_STACK_BASE && end <= NOMMU_CORE0_STACK_END);
}

/**
 * 检查地址是否在Privileged-1区域
 */
static inline bool nommu_is_privileged_region(phys_addr_t addr, size_t size)
{
    phys_addr_t end = addr + size - 1;
    return (addr >= NOMMU_PRIVILEGED_BASE && end <= NOMMU_PRIVILEGED_END);
}

/**
 * 检查地址是否在Application-3区域
 */
static inline bool nommu_is_application_region(phys_addr_t addr, size_t size)
{
    phys_addr_t end = addr + size - 1;
    return (addr >= NOMMU_APPLICATION_BASE && end <= NOMMU_APPLICATION_END);
}

/**
 * 检查地址是否在共享内存区域
 */
static inline bool nommu_is_shared_region(phys_addr_t addr, size_t size)
{
    phys_addr_t end = addr + size - 1;
    return (addr >= NOMMU_SHARED_BASE && end <= NOMMU_SHARED_END);
}

/**
 * 检查地址是否在设备映射区域
 */
static inline bool nommu_is_device_region(phys_addr_t addr, size_t size)
{
    phys_addr_t end = addr + size - 1;
    return (addr >= NOMMU_DEVICE_BASE && end <= NOMMU_DEVICE_END);
}

/* ===== 无MMU架构初始化 ===== */

/**
 * 初始化无MMU架构
 * - 设置静态内存布局
 * - 配置MPU（如果支持）
 * - 设置内存保护
 */
void nommu_init(void);

/* ===== MPU保护机制 ===== */

#if CONFIG_MPU == 1

/**
 * MPU区域描述符
 */
typedef struct nommu_mpu_region {
    phys_addr_t base;          /* 基地址 */
    size_t     size;           /* 大小 */
    u32        permissions;    /* 权限：读/写/执行 */
    bool       enabled;        /* 是否启用 */
} nommu_mpu_region_t;

/**
 * MPU权限定义
 */
#define MPU_PERM_READ     (1U << 0)
#define MPU_PERM_WRITE    (1U << 1)
#define MPU_PERM_EXECUTE  (1U << 2)

/**
 * 配置MPU区域
 */
hic_status_t nommu_mpu_config_region(u32 region_id, phys_addr_t base,
                                     size_t size, u32 permissions);

/**
 * 启用MPU
 */
void nommu_mpu_enable(void);

/**
 * 禁用MPU
 */
void nommu_mpu_disable(void);

#endif /* CONFIG_MPU */

/* ===== 软件能力保护 ===== */

/**
 * 软件能力检查（无MMU时使用）
 * 用于检查域对内存的访问权限
 */
bool nommu_capability_check(domain_id_t domain, phys_addr_t addr,
                            size_t size, u32 access_type);

/* ===== 无MMU架构统计 ===== */

/**
 * 获取无MMU架构内存布局信息
 */
void nommu_get_layout_info(u64 *core0_size, u64 *privileged_size,
                           u64 *application_size, u64 *shared_size);

#endif /* CONFIG_MMU == 0 */

#endif /* HIC_KERNEL_NOMMU_H */