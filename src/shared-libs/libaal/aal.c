/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * AAL (Architecture Adaptation Layer) - 共享库实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* 系统调用号 */
#define SYSCALL_AAL_BASE           0x1100

#define SYSCALL_AAL_PGTBL_CREATE   (SYSCALL_AAL_BASE + 0)
#define SYSCALL_AAL_PGTBL_DESTROY  (SYSCALL_AAL_BASE + 1)
#define SYSCALL_AAL_PAGE_MAP       (SYSCALL_AAL_BASE + 2)
#define SYSCALL_AAL_PAGE_UNMAP     (SYSCALL_AAL_BASE + 3)
#define SYSCALL_AAL_PAGE_GET_PHYS  (SYSCALL_AAL_BASE + 4)
#define SYSCALL_AAL_PAGE_SET_FLAGS (SYSCALL_AAL_BASE + 5)
#define SYSCALL_AAL_TLB_FLUSH_ALL  (SYSCALL_AAL_BASE + 10)
#define SYSCALL_AAL_TLB_FLUSH_PAGE (SYSCALL_AAL_BASE + 11)
#define SYSCALL_AAL_CTX_SAVE       (SYSCALL_AAL_BASE + 20)
#define SYSCALL_AAL_CTX_RESTORE    (SYSCALL_AAL_BASE + 21)
#define SYSCALL_AAL_CTX_SWITCH     (SYSCALL_AAL_BASE + 22)
#define SYSCALL_AAL_CTX_INIT       (SYSCALL_AAL_BASE + 23)
#define SYSCALL_AAL_GET_CR3        (SYSCALL_AAL_BASE + 30)
#define SYSCALL_AAL_SET_CR3        (SYSCALL_AAL_BASE + 31)
#define SYSCALL_AAL_GET_FS_BASE    (SYSCALL_AAL_BASE + 32)
#define SYSCALL_AAL_SET_FS_BASE    (SYSCALL_AAL_BASE + 33)
#define SYSCALL_AAL_GET_GS_BASE    (SYSCALL_AAL_BASE + 34)
#define SYSCALL_AAL_SET_GS_BASE    (SYSCALL_AAL_BASE + 35)
#define SYSCALL_AAL_CPUID          (SYSCALL_AAL_BASE + 40)
#define SYSCALL_AAL_READ_MSR       (SYSCALL_AAL_BASE + 41)
#define SYSCALL_AAL_WRITE_MSR      (SYSCALL_AAL_BASE + 42)
#define SYSCALL_AAL_CACHE_FLUSH    (SYSCALL_AAL_BASE + 50)

extern long syscall0(long num);
extern long syscall1(long num, long a1);
extern long syscall2(long num, long a1, long a2);
extern long syscall3(long num, long a1, long a2, long a3);
extern long syscall4(long num, long a1, long a2, long a3, long a4);

/* ==================== 页表操作 ==================== */

typedef uint64_t aal_pgtbl_t;
typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;

aal_pgtbl_t aal_page_table_create(void) {
    return (aal_pgtbl_t)syscall0(SYSCALL_AAL_PGTBL_CREATE);
}

int aal_page_table_destroy(aal_pgtbl_t pgtbl) {
    return (int)syscall1(SYSCALL_AAL_PGTBL_DESTROY, (long)pgtbl);
}

int aal_page_map(aal_pgtbl_t pgtbl, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
    return (int)syscall4(SYSCALL_AAL_PAGE_MAP, (long)pgtbl, (long)vaddr, (long)paddr, (long)flags);
}

int aal_page_unmap(aal_pgtbl_t pgtbl, vaddr_t vaddr) {
    return (int)syscall2(SYSCALL_AAL_PAGE_UNMAP, (long)pgtbl, (long)vaddr);
}

paddr_t aal_page_get_phys(aal_pgtbl_t pgtbl, vaddr_t vaddr) {
    return (paddr_t)syscall2(SYSCALL_AAL_PAGE_GET_PHYS, (long)pgtbl, (long)vaddr);
}

