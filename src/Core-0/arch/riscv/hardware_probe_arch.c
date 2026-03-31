/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * RISC-V 架构特定的硬件探测实现
 * 
 * RISC-V 不需要 FSGSBASE 指令：
 * - TLS 基址通过 tp 寄存器直接访问
 * - 无需特殊指令，直接使用标准寄存器访问
 */

#include "hardware_probe.h"

/**
 * 检测 CPU 是否支持 FSGSBASE 指令
 * 
 * RISC-V 不适用此功能，始终返回 false
 */
bool cpu_has_fsgsbase(void) {
    return false;
}
