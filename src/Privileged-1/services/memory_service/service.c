/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 内存管理服务实现
 * 
 * 提供：
 * - 伙伴系统分配器
 * - 动态服务池管理
 * - 内存碎片整理
 * - 服务迁移
 */

#include "service.h"

/* ==================== 外部依赖（Core-0 提供） ==================== */

/* 串口输出 */
extern void serial_print(const char *msg);
extern void serial_putchar(char c);

/* 物理内存原语（Core-0 PMM） */
extern int pmm_alloc_page(uint64_t *out_phys);
extern void pmm_free_page(uint64_t phys);

/* 能力系统 */
extern int cap_update_memory(uint32_t domain, uint64_t old_addr, uint64_t new_addr);

/* 简化日志 */
#define LOG(msg) serial_print("[MEM_SVC] " msg "\n")
#define LOG_HEX(msg, val) do { \
    serial_print("[MEM_SVC] " msg ": 0x"); \
    /* 简化：直接输出 */ \
} while(0)

/* ==================== 伙伴系统常量 ==================== */

#define BUDDY_PAGE_SIZE     4096
#define BUDDY_MIN_ORDER     0
#define BUDDY_MAX_ORDER     20
#define BUDDY_ORDER_COUNT   (BUDDY_MAX_ORDER + 1)

/* 块标志 */
#define BLOCK_FREE      0
#define BLOCK_USED      1
#define BLOCK_MOVABLE   2

/* ==================== 伙伴系统数据结构 ==================== */

typedef struct buddy_block {
    uint64_t            phys_addr;
    uint32_t            order;
    uint32_t            flags;
    uint32_t            owner;
    struct buddy_block  *prev;
    struct buddy_block  *next;
} buddy_block_t;

typedef struct buddy_zone {
    uint64_t            base;
    uint64_t            size;
    uint32_t            max_order;
    
    buddy_block_t       *free_lists[BUDDY_ORDER_COUNT];
    uint32_t            free_counts[BUDDY_ORDER_COUNT];
    
    uint64_t            total_pages;
    uint64_t            free_pages;
    uint64_t            used_pages;
    
    buddy_block_t       *blocks;
    uint64_t            block_count;
} buddy_zone_t;

/* ==================== 全局状态 ==================== */

static buddy_zone_t g_dynamic_pool = {0};
static int g_initialized = 0;

/* 内存布局（从 Core-0 获取或配置） */
static uint64_t g_pool_base = 0;
static uint64_t g_pool_size = 0;

/* ==================== 辅助函数 ==================== */

static inline uint64_t buddy_block_size(uint32_t order) {
    return (uint64_t)BUDDY_PAGE_SIZE << order;
}

static inline uint64_t buddy_address(uint64_t addr, uint32_t order) {
    return addr ^ buddy_block_size(order);
}

static inline int in_zone(buddy_zone_t *zone, uint64_t addr) {
    return addr >= zone->base && addr < zone->base + zone->size;
}

/* ==================== 链表操作 ==================== */

static void list_add_head(buddy_zone_t *zone, uint32_t order, buddy_block_t *block) {
    block->prev = 0;
    block->next = zone->free_lists[order];
    if (zone->free_lists[order]) {
        zone->free_lists[order]->prev = block;
    }
    zone->free_lists[order] = block;
    zone->free_counts[order]++;
}

static void list_remove(buddy_zone_t *zone, uint32_t order, buddy_block_t *block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        zone->free_lists[order] = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    }
    block->prev = 0;
    block->next = 0;
    zone->free_counts[order]--;
}

/* ==================== 伙伴系统核心 ==================== */

static buddy_block_t* get_block_desc(buddy_zone_t *zone, uint64_t addr) {
    if (!in_zone(zone, addr)) return 0;
    uint64_t index = (addr - zone->base) / BUDDY_PAGE_SIZE;
    if (index >= zone->block_count) return 0;
    return &zone->blocks[index];
}

static void try_merge(buddy_zone_t *zone, buddy_block_t *block, uint32_t order) {
    if (order >= BUDDY_MAX_ORDER) return;
    
    uint64_t buddy_addr = buddy_address(block->phys_addr, order);
    buddy_block_t *buddy = get_block_desc(zone, buddy_addr);
    
    if (!buddy || buddy->flags != BLOCK_FREE || buddy->order != order) {
        list_add_head(zone, order, block);
        return;
    }
    
    list_remove(zone, order, buddy);
    
    uint64_t merged_addr = (block->phys_addr < buddy_addr) ? 
                           block->phys_addr : buddy_addr;
    buddy_block_t *merged = get_block_desc(zone, merged_addr);
    if (merged) {
        merged->order = order + 1;
        merged->flags = BLOCK_FREE;
        try_merge(zone, merged, order + 1);
    }
}

