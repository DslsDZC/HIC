/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC页表管理器实现（完整版）
 * 遵循文档第2.1节：物理资源管理与分配
 * 实现直接物理内存映射和MMU隔离
 */

#include "pagetable.h"
#include "pmm.h"
#include "hal.h"
#include "hal_cr3.h"
#include "domain_switch.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 页对齐辅助宏，避免符号转换警告 */
#define PAGE_ALIGN_MASK(addr, mask) ((addr) & ~(typeof(addr))(mask))
#define PAGE_ALIGN_DOWN(addr, size) ((addr) & ~(typeof(addr))((size) - 1))
#define PAGE_ALIGN_UP(addr, size) (((addr) + (size) - 1) & ~(typeof(addr))((size) - 1))

/* 页表分配器（从PMM分配） */
static page_table_t* allocate_page_table(void)
{
    phys_addr_t phys;
    hic_status_t status = pmm_alloc_frames(HIC_DOMAIN_CORE, 1, 
                                           PAGE_FRAME_PRIVILEGED, &phys);
    if (status != HIC_SUCCESS) {
        return NULL;
    }
    
    /* 获取虚拟地址（使用恒等映射） */
    page_table_t* virt = (page_table_t*)phys;
    
    /* 清零页表 */
    memzero(virt, sizeof(page_table_t));
    
    return virt;
}

/* 释放页表 */
static void free_page_table(page_table_t* table)
{
    if (table) {
        pmm_free_frames((phys_addr_t)table, 1);
    }
}

/* 递归释放页表树 */
static void free_page_table_tree(page_table_t* root, int level)
{
    if (!root) {
        return;
    }
    
    if (level > 0) {
        /* 递归释放子页表 */
        for (u32 i = 0; i < 512; i++) {
            u64 entry = root->entries[i];
            if (entry & PAGE_FLAG_PRESENT) {
                u64 phys = PAGE_ALIGN_MASK(entry, 0xFFF);
                page_table_t* child = (page_table_t*)phys;
                free_page_table_tree(child, level - 1);
            }
        }
    }
    
    /* 释放当前页表 */
    free_page_table(root);
}

/* 初始化页表管理器 */
void pagetable_init(void)
{
    console_puts("[PAGETABLE] Page table manager initialized\n");
}

/* 创建页表 */
page_table_t* pagetable_create(void)
{
    /* 创建PML4（4级页表根） */
    page_table_t* pml4 = allocate_page_table();
    if (!pml4) {
        return NULL;
    }
    
    console_puts("[PAGETABLE] Created new page table at 0x");
    console_puthex64((u64)pml4);
    console_puts("\n");
    
    return pml4;
}

/* 销毁页表（完整实现） */
void pagetable_destroy(page_table_t* root)
{
    if (!root) {
        return;
    }
    
    /* 完整实现：递归释放所有子页表 */
    free_page_table_tree(root, 3); /* PML4 -> PDPT -> PD -> PT */
}

