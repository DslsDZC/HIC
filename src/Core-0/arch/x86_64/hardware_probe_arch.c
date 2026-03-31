/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86_64 架构特定的硬件探测实现
 * 
 * 包含：
 * - FSGSBASE 指令支持检测
 * - CPUID 扩展功能检测
 */

#include "hardware_probe.h"
#include "lib/console.h"

/**
 * 检测 CPU 是否支持 FSGSBASE 指令
 * 
 * FSGSBASE 在 Intel Ivy Bridge+ 和 AMD Bulldozer+ 支持。
 * CPUID Leaf 7, Subleaf 0, EBX bit 0。
 * 
 * 性能影响：
 * - 支持 FSGSBASE: rdfsbase/wrfsbase ~10 周期
 * - 不支持: rdmsr/wrmsr ~100-200 周期
 */
bool cpu_has_fsgsbase(void) {
    u32 eax, ebx, ecx, edx;
    
    /* 首先检查是否支持 Leaf 7 */
    u32 max_leaf;
    cpuid_execute(0, 0, &max_leaf, &ebx, &ecx, &edx);
    
    if (max_leaf < 7) {
        return false;
    }
    
    /* 检查 FSGSBASE (EBX bit 0) */
    cpuid_execute(7, 0, &eax, &ebx, &ecx, &edx);
    return (ebx & (1 << 0)) != 0;
}