static void split_block(buddy_zone_t *zone, uint32_t order) {
    if (order <= BUDDY_MIN_ORDER || !zone->free_lists[order]) return;
    
    buddy_block_t *block = zone->free_lists[order];
    list_remove(zone, order, block);
    
    uint32_t new_order = order - 1;
    uint64_t buddy_addr = buddy_address(block->phys_addr, new_order);
    
    buddy_block_t *block1 = get_block_desc(zone, block->phys_addr);
    buddy_block_t *block2 = get_block_desc(zone, buddy_addr);
    
    if (block1) {
        block1->order = new_order;
        block1->flags = BLOCK_FREE;
        list_add_head(zone, new_order, block1);
    }
    
    if (block2) {
        block2->order = new_order;
        block2->flags = BLOCK_FREE;
        list_add_head(zone, new_order, block2);
    }
}

/* ==================== 服务入口点 ==================== */

__attribute__((section(".static_svc.memory_service.text"), used, noinline))
int _memory_service_entry(void)
{
    serial_print("[MEM_SVC] Entry point reached\n");
    
    if (memory_service_init() != MEM_SVC_OK) {
        serial_print("[MEM_SVC] Init failed\n");
        return -1;
    }
    
    memory_service_start();
    
    /* 服务初始化完成，返回让调度器继续执行其他线程 */
    /* 后续通过中断或轮询处理请求 */
    serial_print("[MEM_SVC] Service ready, yielding to scheduler\n");
    
    return 0;
}

/* ==================== 服务实现 ==================== */

