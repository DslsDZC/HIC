/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 内存管理服务
 * 
 * 运行在 Privileged-1 层，提供：
 * - 伙伴系统内存分配
 * - 动态服务池管理
 * - 内存碎片整理
 * - 服务迁移支持
 * 
 * 设计原则（机制与策略分离）：
 * - Core-0 保留物理页位图分配原语（机制）
 * - 本服务实现伙伴系统和碎片整理（策略）
 */

#ifndef MEMORY_SERVICE_H
#define MEMORY_SERVICE_H

#include <stdint.h>
#include <stddef.h>

/* 服务状态 */
typedef enum {
    MEM_SVC_OK = 0,
    MEM_SVC_ERROR = 1,
    MEM_SVC_NO_MEMORY = 2,
    MEM_SVC_INVALID_PARAM = 3,
} memory_service_status_t;

/* 内存区域类型 */
typedef enum {
    MEM_ZONE_DYNAMIC_POOL = 0,
    MEM_ZONE_APPLICATION = 1,
} memory_zone_t;

/* 服务接口 */
memory_service_status_t memory_service_init(void);
memory_service_status_t memory_service_start(void);

/* 分配接口 */
memory_service_status_t memory_service_alloc(uint32_t size, 
                                              uint32_t owner_domain,
                                              memory_zone_t zone,
                                              uint64_t *out_phys_addr);

/* 释放接口 */
memory_service_status_t memory_service_free(uint64_t phys_addr, 
                                             uint32_t size,
                                             memory_zone_t zone);

/* 碎片整理 */
memory_service_status_t memory_service_defrag(void);
memory_service_status_t memory_service_migrate(uint32_t domain_id,
                                               uint64_t new_addr);

/* 统计信息 */
typedef struct {
    uint64_t total_pages;
    uint64_t free_pages;
    uint64_t used_pages;
    uint64_t largest_free_order;
} memory_stats_t;

memory_service_status_t memory_service_get_stats(memory_zone_t zone,
                                                  memory_stats_t *stats);

#endif /* MEMORY_SERVICE_H */
