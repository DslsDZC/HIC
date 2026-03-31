/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * ARM64 架构特定的 PMU 事件映射实现
 * 
 * ARMv8-A 性能监控事件编码
 * 使用 ARM Architectural Performance Events
 */

#include "../../pmu.h"

/**
 * 标准事件到 ARM64 架构配置映射
 * 
 * 使用 ARMv8-A Architectural Performance Events
 * 参考：ARM Architecture Reference Manual, Chapter D10
 */
u64 arch_pmu_event_to_config(pmu_event_type_t event)
{
    switch (event) {
        /* CPU 基本事件 - ARM Architectural Events */
        case PMU_EVENT_CPU_CYCLES:        return 0x0011;  /* CPU_CYCLES */
        case PMU_EVENT_INSTRUCTIONS:      return 0x0008;  /* INST_RETIRED */
        case PMU_EVENT_BRANCHES:          return 0x0012;  /* BR_RETIRED */
        case PMU_EVENT_BRANCH_MISSES:     return 0x0010;  /* BR_MIS_PRED */
        
        /* 缓存事件 */
        case PMU_EVENT_CACHE_REFERENCES:  return 0x0004;  /* L1D_CACHE */
        case PMU_EVENT_CACHE_MISSES:      return 0x0003;  /* L1D_CACHE_REFILL */
        case PMU_EVENT_CACHE_L1D_MISS:    return 0x0003;  /* L1D_CACHE_REFILL */
        case PMU_EVENT_CACHE_L1I_MISS:    return 0x0001;  /* L1I_CACHE_REFILL */
        case PMU_EVENT_CACHE_LL_READ:     return 0x0016;  /* LL_CACHE */
        case PMU_EVENT_CACHE_LL_MISS:     return 0x0017;  /* LL_CACHE_MISS */
        
        /* TLB 事件 */
        case PMU_EVENT_TLB_DTLB_MISS:     return 0x0005;  /* L1D_TLB_REFILL */
        case PMU_EVENT_TLB_ITLB_MISS:     return 0x0002;  /* L1I_TLB_REFILL */
        
        /* 流水线事件 */
        case PMU_EVENT_STALLED_CYCLES_FRONTEND: return 0x001F;  /* STALL_FRONTEND */
        case PMU_EVENT_STALLED_CYCLES_BACKEND:  return 0x0020;  /* STALL_BACKEND */
        
        default:                          return event;
    }
}

/**
 * ARM64 架构配置到标准事件映射
 */
pmu_event_type_t arch_pmu_config_to_event(u64 arch_config)
{
    switch (arch_config) {
        case 0x0011: return PMU_EVENT_CPU_CYCLES;
        case 0x0008: return PMU_EVENT_INSTRUCTIONS;
        case 0x0012: return PMU_EVENT_BRANCHES;
        case 0x0010: return PMU_EVENT_BRANCH_MISSES;
        case 0x0004: return PMU_EVENT_CACHE_REFERENCES;
        case 0x0003: return PMU_EVENT_CACHE_MISSES;
        case 0x0001: return PMU_EVENT_CACHE_L1I_MISS;
        case 0x0005: return PMU_EVENT_TLB_DTLB_MISS;
        case 0x0002: return PMU_EVENT_TLB_ITLB_MISS;
        case 0x001F: return PMU_EVENT_STALLED_CYCLES_FRONTEND;
        case 0x0020: return PMU_EVENT_STALLED_CYCLES_BACKEND;
        default:     return PMU_EVENT_RAW_START;
    }
}