memory_service_status_t memory_service_init(void) {
    LOG("Initializing memory service...");
    
    if (g_initialized) {
        LOG("Already initialized");
        return MEM_SVC_OK;
    }
    
    /* 
     * 动态服务池参数（应从配置获取）
     * 这里使用固定值，实际应从 platform.yaml 读取
     */
    g_pool_base = 0x10000000;  /* 256MB 起始 */
    g_pool_size = 0x10000000;  /* 256MB 大小 */
    
    buddy_zone_t *zone = &g_dynamic_pool;
    
    zone->base = g_pool_base;
    zone->size = g_pool_size;
    zone->max_order = BUDDY_MAX_ORDER;
    
    /* 计算块描述符数量 */
    uint64_t page_count = zone->size / BUDDY_PAGE_SIZE;
    uint64_t metadata_size = page_count * sizeof(buddy_block_t);
    uint64_t metadata_pages = (metadata_size + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
    
    /* 使用区域起始存储块描述符 */
    zone->blocks = (buddy_block_t *)zone->base;
    zone->block_count = page_count;
    
    /* 初始化块描述符 */
    for (uint64_t i = 0; i < page_count; i++) {
        zone->blocks[i].phys_addr = zone->base + i * BUDDY_PAGE_SIZE;
        zone->blocks[i].order = 0;
        zone->blocks[i].flags = (i < metadata_pages) ? BLOCK_USED : BLOCK_FREE;
        zone->blocks[i].owner = 0;
        zone->blocks[i].prev = 0;
        zone->blocks[i].next = 0;
    }
    
    /* 调整可用区域 */
    zone->base += metadata_pages * BUDDY_PAGE_SIZE;
    zone->size -= metadata_pages * BUDDY_PAGE_SIZE;
    zone->total_pages = zone->size / BUDDY_PAGE_SIZE;
    zone->free_pages = zone->total_pages;
    zone->used_pages = 0;
    
    /* 添加所有空闲页到链表 */
    for (uint64_t i = metadata_pages; i < page_count; i++) {
        buddy_block_t *block = &g_dynamic_pool.blocks[i];
        if (block->flags == BLOCK_FREE) {
            list_add_head(zone, 0, block);
        }
    }
    
    g_initialized = 1;
    
    LOG("Memory service initialized");
    return MEM_SVC_OK;
}

memory_service_status_t memory_service_start(void) {
    LOG("Memory service started");
    return MEM_SVC_OK;
}

/* 计算合适的阶数 */
static uint32_t calc_order(uint32_t size) {
    if (size == 0) return 0;
    uint32_t pages = (size + BUDDY_PAGE_SIZE - 1) / BUDDY_PAGE_SIZE;
    uint32_t order = 0;
    uint32_t block = 1;
    while (block < pages && order < BUDDY_MAX_ORDER) {
        order++;
        block <<= 1;
    }
    return order;
}

memory_service_status_t memory_service_alloc(uint32_t size,
                                              uint32_t owner_domain,
                                              memory_zone_t zone_type,
                                              uint64_t *out_phys_addr) {
    if (!out_phys_addr || size == 0) {
        return MEM_SVC_INVALID_PARAM;
    }
    
    if (zone_type != MEM_ZONE_DYNAMIC_POOL) {
        return MEM_SVC_INVALID_PARAM;
    }
    
    buddy_zone_t *zone = &g_dynamic_pool;
    uint32_t order = calc_order(size);
    
    /* 查找可用块 */
    uint32_t current_order = order;
    while (current_order <= zone->max_order && !zone->free_lists[current_order]) {
        current_order++;
    }
    
    if (current_order > zone->max_order) {
        LOG("No free block");
        return MEM_SVC_NO_MEMORY;
    }
    
    /* 分裂大块 */
    while (current_order > order) {
        split_block(zone, current_order);
        current_order--;
    }
    
    /* 取出块 */
    buddy_block_t *block = zone->free_lists[order];
    if (!block) {
        return MEM_SVC_NO_MEMORY;
    }
    
    list_remove(zone, order, block);
    
    block->flags = BLOCK_USED;
    block->owner = owner_domain;
    
    uint64_t pages = 1ULL << order;
    zone->free_pages -= pages;
    zone->used_pages += pages;
    
    *out_phys_addr = block->phys_addr;
    
    return MEM_SVC_OK;
}

memory_service_status_t memory_service_free(uint64_t phys_addr,
                                             uint32_t size,
                                             memory_zone_t zone_type) {
    if (zone_type != MEM_ZONE_DYNAMIC_POOL) {
        return MEM_SVC_INVALID_PARAM;
    }
    
    buddy_zone_t *zone = &g_dynamic_pool;
    uint32_t order = calc_order(size);
    
    buddy_block_t *block = get_block_desc(zone, phys_addr);
    if (!block || block->flags != BLOCK_USED) {
        return MEM_SVC_INVALID_PARAM;
    }
    
    block->flags = BLOCK_FREE;
    block->owner = 0;
    block->order = order;
    
    uint64_t pages = 1ULL << order;
    zone->free_pages += pages;
    zone->used_pages -= pages;
    
    try_merge(zone, block, order);
    
    return MEM_SVC_OK;
}

memory_service_status_t memory_service_defrag(void) {
    LOG("Defragmenting...");
    
    buddy_zone_t *zone = &g_dynamic_pool;
    
    for (uint32_t order = 0; order < BUDDY_MAX_ORDER; order++) {
        buddy_block_t *block = zone->free_lists[order];
        while (block) {
            buddy_block_t *next = block->next;
            try_merge(zone, block, order);
            block = next;
        }
    }
    
    LOG("Defragmentation complete");
    return MEM_SVC_OK;
}

memory_service_status_t memory_service_migrate(uint32_t domain_id,
                                               uint64_t new_addr) {
    /* 服务迁移 - 简化实现 */
    LOG("Migration not fully implemented");
    return MEM_SVC_ERROR;
}

memory_service_status_t memory_service_get_stats(memory_zone_t zone_type,
                                                  memory_stats_t *stats) {
    if (!stats) return MEM_SVC_INVALID_PARAM;
    if (zone_type != MEM_ZONE_DYNAMIC_POOL) return MEM_SVC_INVALID_PARAM;
    
    buddy_zone_t *zone = &g_dynamic_pool;
    stats->total_pages = zone->total_pages;
    stats->free_pages = zone->free_pages;
    stats->used_pages = zone->used_pages;
    
    /* 查找最大空闲块 */
    stats->largest_free_order = 0;
    for (int i = BUDDY_MAX_ORDER; i >= 0; i--) {
        if (zone->free_lists[i]) {
            stats->largest_free_order = i;
            break;
        }
    }
    
    return MEM_SVC_OK;
}

/* 服务接口（供模块加载器调用） */
int memory_service_module_init(void) {
    return (memory_service_init() == MEM_SVC_OK) ? 0 : -1;
}

int memory_service_module_start(void) {
    return (memory_service_start() == MEM_SVC_OK) ? 0 : -1;
}
