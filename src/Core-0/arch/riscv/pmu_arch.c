/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * RISC-V 架构特定的 PMU 事件映射实现
 * 
 * RISC-V 性能计数器事件编码
 * 使用标准 RISC-V HPM (Hardware Performance Monitor) 事件
 */

#include "../../pmu.h"

/**
 * 标准事件到 RISC-V 架构配置映射
 * 
 * RISC-V HPM 事件编码（平台特定）
 * 参考：RISC-V Privileged Architecture Manual
 * 
 * 注意：RISC-V 的性能事件编码是平台特定的
 * 这里使用通用映射，实际使用时需要根据具体平台调整
 */
u64 arch_pmu_event_to_config(pmu_event_type_t event)
{
    switch (event) {
        /* CPU 基本事件 */
        case PMU_EVENT_CPU_CYCLES:        return 0x00;  /* mcycle */
        case PMU_EVENT_INSTRUCTIONS:      return 0x02;  /* instret */
        case PMU_EVENT_BRANCHES:          return 0x03;  /* 平台特定 */
        case PMU_EVENT_BRANCH_MISSES:     return 0x04;  /* 平台特定 */
        
        /* 缓存事件 - 平台特定 */
        case PMU_EVENT_CACHE_REFERENCES:  return 0x10;
        case PMU_EVENT_CACHE_MISSES:      return 0x11;
        case PMU_EVENT_CACHE_L1D_MISS:    return 0x12;
        case PMU_EVENT_CACHE_L1I_MISS:    return 0x13;
        case PMU_EVENT_CACHE_LL_READ:     return 0x14;
        case PMU_EVENT_CACHE_LL_MISS:     return 0x15;
        
        /* TLB 事件 - 平台特定 */
        case PMU_EVENT_TLB_DTLB_MISS:     return 0x20;
        case PMU_EVENT_TLB_ITLB_MISS:     return 0x21;
        
        /* 流水线事件 - 平台特定 */
        case PMU_EVENT_STALLED_CYCLES_FRONTEND: return 0x30;
        case PMU_EVENT_STALLED_CYCLES_BACKEND:  return 0x31;
        
        default:                          return event;
    }
}

/**
 * RISC-V 架构配置到标准事件映射
 */
pmu_event_type_t arch_pmu_config_to_event(u64 arch_config)
{
    switch (arch_config) {
        case 0x00: return PMU_EVENT_CPU_CYCLES;
        case 0x02: return PMU_EVENT_INSTRUCTIONS;
        case 0x03: return PMU_EVENT_BRANCHES;
        case 0x04: return PMU_EVENT_BRANCH_MISSES;
        case 0x10: return PMU_EVENT_CACHE_REFERENCES;
        case 0x11: return PMU_EVENT_CACHE_MISSES;
        case 0x12: return PMU_EVENT_CACHE_L1D_MISS;
        case 0x13: return PMU_EVENT_CACHE_L1I_MISS;
        case 0x20: return PMU_EVENT_TLB_DTLB_MISS;
        case 0x21: return PMU_EVENT_TLB_ITLB_MISS;
        case 0x30: return PMU_EVENT_STALLED_CYCLES_FRONTEND;
        case 0x31: return PMU_EVENT_STALLED_CYCLES_BACKEND;
        default:   return PMU_EVENT_RAW_START;
    }
}
