/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 架构特定的硬件探测实现
 * 
 * ARM64 不需要 FSGSBASE 指令：
 * - TLS 基址通过 TPIDRRO_EL0/TPIDRRO_EL1 寄存器直接访问
 * - MRS 指令可直接读取，无需特权级切换
 */

#include "hardware_probe.h"

/**
 * 检测 CPU 是否支持 FSGSBASE 指令
 * 
 * ARM64 不适用此功能，始终返回 false
 */
bool cpu_has_fsgsbase(void) {
    return false;
}
