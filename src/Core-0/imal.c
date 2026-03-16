/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 隔离机制抽象层实现
 * 
 * 提供跨 MMU/MPU/noMMU 变体的统一初始化和管理
 */

#include "imal.h"
#include "lib/console.h"

#if CONFIG_MMU == 1
#include "pagetable.h"
#elif CONFIG_MPU == 1 || CONFIG_MMU == 0
#include "nommu.h"
#endif

/**
 * imal_init - 初始化 IMAL 层
 * 
 * 根据编译配置初始化对应的隔离机制
 */
void imal_init(void)
{
    console_puts("[IMAL] Initializing Isolation Mechanism Abstraction Layer\n");
    console_puts("[IMAL] Variant: ");
    console_puts(imal_get_variant_name());
    console_puts("\n");
    
#if CONFIG_MMU == 1
    /* MMU 变体：初始化页表管理器 */
    console_puts("[IMAL] Initializing page table manager...\n");
    console_puts("[IMAL] MMU-based isolation enabled\n");
    
#elif CONFIG_MPU == 1
    /* MPU 变体：初始化 MPU 保护 */
    console_puts("[IMAL] Initializing MPU protection...\n");
    nommu_init();
    nommu_mpu_enable();
    console_puts("[IMAL] MPU-based isolation enabled\n");
    
#else
    /* Safe 变体：无硬件隔离 */
    console_puts("[IMAL] No hardware isolation (Safe variant)\n");
    console_puts("[IMAL] Protection relies on capability system\n");
    nommu_init();
    
#endif
    
    console_puts("[IMAL] Initialization complete\n");
}
