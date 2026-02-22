/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC页表管理器
 * 遵循文档第2.1节：物理资源管理与分配
 * 实现直接物理内存映射和MMU隔离
 */

#ifndef HIC_KERNEL_PAGETABLE_H
#define HIC_KERNEL_PAGETABLE_H

#include "types.h"
#include "domain.h"

/* 页表项标志（x86-64 PML4E/PDPE/PDE/PTE） */
#define PAGE_FLAG_PRESENT    (1ULL << 0)
#define PAGE_FLAG_WRITE      (1ULL << 1)
#define PAGE_FLAG_USER       (1ULL << 2)
#define PAGE_FLAG_PWT        (1ULL << 3)  /* Write-through */
#define PAGE_FLAG_PCD        (1ULL << 4)  /* Cache-disable */
#define PAGE_FLAG_ACCESSED   (1ULL << 5)
#define PAGE_FLAG_DIRTY      (1ULL << 6)
#define PAGE_FLAG_PS         (1ULL << 7)  /* Page size */
#define PAGE_FLAG_GLOBAL     (1ULL << 8)
#define PAGE_FLAG_NX         (1ULL << 63) /* No-execute */

/* 页大小 */
#define PAGE_SIZE_4K   0x1000
#define PAGE_SIZE_2M   0x200000
#define PAGE_SIZE_1G   0x40000000

/* 页表结构（x86-64 4级页表） */
typedef struct {
    u64 entries[512];
} __attribute__((aligned(PAGE_SIZE_4K))) page_table_t;

/* 页映射类型 */
typedef enum {
    MAP_TYPE_IDENTITY,      /* 恒等映射：虚拟地址 = 物理地址 */
    MAP_TYPE_KERNEL,        /* 内核映射 */
    MAP_TYPE_USER,          /* 用户映射 */
} map_type_t;

/* 页权限 */
typedef enum {
    PERM_NONE    = 0,
    PERM_READ    = PAGE_FLAG_PRESENT,
    PERM_WRITE   = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
    PERM_EXEC    = PAGE_FLAG_PRESENT,
    PERM_RW      = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
    PERM_RX      = PAGE_FLAG_PRESENT,
    PERM_RWX     = PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE,
} page_perm_t;

/* 页表管理接口 */
void pagetable_init(void);

/* 创建页表 */
page_table_t* pagetable_create(void);
void pagetable_destroy(page_table_t* root);

/* 映射物理页 */
hic_status_t pagetable_map(page_table_t* root, virt_addr_t virt, phys_addr_t phys, 
                           size_t size, page_perm_t perm, map_type_t type);

/* 取消映射 */
hic_status_t pagetable_unmap(page_table_t* root, virt_addr_t virt, size_t size);

/* 更改权限 */
hic_status_t pagetable_set_perm(page_table_t* root, virt_addr_t virt, 
                                size_t size, page_perm_t perm);

/* 获取物理地址 */
phys_addr_t pagetable_get_phys(page_table_t* root, virt_addr_t virt);

/* 切换页表 */
void pagetable_switch(page_table_t* root);

/* 清空TLB */
void pagetable_flush_tlb(virt_addr_t addr);
void pagetable_flush_tlb_all(void);

/* 域页表管理 */
hic_status_t pagetable_setup_domain(domain_id_t domain, page_table_t* root);
void pagetable_cleanup_domain(domain_id_t domain);

#endif /* HIC_KERNEL_PAGETABLE_H */
