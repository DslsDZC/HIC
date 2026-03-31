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
#include "hal.h"
#include "hardware_probe.h"

/* 声明 kernel_main 函数（在 main.c 中定义） */
extern void kernel_main(void *boot_info);

/**
 * 内核入口点（汇编调用）
 *
 * 这个函数由bootloader的jump_to_kernel函数调用
 * 接收boot_info作为参数（在RDI寄存器中）
 * 
 * 启动流程：
 * 1. kernel_start() - 本函数，由汇编入口调用
 * 2. kernel_main() - 主入口，完成所有初始化和启动流程
 *    - boot_info_validate() - 验证启动信息
 *    - boot_info_init_memory() - 初始化内存
 *    - boot_info_process() - 处理启动信息
 *    - 其他子系统初始化...
 */
void kernel_start(hic_boot_info_t* boot_info) {
    /* ==================== 第一步：初始化 HAL ==================== */
    /* HAL 初始化会设置架构相关的底层功能 */
    hal_init();
    
    /* ==================== 第一步：初始化硬件探测机制层 ==================== */
    /* 检测 FSGSBASE 支持等 CPU 特性，供 syscall 快速路径使用 */
    hardware_probe_mechanism_init();
    
    /* ==================== 第二步：转发到 kernel_main ==================== */
    /* 所有启动流程代码都在 kernel_main 中 */
    kernel_main(boot_info);

    /* 永远不应该到达这里 */
    while (1) {
        hal_halt();
    }
}