/* 映射物理页（完整实现） */
hic_status_t pagetable_map(page_table_t* root, virt_addr_t virt, phys_addr_t phys, 
                           size_t size, page_perm_t perm, map_type_t type)
{
    if (!root || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 对齐到页边界 */
    virt_addr_t virt_aligned = PAGE_ALIGN_UP(virt, PAGE_SIZE_4K);
    phys_addr_t phys_aligned = PAGE_ALIGN_UP(phys, PAGE_SIZE_4K);
    size_t size_aligned = PAGE_ALIGN_UP(size, PAGE_SIZE_4K);
    
    /* 设置权限标志 */
    u64 flags = perm;
    if (type == MAP_TYPE_USER) {
        flags |= PAGE_FLAG_USER;
    } else {
        flags &= ~PAGE_FLAG_USER;
    }
    
    /* 逐页映射 */
    for (size_t offset = 0; offset < size_aligned; offset += PAGE_SIZE_4K) {
        virt_addr_t current_virt = virt_aligned + offset;
        phys_addr_t current_phys = phys_aligned + offset;
        
        /* 获取PML4索引 */
        u64 pml4_index = (current_virt >> 39) & 0x1FF;
        
        /* 获取或创建PDPT */
        if (!(root->entries[pml4_index] & PAGE_FLAG_PRESENT)) {
            page_table_t* pdpt = allocate_page_table();
            if (!pdpt) {
                return HIC_ERROR_NO_MEMORY;
            }
            root->entries[pml4_index] = (u64)pdpt | PAGE_FLAG_PRESENT | 
                                         PAGE_FLAG_WRITE | PAGE_FLAG_USER;
        }
        page_table_t* pdpt = (page_table_t*)PAGE_ALIGN_MASK(root->entries[pml4_index], 0xFFF);
        
        /* 获取PDPT索引 */
        u64 pdpt_index = (current_virt >> 30) & 0x1FF;
        
        /* 获取或创建PD */
        if (!(pdpt->entries[pdpt_index] & PAGE_FLAG_PRESENT)) {
            page_table_t* pd = allocate_page_table();
            if (!pd) {
                return HIC_ERROR_NO_MEMORY;
            }
            pdpt->entries[pdpt_index] = (u64)pd | PAGE_FLAG_PRESENT | 
                                        PAGE_FLAG_WRITE | PAGE_FLAG_USER;
        }
        page_table_t* pd = (page_table_t*)PAGE_ALIGN_MASK(pdpt->entries[pdpt_index], 0xFFF);
        
        /* 获取PD索引 */
        u64 pd_index = (current_virt >> 21) & 0x1FF;
        
        /* 获取或创建PT */
        if (!(pd->entries[pd_index] & PAGE_FLAG_PRESENT)) {
            page_table_t* pt = allocate_page_table();
            if (!pt) {
                return HIC_ERROR_NO_MEMORY;
            }
            pd->entries[pd_index] = (u64)pt | PAGE_FLAG_PRESENT | 
                                   PAGE_FLAG_WRITE | PAGE_FLAG_USER;
        }
        page_table_t* pt = (page_table_t*)PAGE_ALIGN_MASK(pd->entries[pd_index], 0xFFF);
        
        /* 获取PT索引 */
        u64 pt_index = (current_virt >> 12) & 0x1FF;
        
        /* 设置页表项 */
        pt->entries[pt_index] = current_phys | flags;
    }
    
    return HIC_SUCCESS;
}

/* 取消映射 */
hic_status_t pagetable_unmap(page_table_t* root, virt_addr_t virt, size_t size)
{
    if (!root || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 对齐到页边界 */
    virt_addr_t virt_aligned = PAGE_ALIGN_UP(virt, PAGE_SIZE_4K);
    size_t size_aligned = PAGE_ALIGN_UP(size, PAGE_SIZE_4K);
    
    /* 逐页取消映射 */
    for (size_t offset = 0; offset < size_aligned; offset += PAGE_SIZE_4K) {
        virt_addr_t current_virt = virt_aligned + offset;
        
        /* 获取各级索引 */
        u64 pml4_index = (current_virt >> 39) & 0x1FF;
        if (!(root->entries[pml4_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pdpt = (page_table_t*)PAGE_ALIGN_MASK(root->entries[pml4_index], 0xFFF);
        u64 pdpt_index = (current_virt >> 30) & 0x1FF;
        if (!(pdpt->entries[pdpt_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pd = (page_table_t*)PAGE_ALIGN_MASK(pdpt->entries[pdpt_index], 0xFFF);
        u64 pd_index = (current_virt >> 21) & 0x1FF;
        if (!(pd->entries[pd_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pt = (page_table_t*)PAGE_ALIGN_MASK(pd->entries[pd_index], 0xFFF);
        u64 pt_index = (current_virt >> 12) & 0x1FF;
        
        /* 清除页表项 */
        pt->entries[pt_index] = 0;
    }
    
    /* 刷新TLB */
    pagetable_flush_tlb_all();
    
    return HIC_SUCCESS;
}

/* 更改权限 */
hic_status_t pagetable_set_perm(page_table_t* root, virt_addr_t virt, 
                                size_t size, page_perm_t perm)
{
    if (!root || size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 对齐到页边界 */
    virt_addr_t virt_aligned = PAGE_ALIGN_UP(virt, PAGE_SIZE_4K);
    size_t size_aligned = PAGE_ALIGN_UP(size, PAGE_SIZE_4K);
    
    /* 逐页更改权限 */
    for (size_t offset = 0; offset < size_aligned; offset += PAGE_SIZE_4K) {
        virt_addr_t current_virt = virt_aligned + offset;
        
        /* 获取各级索引 */
        u64 pml4_index = (current_virt >> 39) & 0x1FF;
        if (!(root->entries[pml4_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pdpt = (page_table_t*)PAGE_ALIGN_MASK(root->entries[pml4_index], 0xFFF);
        u64 pdpt_index = (current_virt >> 30) & 0x1FF;
        if (!(pdpt->entries[pdpt_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pd = (page_table_t*)PAGE_ALIGN_MASK(pdpt->entries[pdpt_index], 0xFFF);
        u64 pd_index = (current_virt >> 21) & 0x1FF;
        if (!(pd->entries[pd_index] & PAGE_FLAG_PRESENT)) {
            continue;
        }

        page_table_t* pt = (page_table_t*)PAGE_ALIGN_MASK(pd->entries[pd_index], 0xFFF);
        u64 pt_index = (current_virt >> 12) & 0x1FF;
        
        /* 更新权限（保留PRESENT标志） */
        u64 entry = pt->entries[pt_index];
        entry &= PAGE_FLAG_PRESENT;
        entry |= perm;
        pt->entries[pt_index] = entry;
    }
    
    /* 刷新TLB */
    pagetable_flush_tlb_all();
    
    return HIC_SUCCESS;
}

/* 获取物理地址 */
phys_addr_t pagetable_get_phys(page_table_t* root, virt_addr_t virt)
{
    if (!root) {
        return 0;
    }
    
    /* 获取各级索引 */
    u64 pml4_index = (virt >> 39) & 0x1FF;
    if (!(root->entries[pml4_index] & PAGE_FLAG_PRESENT)) {
        return 0;
    }

    page_table_t* pdpt = (page_table_t*)PAGE_ALIGN_MASK(root->entries[pml4_index], 0xFFF);
            u64 pdpt_index = (virt >> 30) & 0x1FF;
            if (!(pdpt->entries[pdpt_index] & PAGE_FLAG_PRESENT)) {
                return 0;
            }
    
            page_table_t* pd = (page_table_t*)PAGE_ALIGN_MASK(pdpt->entries[pdpt_index], 0xFFF);
            u64 pd_index = (virt >> 21) & 0x1FF;    if (!(pd->entries[pd_index] & PAGE_FLAG_PRESENT)) {
        return 0;
    }

    page_table_t* pt = (page_table_t*)PAGE_ALIGN_MASK(pd->entries[pd_index], 0xFFF);
    u64 pt_index = (virt >> 12) & 0x1FF;
    if (!(pt->entries[pt_index] & PAGE_FLAG_PRESENT)) {
        return 0;
    }
    
    /* 返回物理地址 */
    return PAGE_ALIGN_MASK(pt->entries[pt_index], 0xFFF) + (virt & 0xFFF);
}

/* 切换页表 */
void pagetable_switch(page_table_t* root)
{
    if (!root) {
        return;
    }
    
    /* 加载CR3寄存器 */
    hal_set_cr3((u64)root);
}

/* 清空单个TLB条目 */
void pagetable_flush_tlb(virt_addr_t addr)
{
    hal_invalidate_page((void*)addr);
}

/* 清空所有TLB */
void pagetable_flush_tlb_all(void)
{
    /* 重新加载CR3会清空TLB */
    u64 cr3 = hal_get_cr3();
    hal_set_cr3(cr3);
}

/* 设置域页表（完整实现） */
hic_status_t pagetable_setup_domain(domain_id_t domain, page_table_t* root)
{
    if (domain >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 完整实现：保存域的页表指针 */
    /* 使用domain_switch_set_pagetable保存 */
    domain_switch_set_pagetable(domain, root);
    
    return HIC_SUCCESS;
}

/* 清理域页表（完整实现） */
void pagetable_cleanup_domain(domain_id_t domain)
{
    if (domain >= HIC_DOMAIN_MAX) {
        return;
    }
    
    /* 完整实现：释放域的页表 */
    page_table_t* pagetable = domain_switch_get_pagetable(domain);
    if (pagetable) {
        pagetable_destroy(pagetable);
        domain_switch_set_pagetable(domain, NULL);
    }
}
