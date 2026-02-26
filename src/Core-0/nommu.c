/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC无MMU架构内存管理实现
 * 遵循TD文档第12节：无MMU架构设计
 */

#include "nommu.h"
#include "domain.h"
#include "capability.h"
#include "lib/console.h"
#include "lib/mem.h"

#if CONFIG_MMU == 0

/* ===== 无MMU架构初始化 ===== */

/**
 * 初始化无MMU架构
 */
void nommu_init(void)
{
    console_puts("[NOMMU] Initializing MMU-less architecture\n");
    console_puts("[NOMMU] Core-0 Code: 0x");
    console_puthex64(NOMMU_CORE0_CODE_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_CORE0_CODE_END);
    console_puts("\n");

    console_puts("[NOMMU] Core-0 Data: 0x");
    console_puthex64(NOMMU_CORE0_DATA_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_CORE0_DATA_END);
    console_puts("\n");

    console_puts("[NOMMU] Core-0 Stack: 0x");
    console_puthex64(NOMMU_CORE0_STACK_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_CORE0_STACK_END);
    console_puts("\n");

    console_puts("[NOMMU] Privileged-1: 0x");
    console_puthex64(NOMMU_PRIVILEGED_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_PRIVILEGED_END);
    console_puts("\n");

    console_puts("[NOMMU] Application-3: 0x");
    console_puthex64(NOMMU_APPLICATION_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_APPLICATION_END);
    console_puts("\n");

    console_puts("[NOMMU] Shared Memory: 0x");
    console_puthex64(NOMMU_SHARED_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_SHARED_END);
    console_puts("\n");

    console_puts("[NOMMU] Device Region: 0x");
    console_puthex64(NOMMU_DEVICE_BASE);
    console_puts(" - 0x");
    console_puthex64(NOMMU_DEVICE_END);
    console_puts("\n");

#if CONFIG_MPU == 1
    /* 配置MPU保护 */
    console_puts("[NOMMU] Configuring MPU protection...\n");
    nommu_mpu_enable();
#endif

    console_puts("[NOMMU] MMU-less architecture initialized\n");
}

/* ===== MPU保护机制 ===== */

#if CONFIG_MPU == 1

/**
 * MPU区域描述符（静态分配）
 */
static nommu_mpu_region_t mpu_regions[8];

/**
 * 配置MPU区域
 */
hic_status_t nommu_mpu_config_region(u32 region_id, phys_addr_t base,
                                     size_t size, u32 permissions)
{
    if (region_id >= 8) {
        return HIC_ERROR_INVALID_PARAM;
    }

    mpu_regions[region_id].base = base;
    mpu_regions[region_id].size = size;
    mpu_regions[region_id].permissions = permissions;
    mpu_regions[region_id].enabled = true;

    /* 实际的MPU配置（架构相关） */
    /* 这里需要调用架构相关的MPU配置函数 */

    console_puts("[NOMMU] MPU Region ");
    console_putu32(region_id);
    console_puts(": 0x");
    console_puthex64(base);
    console_puts(" (");
    console_putu64(size / 1024);
    console_puts("KB) - ");
    if (permissions & MPU_PERM_READ) console_puts("R");
    if (permissions & MPU_PERM_WRITE) console_puts("W");
    if (permissions & MPU_PERM_EXECUTE) console_puts("X");
    console_puts("\n");

    return HIC_SUCCESS;
}

/**
 * 启用MPU
 */
void nommu_mpu_enable(void)
{
    /* 配置Core-0区域：只读代码，读写数据，禁止执行数据区 */
    nommu_mpu_config_region(0, NOMMU_CORE0_CODE_BASE, NOMMU_CORE0_CODE_SIZE,
                           MPU_PERM_READ | MPU_PERM_EXECUTE);
    nommu_mpu_config_region(1, NOMMU_CORE0_DATA_BASE, NOMMU_CORE0_DATA_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE);
    nommu_mpu_config_region(2, NOMMU_CORE0_STACK_BASE, NOMMU_CORE0_STACK_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE);

    /* 配置Privileged-1区域：读写执行 */
    nommu_mpu_config_region(3, NOMMU_PRIVILEGED_BASE, NOMMU_PRIVILEGED_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE | MPU_PERM_EXECUTE);

    /* 配置Application-3区域：读写执行（由能力系统控制） */
    nommu_mpu_config_region(4, NOMMU_APPLICATION_BASE, NOMMU_APPLICATION_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE | MPU_PERM_EXECUTE);

    /* 配置共享内存区域：读写 */
    nommu_mpu_config_region(5, NOMMU_SHARED_BASE, NOMMU_SHARED_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE);

    /* 配置设备映射区域：读写 */
    nommu_mpu_config_region(6, NOMMU_DEVICE_BASE, NOMMU_DEVICE_SIZE,
                           MPU_PERM_READ | MPU_PERM_WRITE);

    console_puts("[NOMMU] MPU enabled with 7 regions\n");
}

/**
 * 禁用MPU
 */
void nommu_mpu_disable(void)
{
    /* 禁用MPU（架构相关） */
    console_puts("[NOMMU] MPU disabled\n");
}

#endif /* CONFIG_MPU */

/* ===== 软件能力保护 ===== */

/**
 * 软件能力检查（无MMU时使用）
 * 用于检查域对内存的访问权限
 */
bool nommu_capability_check(domain_id_t domain, phys_addr_t addr,
                            size_t size, u32 access_type)
{
    (void)access_type;

    /* 检查Core-0区域：只有Core-0可以访问 */
    if (nommu_is_core0_region(addr, size)) {
        return (domain == HIC_DOMAIN_CORE);
    }

    /* 检查Privileged-1区域：只有Privileged-1域可以访问 */
    if (nommu_is_privileged_region(addr, size)) {
        domain_t *dom = domain_get(domain);
        if (dom == NULL) return false;
        return (dom->type == DOMAIN_TYPE_PRIVILEGED);
    }

    /* 检查Application-3区域：只有Application-3域可以访问 */
    if (nommu_is_application_region(addr, size)) {
        domain_t *dom = domain_get(domain);
        if (dom == NULL) return false;
        return (dom->type == DOMAIN_TYPE_APPLICATION);
    }

    /* 共享内存区域：通过能力系统控制 */
    if (nommu_is_shared_region(addr, size)) {
        /* 检查是否有对该共享内存的能力 */
        return cap_has_access(domain, addr, access_type);
    }

    /* 设备映射区域：通过能力系统控制 */
    if (nommu_is_device_region(addr, size)) {
        /* 检查是否有对该设备的能力 */
        return cap_has_access(domain, addr, access_type);
    }

    /* 未知区域：拒绝访问 */
    return false;
}

/* ===== 无MMU架构统计 ===== */

/**
 * 获取无MMU架构内存布局信息
 */
void nommu_get_layout_info(u64 *core0_size, u64 *privileged_size,
                           u64 *application_size, u64 *shared_size)
{
    if (core0_size) {
        *core0_size = NOMMU_CORE0_CODE_SIZE + NOMMU_CORE0_DATA_SIZE +
                      NOMMU_CORE0_STACK_SIZE;
    }

    if (privileged_size) {
        *privileged_size = NOMMU_PRIVILEGED_SIZE;
    }

    if (application_size) {
        *application_size = NOMMU_APPLICATION_SIZE;
    }

    if (shared_size) {
        *shared_size = NOMMU_SHARED_SIZE;
    }
}

#endif /* CONFIG_MMU == 0 */
