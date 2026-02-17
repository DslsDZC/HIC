/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK内核主入口
 * Bootloader跳转到的第一个C函数
 */

#include "boot_info.h"
#include "types.h"
#include "lib/console.h"

/**
 * 内核入口点（汇编调用）
 * 
 * 这个函数由bootloader的jump_to_kernel函数调用
 * 接收boot_info作为参数（在RDI寄存器中）
 */
void kernel_start(hik_boot_info_t* boot_info) {
    // 直接转发到实际的内核入口点
    // console_init会在kernel_entry中调用
    kernel_entry(boot_info);
}