int aal_page_set_flags(aal_pgtbl_t pgtbl, vaddr_t vaddr, uint32_t flags) {
    return (int)syscall3(SYSCALL_AAL_PAGE_SET_FLAGS, (long)pgtbl, (long)vaddr, (long)flags);
}

/* ==================== TLB 操作 ==================== */

void aal_tlb_flush_all(void) {
    syscall0(SYSCALL_AAL_TLB_FLUSH_ALL);
}

void aal_tlb_flush_page(vaddr_t vaddr) {
    syscall1(SYSCALL_AAL_TLB_FLUSH_PAGE, (long)vaddr);
}

void aal_tlb_flush_range(vaddr_t vaddr, size_t size) {
    (void)vaddr;
    (void)size;
    aal_tlb_flush_all();
}

/* ==================== 上下文操作 ==================== */

typedef struct aal_context aal_context_t;

int aal_context_save(aal_context_t *ctx) {
    return (int)syscall1(SYSCALL_AAL_CTX_SAVE, (long)ctx);
}

int aal_context_restore(aal_context_t *ctx) {
    return (int)syscall1(SYSCALL_AAL_CTX_RESTORE, (long)ctx);
}

void aal_context_switch(aal_context_t *from, aal_context_t *to) {
    syscall2(SYSCALL_AAL_CTX_SWITCH, (long)from, (long)to);
}

int aal_context_init(aal_context_t *ctx, void *entry, void *stack) {
    return (int)syscall3(SYSCALL_AAL_CTX_INIT, (long)ctx, (long)entry, (long)stack);
}

/* ==================== 特殊寄存器 ==================== */

uint64_t aal_get_cr3(void) {
    return (uint64_t)syscall0(SYSCALL_AAL_GET_CR3);
}

void aal_set_cr3(uint64_t cr3) {
    syscall1(SYSCALL_AAL_SET_CR3, (long)cr3);
}

uint64_t aal_get_fs_base(void) {
    return (uint64_t)syscall0(SYSCALL_AAL_GET_FS_BASE);
}

void aal_set_fs_base(uint64_t base) {
    syscall1(SYSCALL_AAL_SET_FS_BASE, (long)base);
}

uint64_t aal_get_gs_base(void) {
    return (uint64_t)syscall0(SYSCALL_AAL_GET_GS_BASE);
}

void aal_set_gs_base(uint64_t base) {
    syscall1(SYSCALL_AAL_SET_GS_BASE, (long)base);
}

/* ==================== CPU 特性 ==================== */

typedef struct {
    uint32_t leaf;
    uint32_t subleaf;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} aal_cpuid_t;

int aal_cpuid(aal_cpuid_t *info) {
    return (int)syscall1(SYSCALL_AAL_CPUID, (long)info);
}

uint64_t aal_read_msr(uint32_t msr) {
    return (uint64_t)syscall1(SYSCALL_AAL_READ_MSR, (long)msr);
}

void aal_write_msr(uint32_t msr, uint64_t value) {
    syscall2(SYSCALL_AAL_WRITE_MSR, (long)msr, (long)value);
}

/* ==================== 缓存 ==================== */

void aal_cache_flush(void *addr, size_t size) {
    syscall2(SYSCALL_AAL_CACHE_FLUSH, (long)addr, (long)size);
}

void aal_cache_invalidate(void *addr, size_t size) {
    aal_cache_flush(addr, size);
}

void aal_cache_prefetch(const void *addr) {
    (void)addr;
}

/* ==================== 库加载辅助 ==================== */

typedef struct aal_lib_handle aal_lib_handle_t;

aal_lib_handle_t *aal_lib_load(void) { return 0; }
void aal_lib_unload(aal_lib_handle_t *handle) { (void)handle; }
uint32_t aal_lib_get_version(aal_lib_handle_t *handle) { (void)handle; return 0; }
