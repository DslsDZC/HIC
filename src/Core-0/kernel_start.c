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
    /* ==================== 第一步：测试串口并重新配置 ==================== */
    /* 从 bootloader 跳转到内核后，GDT、段寄存器等环境变化可能导致串口配置失效
       这里测试串口是否可用，如果输出异常则重新配置 */

    /* 调试：直接使用内联汇编输出 'S'（测试 bootloader 的串口是否还有效） */
    __asm__ volatile("outb %%al, %%dx" : : "a"('S'), "d"(0x3F8));

    /* 简单重新配置串口：禁用中断和FIFO */
    __asm__ volatile(
        "mov $0x3F9, %%dx\n"     /* IER */
        "mov $0x00, %%al\n"      /* 禁用中断 */
        "outb %%al, %%dx\n"
        "mov $0x3FA, %%dx\n"     /* FCR */
        "mov $0x00, %%al\n"      /* 禁用FIFO */
        "outb %%al, %%dx\n"
        "mov $0x3FC, %%dx\n"     /* MCR */
        "mov $0x00, %%al\n"      /* 禁用RTS/DTR */
        "outb %%al, %%dx\n"
        :
        :
        : "dx", "al"
    );

    /* 调试：输出 boot_info 指针的低字节 */
    __asm__ volatile("outb %%al, %%dx" : : "a"((uint8_t)((uint64_t)boot_info)), "d"(0x3F8));

    /* 直接转发到实际的内核入口点 */
    // console_init会在kernel_boot_info_init中调用
    kernel_boot_info_init(boot_info);

    // 永远不应该到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}