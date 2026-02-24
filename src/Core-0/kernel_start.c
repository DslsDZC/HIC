/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核主入口
 * Bootloader跳转到的第一个C函数
 */

#include "boot_info.h"
#include "types.h"
#include "lib/console.h"

/* 声明kernel_boot_info_init函数（在boot_info.c中定义） */
extern void kernel_boot_info_init(hic_boot_info_t* boot_info);

/**
 * 内核入口点（汇编调用）
 * 
 * 这个函数由bootloader的jump_to_kernel函数调用
 * 接收boot_info作为参数（在RDI寄存器中）
 */
void kernel_start(hic_boot_info_t* boot_info) {
    // 直接转发到实际的内核入口点
    // console_init会在kernel_boot_info_init中调用
    kernel_boot_info_init(boot_info);
    
    // 永远不应该到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}