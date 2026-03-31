/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86_64 架构特定的 PMU 事件映射实现
 * 
 * Intel/AMD x86_64 性能监控事件编码
 * 使用 Intel Architectural Performance Monitoring 事件
 */

#include "../../pmu.h"

/**
 * 标准事件到 x86_64 架构配置映射
 * 
 * 使用 Intel Architectural Performance Monitoring 事件编码
 * 参考：Intel SDM Volume 3B, Chapter 19
 */
u64 arch_pmu_event_to_config(pmu_event_type_t event)
{
    switch (event) {
        /* CPU 基本事件 */
        case PMU_EVENT_CPU_CYCLES:        return 0x003C;  /* Unhalted Core Cycles */
        case PMU_EVENT_INSTRUCTIONS:      return 0x00C0;  /* Instructions Retired */
        case PMU_EVENT_BRANCHES:          return 0x00C4;  /* Branch Instructions Retired */
        case PMU_EVENT_BRANCH_MISSES:     return 0x00C5;  /* Branch Misses Retired */
        
        /* 缓存事件 */
        case PMU_EVENT_CACHE_REFERENCES:  return 0x004F;  /* L1D cache loads */
        case PMU_EVENT_CACHE_MISSES:      return 0x004E;  /* L1D cache load misses */
        case PMU_EVENT_CACHE_L1D_MISS:    return 0x0051;  /* L1D cache load miss */
        case PMU_EVENT_CACHE_L1I_MISS:    return 0x0080;  /* L1I cache miss */
        case PMU_EVENT_CACHE_LL_READ:     return 0x0027;  /* L2 cache reads */
        case PMU_EVENT_CACHE_LL_MISS:     return 0x0028;  /* L2 cache misses */
        
        /* TLB 事件 */
        case PMU_EVENT_TLB_DTLB_MISS:     return 0x0049;  /* DTLB load misses */
        case PMU_EVENT_TLB_ITLB_MISS:     return 0x0085;  /* ITLB misses */
        
        /* 流水线事件 */
        case PMU_EVENT_STALLED_CYCLES_FRONTEND: return 0x000E;  /* Frontend stalls */
        case PMU_EVENT_STALLED_CYCLES_BACKEND:  return 0x00B1;  /* Backend stalls */
        
        default:                          return event;
    }
}

/**
 * x86_64 架构配置到标准事件映射
 */
pmu_event_type_t arch_pmu_config_to_event(u64 arch_config)
{
    switch (arch_config) {
        case 0x003C: return PMU_EVENT_CPU_CYCLES;
        case 0x00C0: return PMU_EVENT_INSTRUCTIONS;
        case 0x00C4: return PMU_EVENT_BRANCHES;
        case 0x00C5: return PMU_EVENT_BRANCH_MISSES;
        case 0x004F: return PMU_EVENT_CACHE_REFERENCES;
        case 0x004E: return PMU_EVENT_CACHE_MISSES;
        case 0x0051: return PMU_EVENT_CACHE_L1D_MISS;
        case 0x0080: return PMU_EVENT_CACHE_L1I_MISS;
        case 0x0049: return PMU_EVENT_TLB_DTLB_MISS;
        case 0x0085: return PMU_EVENT_TLB_ITLB_MISS;
        case 0x000E: return PMU_EVENT_STALLED_CYCLES_FRONTEND;
        case 0x00B1: return PMU_EVENT_STALLED_CYCLES_BACKEND;
        default:     return PMU_EVENT_RAW_START;
    }
}
