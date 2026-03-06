/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
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
    // 先初始化串口硬件（重要！否则直接访问0x3F8端口会崩溃）
    console_init(CONSOLE_TYPE_SERIAL);
    
    // 输出 'S' 表示进入kernel_start
    console_putchar('S');
    
    // 初始化BSS段（清零未初始化的全局变量）
    extern char __bss_start[];
    extern char __bss_end[];
    char *bss_start = __bss_start;
    char *bss_end = __bss_end;
    
    // 清零BSS段
    while (bss_start < bss_end) {
        *bss_start = 0;
        bss_start++;
    }
    
    // 输出 'B' 表示BSS初始化完成
    console_putchar('B');
    
    // 输出 'C' 表示准备调用kernel_boot_info_init
    console_putchar('C');
    
    // 直接转发到实际的内核入口点
    kernel_boot_info_init(boot_info);
    
    // 输出 'D' 表示从kernel_boot_info_init返回（不应该到达这里）
    console_putchar('D');
    
    // 永远不应该到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}