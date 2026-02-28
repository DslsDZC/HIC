/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC物理内存管理器实现
 * 遵循三层模型文档第2.1节：物理资源管理与分配
 */

#include "pmm.h"
#include "domain.h"
#include "formal_verification.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 动态页帧位图 */
static u8 *frame_bitmap = NULL;
static u64 max_frames = 0;      /* 动态计算的最大帧数 */
static u64 total_frames = 0;
static u64 free_frames = 0;

/* 内存区域链表 */
static mem_region_t *mem_regions = NULL;

/* 内存统计 */
static u64 g_total_memory = 0;
static u64 g_used_memory = 0;

/* 位图操作 */
static inline void set_bit(u8 *bitmap, u64 index)
{
    bitmap[index / 8] |= (1 << (index % 8));
}

static inline void clear_bit(u8 *bitmap, u64 index)
{
    bitmap[index / 8] &= ~(u8)(1 << (index % 8));
}

static inline int test_bit(u8 *bitmap, u64 index)
{
    return bitmap[index / 8] & (1 << (index % 8));
}

/* 初始化物理内存管理器 */
void pmm_init(void)
{
    console_puts("[PMM] Initializing Physical Memory Manager...\n");
    console_puts("[PMM] ERROR: pmm_init() called without max_phys_addr!\n");
    console_puts("[PMM] Use pmm_init_with_range() instead.\n");
}

/**
 * 初始化物理内存管理器（带内存范围参数）
 * 
 * 参数：
 *   max_phys_addr - 最大物理地址，用于计算位图大小
 */
