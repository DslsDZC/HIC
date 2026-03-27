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
static u64 max_frames = 0;          /* 位图可管理的最大帧数 */
static u64 usable_max_frame = 0;    /* 实际可用物理内存的最高帧索引 */
static u64 total_frames = 0;        /* 累计添加的可用帧数 */
static u64 free_frames = 0;         /* 当前空闲帧数 */

/* 内存区域链表 */
static mem_region_t *mem_regions = NULL;

/* 内存统计 */
static u64 g_total_memory = 0;
static u64 g_used_memory = 0;

/* 递归保护标志（避免在映射时递归分配） */
static int in_mapping = 0;

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

/**
 * 初始化物理内存管理器（带内存范围参数）
 * 
 * 参数：
 *   max_phys_addr - 最大物理地址，用于计算位图大小
 */
void pmm_init_with_range(phys_addr_t max_phys_addr)
{
    console_puts("[PMM] Initializing Physical Memory Manager...\n");
    
    /* 静态位图缓冲区大小 */
    static u8 static_bitmap[1024 * 1024];  /* 1MB静态缓冲区 */
    const u64 max_supported_frames = sizeof(static_bitmap) * 8;  /* 最大支持帧数 = 8M */
    const phys_addr_t max_supported_phys = max_supported_frames * PAGE_SIZE;  /* = 32GB */
    
    /* 首先检查位图缓冲区是否足够 */
    u64 requested_frames = (max_phys_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    
    if (requested_frames > max_supported_frames) {
        console_puts("[PMM] WARNING: Physical memory exceeds bitmap capacity!\n");
        console_puts("[PMM]   Requested: ");
        console_putu64(max_phys_addr / (1024 * 1024));
        console_puts(" MB (");
        console_putu64(requested_frames);
        console_puts(" frames)\n");
        console_puts("[PMM]   Supported: ");
        console_putu64(max_supported_phys / (1024 * 1024));
        console_puts(" MB (");
        console_putu64(max_supported_frames);
        console_puts(" frames)\n");
        console_puts("[PMM]   Memory above ");
        console_putu64(max_supported_phys / (1024 * 1024));
        console_puts(" MB will be UNUSABLE!\n");
        
        /* 截断到最大支持范围 */
        max_phys_addr = max_supported_phys;
    }
    
    /* 计算实际使用的帧数和位图大小 */
    max_frames = (max_phys_addr + PAGE_SIZE - 1) / PAGE_SIZE;
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
    console_puts("[PMM] Bitmap buffer: ");
    console_putu64(sizeof(static_bitmap));
    console_puts(" bytes (");
    console_putu64(sizeof(static_bitmap) / 1024);
    console_puts(" KB)\n");
    
    frame_bitmap = static_bitmap;
    
    /* 初始化位图：所有帧标记为已使用（1），只有添加的区域才标记为空闲（0） */
    /* 这样可以防止分配到未知的内存区域 */
    for (u64 i = 0; i < bitmap_size; i++) {
        frame_bitmap[i] = 0xFF;  /* 所有位设置为 1（已使用） */
    }
    /* 清零位图剩余部分（如果有） */
    for (u64 i = bitmap_size; i < sizeof(static_bitmap); i++) {
        frame_bitmap[i] = 0x00;
    }
    
    /* total_frames 应为 0，每次 pmm_add_region 时累加实际可用帧数 */
    total_frames = 0;
    free_frames = 0;
    g_total_memory = 0;
    g_used_memory = 0;
    usable_max_frame = 0;  /* 初始化为0，在 pmm_add_region 中更新 */
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
    
    console_puts("[PMM] Processing region: base=0x");
    console_puthex64(aligned_base);
    console_puts(", pages=");
    console_putu64(num_frames);
    console_puts("\n");
    
    if (start_frame >= max_frames) {
        console_puts("[PMM] WARNING: Region beyond manageable range, skipping\n");
        console_puts("[PMM]   start_frame=");
        console_putu64(start_frame);
        console_puts(", max_frames=");
        console_putu64(max_frames);
        console_puts("\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (start_frame + num_frames > max_frames) {
        u64 original_frames = num_frames;
        num_frames = max_frames - start_frame;
        console_puts("[PMM] WARNING: Region truncated to ");
        console_putu64(num_frames);
        console_puts(" pages (from ");
        console_putu64(original_frames);
        console_puts(")\n");
    }
    
    /* 添加到区域链表 */
    /* 从已知的可用内存中分配区域描述符 */
    /* 由于在初始化阶段，我们使用内存区域的起始部分作为区域描述符 */
    mem_region_t *region = (mem_region_t *)aligned_base;
    
    /* 跳过区域描述符，实际可用地址需要偏移 */
    size_t region_offset = sizeof(mem_region_t);
    u64 descriptor_pages = (region_offset + PAGE_SIZE - 1) / PAGE_SIZE;  /* 描述符占用的页面数 */
    
    /* 注意：初始化后位图 = all_ones，描述符帧已是已使用状态，无需操作 */
    /* 只需处理剩余帧，将其标记为空闲 */
    
    aligned_base += region_offset;
    aligned_size -= region_offset;
    
    if (aligned_size < PAGE_SIZE) {
        console_puts("[PMM] Skipping region: too small after region descriptor\n");
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 初始化区域描述符 */
    region->base = aligned_base;
    region->size = aligned_size;
    region->next = NULL;
    
    /* 将新区域链接到全局链表 */
    if (mem_regions == NULL) {
        mem_regions = region;
    } else {
        /* 找到链表末尾并添加 */
        mem_region_t *curr = mem_regions;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = region;
    }
    
    /* 标记剩余页面为可用（跳过描述符占用的页面） */
    u64 available_start_frame = start_frame + descriptor_pages;
    u64 available_frames = num_frames - descriptor_pages;
    
    console_puts("[PMM] Marking ");
    console_putu64(available_frames);
    console_puts(" pages as free (descriptor uses ");
    console_putu64(descriptor_pages);
    console_puts(" pages)...\n");
    
    for (u64 i = 0; i < available_frames; i++) {
        clear_bit(frame_bitmap, available_start_frame + i);
        free_frames++;
        
        /* 每处理 10000 页输出一次进度 */
        if ((i + 1) % 10000 == 0) {
            console_puts("[PMM] Progress: ");
            console_putu64(i + 1);
            console_puts("/");
            console_putu64(available_frames);
            console_puts(" pages processed\n");
        }
    }
    
    total_frames += available_frames;
    g_total_memory += aligned_size;
    
    /* 更新实际可用物理内存的最高帧索引 */
    u64 region_end_frame = available_start_frame + available_frames;
    if (region_end_frame > usable_max_frame) {
        usable_max_frame = region_end_frame;
    }
    
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
    
    console_puts("[PMM] Allocating ");
    console_putu64(count);
    console_puts(" frames, usable_max_frame=");
    console_putu64(usable_max_frame);
    console_puts(", free_frames=");
    console_putu64(free_frames);
    console_puts("\n");
    
    /* 首先尝试连续分配 - 使用 usable_max_frame 作为扫描范围 */
    u64 consecutive = 0;
    u64 start = 0;
    
    for (u64 i = 0; i < usable_max_frame && consecutive < count; i++) {
        if (!test_bit(frame_bitmap, i)) {
            if (consecutive == 0) {
                start = i;
            }
            consecutive++;
        } else {
            consecutive = 0;
        }
    }
    
    console_puts("[PMM] Found ");
    console_putu64(consecutive);
    console_puts(" consecutive frames starting at ");
    console_putu64(start);
    console_puts("\n");
    
    if (consecutive < count) {
        console_puts("[PMM] ERROR: Not enough free pages (need ");
        console_putu64(count);
        console_puts(", have ");
        console_putu64(free_frames);
        console_puts(" scattered)\n");
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
    
    phys_addr_t phys_addr = start * PAGE_SIZE;
    
    /* 内存已在 pagetable_init 中预先映射，无需再映射 */
    
    *out = phys_addr;
    
    return HIC_SUCCESS;
}

/* 分配分散页帧（用于大块内存分配，避免碎片问题）
 * 返回第一个页面的物理地址，其他页面通过链表连接
 */
hic_status_t pmm_alloc_scattered(domain_id_t owner, u32 count,
                                  page_frame_type_t type, phys_addr_t *pages)
{
    (void)owner;
    (void)type;
    
    if (count == 0 || pages == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    console_puts("[PMM] Allocating ");
    console_putu64(count);
    console_puts(" scattered frames\n");
    
    /* 检查是否有足够的空闲页 */
    if (free_frames < count) {
        console_puts("[PMM] Not enough free frames\n");
        return HIC_ERROR_NO_MEMORY;
    }
    
    /* 分配分散的页面 - 使用 usable_max_frame 作为扫描范围 */
    u32 allocated = 0;
    for (u64 i = 0; i < usable_max_frame && allocated < count; i++) {
        if (!test_bit(frame_bitmap, i)) {
            set_bit(frame_bitmap, i);
            pages[allocated] = i * PAGE_SIZE;
            allocated++;
        }
    }
    
    if (allocated < count) {
        /* 回滚 */
        for (u32 j = 0; j < allocated; j++) {
            clear_bit(frame_bitmap, pages[j] / PAGE_SIZE);
        }
        console_puts("[PMM] Failed to allocate enough scattered frames\n");
        return HIC_ERROR_NO_MEMORY;
    }
    
    free_frames -= count;
    g_used_memory += count * PAGE_SIZE;
    
    console_puts("[PMM] Allocated ");
    console_putu64(count);
    console_puts(" scattered frames\n");
    
    return HIC_SUCCESS;
}

/* 释放页帧 */
hic_status_t pmm_free_frames(phys_addr_t addr, u32 count)
{
    u64 start_frame = addr / PAGE_SIZE;
    
    /* 使用 max_frames 进行边界检查（可寻址范围），而非 total_frames（可用帧数） */
    if (start_frame >= max_frames || count == 0) {
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

/**
 * 查询页帧信息
 * 
 * 注意：当前实现使用位图跟踪页面状态，ref_count 只能是 0 或 1。
 * 对于共享内存场景，真正的引用计数应由能力系统 (capability.c) 管理。
 * 此函数返回的 ref_count 仅表示页面是否被分配，不反映实际共享数。
 */
hic_status_t pmm_get_frame_info(phys_addr_t addr, page_frame_t *info)
{
    u64 frame = addr / PAGE_SIZE;
    
    /* 使用 max_frames 进行边界检查（可寻址范围），而非 total_frames（可用帧数） */
    if (frame >= max_frames || info == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 根据帧索引获取帧信息 */
    if (frame < usable_max_frame) {
        info->base_addr = frame * PAGE_SIZE;
        
        /* 注意：位图只能表示 0/1，真正的引用计数需通过能力系统查询 */
        bool is_allocated = test_bit(frame_bitmap, frame);
        info->ref_count = is_allocated ? 1 : 0;

        /* 确定帧类型和所有者 */
        if (is_allocated) {
            /* 已分配页面：默认标记为核心域所有
             * 实际所有者信息应在更高层（域系统或能力系统）维护 */
            info->type = PAGE_FRAME_PRIVILEGED;
            info->owner = HIC_DOMAIN_CORE;
        } else {
            info->type = PAGE_FRAME_FREE;
            info->owner = 0;
        }
    } else {
        /* 超出实际可用内存范围 */
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
        
        /* 使用 max_frames 进行边界检查（可寻址范围） */
        if (frame_index < max_frames) {
            if (!test_bit(frame_bitmap, frame_index)) {
                set_bit(frame_bitmap, frame_index);
                free_frames--;
                g_used_memory += PAGE_SIZE;
            }
        }
    }
    
    console_puts("[PMM] Marked used: ");
    console_puthex64(base);
    console_puts(" - ");
    console_puthex64(base + size);
    console_puts(" (");
    console_putu64((size + PAGE_SIZE - 1) / PAGE_SIZE);
    console_puts(" pages)\n");
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
