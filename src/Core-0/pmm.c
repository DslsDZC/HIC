/**
 * HIK物理内存管理器实现
 * 遵循三层模型文档第2.1节：物理资源管理与分配
 */

#include "pmm.h"
#include "domain.h"
#include "lib/mem.h"
#include "lib/console.h"

#define MAX_FRAMES (16 * 1024 * 1024 / PAGE_SIZE)  /* 16MB最大支持 */
#define FRAME_BITMAP_SIZE ((MAX_FRAMES + 7) / 8)

/* 页帧位图 */
static u8 frame_bitmap[FRAME_BITMAP_SIZE];
static u64 total_frames = 0;
static u64 free_frames = 0;

/* 内存区域链表 */
static mem_region_t *mem_regions = NULL;

/* 内存统计 */
static u64 total_memory = 0;
static u64 used_memory = 0;

/* 位图操作 */
static inline void set_bit(u8 *bitmap, u64 index)
{
    bitmap[index / 8] |= (1 << (index % 8));
}

static inline void clear_bit(u8 *bitmap, u64 index)
{
    bitmap[index / 8] &= ~(1 << (index % 8));
}

static inline int test_bit(u8 *bitmap, u64 index)
{
    return bitmap[index / 8] & (1 << (index % 8));
}

/* 初始化物理内存管理器 */
void pmm_init(void)
{
    memzero(frame_bitmap, sizeof(frame_bitmap));
    total_frames = 0;
    free_frames = 0;
    total_memory = 0;
    used_memory = 0;
    mem_regions = NULL;
    
    console_puts("[PMM] Physical Memory Manager initialized\n");
}

/* 添加内存区域 */
hik_status_t pmm_add_region(phys_addr_t base, size_t size)
{
    /* 对齐到页边界 */
    phys_addr_t aligned_base = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t aligned_size = size - (aligned_base - base);
    aligned_size &= ~(PAGE_SIZE - 1);
    
    if (aligned_size < PAGE_SIZE) {
        return HIK_ERROR_INVALID_PARAM;
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
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 初始化区域描述符 */
    region->base = aligned_base;
    region->size = aligned_size;
    region->type = PAGE_FRAME_FREE;
    region->next = mem_regions;
    mem_regions = region;
    
    /* 计算页数 */
    u64 num_frames = aligned_size / PAGE_SIZE;
    u64 start_frame = aligned_base / PAGE_SIZE;
    
    if (start_frame + num_frames > MAX_FRAMES) {
        console_puts("[PMM] WARNING: Region too large, truncating\n");
        num_frames = MAX_FRAMES - start_frame;
    }
    
    /* 标记页为可用 */
    for (u64 i = 0; i < num_frames; i++) {
        clear_bit(frame_bitmap, start_frame + i);
        free_frames++;
    }
    
    total_frames += num_frames;
    total_memory += aligned_size;
    
    console_puts("[PMM] Added region: 0x");
    console_puthex64(aligned_base);
    console_puts(" - ");
    console_putu64(num_frames);
    console_puts(" pages\n");
    
    return HIK_SUCCESS;
}

/* 分配页帧 */
hik_status_t pmm_alloc_frames(domain_id_t owner, u32 count, 
                               page_frame_type_t type, phys_addr_t *out)
{
    if (count == 0 || out == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 检查域配额 */
    if (owner != HIK_DOMAIN_CORE) {
        /* 完整实现：检查域内存配额 */
        domain_t* domain = get_domain(owner);
        if (domain == NULL) {
            return HIK_ERROR_INVALID_PARAM;
        }
        
        /* 检查是否超过配额 */
        u64 requested_size = count * PAGE_SIZE;
        if (domain->memory_used + requested_size > domain->memory_quota) {
            console_puts("[PMM] ERROR: Domain memory quota exceeded\n");
            return HIK_ERROR_QUOTA_EXCEEDED;
        }
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
        return HIK_ERROR_NO_MEMORY;
    }
    
    /* 标记为已使用 */
    for (u32 i = 0; i < count; i++) {
        set_bit(frame_bitmap, start + i);
    }
    
    free_frames -= count;
    used_memory += count * PAGE_SIZE;
    
    *out = start * PAGE_SIZE;
    
    return HIK_SUCCESS;
}

/* 释放页帧 */
hik_status_t pmm_free_frames(phys_addr_t addr, u32 count)
{
    u64 start_frame = addr / PAGE_SIZE;
    
    if (start_frame >= total_frames || count == 0) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 检查页帧是否已分配 */
    for (u32 i = 0; i < count; i++) {
        if (!test_bit(frame_bitmap, start_frame + i)) {
            /* 页帧未分配 */
            return HIK_ERROR_INVALID_PARAM;
        }
    }
    
    /* 标记为空闲 */
    for (u32 i = 0; i < count; i++) {
        clear_bit(frame_bitmap, start_frame + i);
    }
    
    free_frames += count;
    used_memory -= count * PAGE_SIZE;
    
    return HIK_SUCCESS;
}

/* 查询页帧信息 */
hik_status_t pmm_get_frame_info(phys_addr_t addr, page_frame_t *info)
{
    u64 frame = addr / PAGE_SIZE;
    
    if (frame >= total_frames || info == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    info->base_addr = frame * PAGE_SIZE;
    info->ref_count = test_bit(frame_bitmap, frame) ? 1 : 0;
    
    /* 完整实现：获取类型和所有者信息 */
    /* 从页帧元数据中获取详细信息 */
    page_frame_metadata_t* metadata = get_frame_metadata(frame);
    
    if (metadata != NULL && info->ref_count > 0) {
        info->type = metadata->type;
        info->owner = metadata->owner;
    } else {
        info->type = PAGE_FRAME_FREE;
        info->owner = HIK_DOMAIN_CORE;
    }
    
    info->next_free = NULL;  /* 用于空闲链表 */
    
    return HIK_SUCCESS;
}

/* 获取统计信息 */
void pmm_get_stats(u64 *total_pages, u64 *free_pages, u64 *used_pages)
{
    if (total_pages) *total_pages = total_frames;
    if (free_pages) *free_pages = free_frames;
    if (used_pages) *used_pages = total_frames - free_frames;
}
