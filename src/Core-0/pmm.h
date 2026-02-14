/**
 * HIK物理内存管理器(Physical Memory Manager)头文件
 * 遵循三层模型文档第2.1节：物理资源管理与分配
 */

#ifndef HIK_KERNEL_PMM_H
#define HIK_KERNEL_PMM_H

#include "types.h"
#include "domain.h"

/* 页大小 */
#define PAGE_SIZE        4096
#define PAGE_SHIFT       12
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* 页帧类型 */
typedef enum {
    PAGE_FRAME_FREE,        /* 空闲 */
    PAGE_FRAME_RESERVED,    /* 保留(硬件、BIOS等) */
    PAGE_FRAME_CORE,        /* Core-0使用 */
    PAGE_FRAME_PRIVILEGED,  /* Privileged-1服务使用 */
    PAGE_FRAME_APPLICATION, /* Application使用 */
    PAGE_FRAME_SHARED,      /* 共享内存 */
} page_frame_type_t;

/* 页帧描述符 */
typedef struct page_frame {
    u64              base_addr;    /* 物理基地址 */
    page_frame_type_t type;        /* 类型 */
    domain_id_t       owner;       /* 拥有者域ID */
    u32              ref_count;    /* 引用计数 */
    struct page_frame *next_free;  /* 下一个空闲帧 */
} page_frame_t;

/* 物理内存区域 */
typedef struct mem_region {
    phys_addr_t base;     /* 基地址 */
    size_t      size;     /* 大小 */
    u32         type;     /* 类型 */
    struct mem_region *next; /* 下一个区域 */
} mem_region_t;

/* 物理内存管理器接口 */
void pmm_init(void);

/* 添加内存区域 */
hik_status_t pmm_add_region(phys_addr_t base, size_t size);

/* 分配页帧 */
hik_status_t pmm_alloc_frames(domain_id_t owner, u32 count, 
                               page_frame_type_t type, phys_addr_t *out);

/* 释放页帧 */
hik_status_t pmm_free_frames(phys_addr_t addr, u32 count);

/* 查询页帧信息 */
hik_status_t pmm_get_frame_info(phys_addr_t addr, page_frame_t *info);

/* 获取统计信息 */
void pmm_get_stats(u64 *total_pages, u64 *free_pages, u64 *used_pages);

#endif /* HIK_KERNEL_PMM_H */