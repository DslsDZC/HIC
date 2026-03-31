/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * IMAL (Isolation Mechanism Abstraction Layer) - 共享库实现
 */

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

/* 系统调用号 */
#define SYSCALL_IMAL_BASE          0x1200

#define SYSCALL_IMAL_DOMAIN_CREATE (SYSCALL_IMAL_BASE + 0)
#define SYSCALL_IMAL_DOMAIN_DESTROY (SYSCALL_IMAL_BASE + 1)
#define SYSCALL_IMAL_DOMAIN_SWITCH (SYSCALL_IMAL_BASE + 2)
#define SYSCALL_IMAL_DOMAIN_CURRENT (SYSCALL_IMAL_BASE + 3)
#define SYSCALL_IMAL_MAP           (SYSCALL_IMAL_BASE + 10)
#define SYSCALL_IMAL_UNMAP         (SYSCALL_IMAL_BASE + 11)
#define SYSCALL_IMAL_REMAP         (SYSCALL_IMAL_BASE + 12)
#define SYSCALL_IMAL_PROTECT       (SYSCALL_IMAL_BASE + 13)
#define SYSCALL_IMAL_QUERY         (SYSCALL_IMAL_BASE + 14)
#define SYSCALL_IMAL_TLB_FLUSH_ALL (SYSCALL_IMAL_BASE + 20)
#define SYSCALL_IMAL_TLB_FLUSH_DOMAIN (SYSCALL_IMAL_BASE + 21)
#define SYSCALL_IMAL_TLB_FLUSH_VA  (SYSCALL_IMAL_BASE + 22)
#define SYSCALL_IMAL_CHECK_PERM    (SYSCALL_IMAL_BASE + 30)
#define SYSCALL_IMAL_GET_PHYS      (SYSCALL_IMAL_BASE + 31)
#define SYSCALL_IMAL_SET_OWNER     (SYSCALL_IMAL_BASE + 40)
#define SYSCALL_IMAL_GET_OWNER     (SYSCALL_IMAL_BASE + 41)
#define SYSCALL_IMAL_GET_INFO      (SYSCALL_IMAL_BASE + 42)

extern long syscall0(long num);
extern long syscall1(long num, long a1);
extern long syscall2(long num, long a1, long a2);
extern long syscall3(long num, long a1, long a2, long a3);
extern long syscall4(long num, long a1, long a2, long a3, long a4);
extern long syscall5(long num, long a1, long a2, long a3, long a4, long a5);

/* ==================== 类型定义 ==================== */

typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;
typedef uint32_t domain_id_t;
typedef uint64_t imal_domain_t;
typedef uint32_t imal_perm_t;

typedef enum {
    IMAL_OK = 0,
    IMAL_ERR_NOMEM = 1,
    IMAL_ERR_INVAL = 2,
    IMAL_ERR_PERM = 3,
    IMAL_ERR_RANGE = 4,
} imal_error_t;

/* ==================== 域操作 ==================== */

imal_domain_t imal_domain_create(void) {
    return (imal_domain_t)syscall0(SYSCALL_IMAL_DOMAIN_CREATE);
}

imal_error_t imal_domain_destroy(imal_domain_t domain) {
    return (imal_error_t)syscall1(SYSCALL_IMAL_DOMAIN_DESTROY, (long)domain);
}

imal_error_t imal_domain_switch(imal_domain_t domain) {
    return (imal_error_t)syscall1(SYSCALL_IMAL_DOMAIN_SWITCH, (long)domain);
}

imal_domain_t imal_domain_get_current(void) {
    return (imal_domain_t)syscall0(SYSCALL_IMAL_DOMAIN_CURRENT);
}

/* ==================== 映射操作 ==================== */

imal_error_t imal_map(imal_domain_t domain, vaddr_t vaddr,
                       paddr_t paddr, size_t size, imal_perm_t perm) {
    return (imal_error_t)syscall5(SYSCALL_IMAL_MAP, (long)domain,
                                   (long)vaddr, (long)paddr, (long)size, (long)perm);
}

imal_error_t imal_unmap(imal_domain_t domain, vaddr_t vaddr, size_t size) {
    return (imal_error_t)syscall3(SYSCALL_IMAL_UNMAP, (long)domain, (long)vaddr, (long)size);
}

imal_error_t imal_remap(imal_domain_t domain, vaddr_t old_vaddr,
                         vaddr_t new_vaddr, size_t size) {
    return (imal_error_t)syscall4(SYSCALL_IMAL_REMAP, (long)domain,
                                   (long)old_vaddr, (long)new_vaddr, (long)size);
}

imal_error_t imal_protect(imal_domain_t domain, vaddr_t vaddr,
                           size_t size, imal_perm_t perm) {
    return (imal_error_t)syscall4(SYSCALL_IMAL_PROTECT, (long)domain,
                                   (long)vaddr, (long)size, (long)perm);
}

imal_error_t imal_query(imal_domain_t domain, vaddr_t vaddr,
                         paddr_t *paddr, imal_perm_t *perm) {
    return (imal_error_t)syscall4(SYSCALL_IMAL_QUERY, (long)domain,
                                   (long)vaddr, (long)paddr, (long)perm);
}

/* ==================== TLB 操作 ==================== */

void imal_tlb_flush_all(void) {
    syscall0(SYSCALL_IMAL_TLB_FLUSH_ALL);
}

void imal_tlb_flush_domain(imal_domain_t domain) {
    syscall1(SYSCALL_IMAL_TLB_FLUSH_DOMAIN, (long)domain);
}

void imal_tlb_flush_va(vaddr_t vaddr) {
    syscall1(SYSCALL_IMAL_TLB_FLUSH_VA, (long)vaddr);
}

/* ==================== 权限查询 ==================== */

imal_error_t imal_check_perm(imal_domain_t domain, vaddr_t vaddr,
                              size_t size, imal_perm_t perm) {
    return (imal_error_t)syscall4(SYSCALL_IMAL_CHECK_PERM, (long)domain,
                                   (long)vaddr, (long)size, (long)perm);
}

paddr_t imal_get_phys(imal_domain_t domain, vaddr_t vaddr) {
    return (paddr_t)syscall2(SYSCALL_IMAL_GET_PHYS, (long)domain, (long)vaddr);
}

/* ==================== 域属性 ==================== */

void imal_set_owner(imal_domain_t domain, domain_id_t owner) {
    syscall2(SYSCALL_IMAL_SET_OWNER, (long)domain, (long)owner);
}

domain_id_t imal_get_owner(imal_domain_t domain) {
    return (domain_id_t)syscall1(SYSCALL_IMAL_GET_OWNER, (long)domain);
}

typedef struct {
    size_t total_mappings;
    size_t total_pages;
    size_t code_pages;
    size_t data_pages;
    domain_id_t owner;
    uint32_t flags;
} imal_domain_info_t;

imal_error_t imal_get_info(imal_domain_t domain, imal_domain_info_t *info) {
    return (imal_error_t)syscall2(SYSCALL_IMAL_GET_INFO, (long)domain, (long)info);
}

/* ==================== 库加载辅助 ==================== */

typedef struct imal_lib_handle imal_lib_handle_t;

imal_lib_handle_t *imal_lib_load(void) { return 0; }
void imal_lib_unload(imal_lib_handle_t *handle) { (void)handle; }
uint32_t imal_lib_get_version(imal_lib_handle_t *handle) { (void)handle; return 0; }