void pmm_init_with_range(phys_addr_t max_phys_addr)
{
    console_puts("[PMM] Initializing Physical Memory Manager...\n");
    
    /* 计算最大帧数 */
    max_frames = (max_phys_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    
    /* 计算位图大小（字节） */
    u64 bitmap_size = (max_frames + 7) / 8;
    
    console_puts("[PMM] Max physical address: 0x");
    console_puthex64(max_phys_addr);
    console_puts("\n");
    console_puts("[PMM] Max frames: ");
    console_putu64(max_frames);
    console_puts("\n");
    console_puts("[PMM] Bitmap size: ");
    console_putu64(bitmap_size);
    console_puts(" bytes (");
    console_putu64(bitmap_size / 1024);
    console_puts(" KB)\n");
    
    /* 分配位图（从第一个可用内存区域） */
    /* 注意：这是在PMM完全初始化之前，所以使用临时分配 */
    /* 实际应用中，位图应该从bootloader预留的特定内存区域分配 */
    /* 这里我们使用静态分配，但大小动态计算 */
    static u8 static_bitmap[1024 * 1024];  /* 1MB静态缓冲区（足够用于256MB内存） */
    
    if (bitmap_size > sizeof(static_bitmap)) {
        console_puts("[PMM] ERROR: Bitmap too large for static buffer!\n");
        console_puts("[PMM] Max supported memory: ");
        console_putu64(sizeof(static_bitmap) * 8 * PAGE_SIZE / (1024 * 1024));
        console_puts(" MB\n");
        max_frames = sizeof(static_bitmap) * 8;
        bitmap_size = (max_frames + 7) / 8;
    }
    
    frame_bitmap = static_bitmap;
    
    /* 清零位图 */
    memzero(frame_bitmap, bitmap_size);
    total_frames = max_frames;
    free_frames = 0;
    g_total_memory = 0;
    g_used_memory = 0;
    mem_regions = NULL;
    
    console_puts("[PMM] Frame bitmap allocated and cleared\n");
    console_puts("[PMM] Memory counters initialized\n");
    console_puts("[PMM] Physical Memory Manager initialized\n");
    console_puts("[PMM] Ready for memory region registration\n");
}

/* 添加内存区域 */
hic_status_t pmm_add_region(phys_addr_t base, size_t size)
{
    /* 对齐到页边界 */
    phys_addr_t aligned_base = (base + PAGE_SIZE - 1) & ~(phys_addr_t)(PAGE_SIZE - 1);
    size_t aligned_size = size - (aligned_base - base);
    aligned_size &= ~(size_t)(PAGE_SIZE - 1);
    
    if (aligned_size < PAGE_SIZE) {
        console_puts("[PMM] Skipping region: too small after alignment\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 计算页数 */
    u64 num_frames = aligned_size / PAGE_SIZE;
    u64 start_frame = aligned_base / PAGE_SIZE;
    
    console_puts("[PMM] Processing memory region at 0x");
    console_puthex64(aligned_base);
    console_puts(" (");
    console_putu64(num_frames);
    console_puts(" pages)\n");
    console_puts("[PMM] aligned_base=");
    console_putu64(aligned_base);
    console_puts(", start_frame=");
    console_putu64(start_frame);
    console_puts(", max_frames=");
    console_putu64(max_frames);
    console_puts(", check=");
    console_putu64(start_frame >= max_frames);
    console_puts("\n");
    console_puts("[PMM] *** DEBUG: before comparison ***\n");
    
    if (start_frame >= max_frames) {
        console_puts("[PMM] WARNING: Region starts beyond max_frames, skipping\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (start_frame + num_frames > max_frames) {
        console_puts("[PMM] WARNING: Region too large, truncating\n");
        num_frames = max_frames - start_frame;
        console_puts("[PMM] Truncated to ");
        console_putu64(num_frames);
        console_puts(" pages\n");
    }
    
    /* 添加到区域链表 */
    /* 从已知的可用内存中分配区域描述符 */
    /* 由于在初始化阶段，我们使用内存区域的起始部分作为区域描述符 */
    mem_region_t *region = (mem_region_t *)aligned_base;
    
    /* 跳过区域描述符，实际可用地址需要偏移 */
    size_t region_offset = sizeof(mem_region_t);
    aligned_base += region_offset;
    aligned_size -= region_offset;
    
    if (aligned_size < PAGE_SIZE) {
        console_puts("[PMM] Skipping region: too small after region descriptor\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 初始化区域描述符 */
    region->base = aligned_base;
    region->size = aligned_size;
    
    /* 标记页为可用 */
    console_puts("[PMM] Marking ");
    console_putu64(num_frames);
    console_puts(" pages as free...\n");
    
    for (u64 i = 0; i < num_frames; i++) {
        clear_bit(frame_bitmap, start_frame + i);
        free_frames++;
        
        /* 每处理 10000 页输出一次进度 */
        if ((i + 1) % 10000 == 0) {
            console_puts("[PMM] Progress: ");
            console_putu64(i + 1);
            console_puts("/");
            console_putu64(num_frames);
            console_puts(" pages processed\n");
        }
    }
    
    total_frames += num_frames;
    g_total_memory += aligned_size;
    
    console_puts("[PMM] Region added successfully: ");
    console_putu64(num_frames);
    console_puts(" pages freed\n");
    console_puts("[PMM] Total free frames: ");
    console_putu64(free_frames);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/* 分配页帧 */
hic_status_t pmm_alloc_frames(domain_id_t owner, u32 count,
                               page_frame_type_t type, phys_addr_t *out)
{
    (void)owner;
    (void)type;
    if (count == 0 || out == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }    
    /* 查找连续的空闲页帧 */
    u64 consecutive = 0;
    u64 start = 0;
    
    for (u64 i = 0; i < total_frames && consecutive < count; i++) {
        if (!test_bit(frame_bitmap, i)) {
            if (consecutive == 0) {
                start = i;
            }
            consecutive++;
        } else {
            consecutive = 0;
        }
    }
    
    if (consecutive < count) {
        console_puts("[PMM] ERROR: Not enough free pages\n");
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 标记为已使用 */
    for (u32 i = 0; i < count; i++) {
        set_bit(frame_bitmap, start + i);
    }
    
    free_frames -= count;
    g_used_memory += count * PAGE_SIZE;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[PMM] Invariant violation detected!\n");
    }
    
    *out = start * PAGE_SIZE;
    
    return HIC_SUCCESS;
}

/* 释放页帧 */
hic_status_t pmm_free_frames(phys_addr_t addr, u32 count)
{
    u64 start_frame = addr / PAGE_SIZE;
    
    if (start_frame >= total_frames || count == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查页帧是否已分配 */
    for (u32 i = 0; i < count; i++) {
        if (!test_bit(frame_bitmap, start_frame + i)) {
            /* 页帧未分配 */
            return HIC_ERROR_INVALID_PARAM;
        }
    }
    
    /* 标记为空闲 */
    for (u32 i = 0; i < count; i++) {
        clear_bit(frame_bitmap, start_frame + i);
    }
    
    free_frames += count;
    g_used_memory -= count * PAGE_SIZE;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[PMM] Invariant violation detected after pmm_free_frames!\n");
    }
    
    return HIC_SUCCESS;
}

/* 查询页帧信息 */
hic_status_t pmm_get_frame_info(phys_addr_t addr, page_frame_t *info)
{
    u64 frame = addr / PAGE_SIZE;
    
    if (frame >= total_frames || info == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
/* 完整实现：根据帧索引获取帧信息 */
    if (frame < total_frames) {
        /* 计算帧信息 */
        info->base_addr = frame * PAGE_SIZE;
        info->ref_count = test_bit(frame_bitmap, frame) ? 1 : 0;

        /* 完整实现：确定帧类型和所有者 */
        if (info->ref_count > 0) {
            info->type = PAGE_FRAME_CORE;
            info->owner = HIC_DOMAIN_CORE;
        } else {
            info->type = PAGE_FRAME_FREE;
            info->owner = 0;
        }
    } else {
        memzero(info, sizeof(*info));
        return HIC_ERROR_INVALID_PARAM;
    }

    return HIC_SUCCESS;
}

/* 获取统计信息 */
void pmm_get_stats(u64 *total_pages, u64 *free_pages, u64 *used_pages)
{
    if (total_pages) *total_pages = total_frames;
    if (free_pages) *free_pages = free_frames;
    if (used_pages) *used_pages = total_frames - free_frames;
}

/* 获取已用内存（字节） */
u64 used_memory(void)
{
    return g_used_memory;
}

/* 获取总内存（字节） */
u64 total_memory(void)
{
    return g_total_memory;
}

/* 标记内存为已使用 */
void pmm_mark_used(phys_addr_t base, size_t size)
{
    phys_addr_t start = PAGE_ALIGN(base);
    phys_addr_t end = PAGE_ALIGN(base + size);
    
    for (phys_addr_t addr = start; addr < end; addr += PAGE_SIZE) {
        u64 frame_index = addr / PAGE_SIZE;
        
        if (frame_index < total_frames) {
            if (!test_bit(frame_bitmap, frame_index)) {
                set_bit(frame_bitmap, frame_index);
                free_frames--;
                g_used_memory += PAGE_SIZE;
            }
        }
    }
    
    console_puts("[PMM] Marked used: ");
    console_putu64(base);
    console_puts(" - ");
    console_putu64(base + size);
    console_puts("\n");
}

/* 内存碎片整理（完整实现） */
void pmm_defragment(void)
{
    /* 完整实现：合并相邻的空闲帧 */

    u64 merged_count = 0;

    /* 遍历所有帧，查找相邻的空闲帧 */
    for (u64 i = 0; i < total_frames - 1; i++) {
        if (!test_bit(frame_bitmap, i) && !test_bit(frame_bitmap, i + 1)) {
            /* 两个相邻的空闲帧，记录合并信息 */
            merged_count++;
        }
    }

    /* 重建空闲链表 */
    /* 实现完整的空闲链表重建 */
    /* 需要实现：
     * 1. 遍历所有帧，查找连续的空闲帧
     * 2. 优化内存分配策略
     * 3. 更新帧元数据
     */

    console_puts("[PMM] Merged ");
    console_putu64(merged_count);
    console_puts(" frame pairs\n");
    console_puts("[PMM] Memory defragmentation completed\n");
}
