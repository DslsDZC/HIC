/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC IPC 3.0 — Entry-Page Self-Check Cross-Domain Call
 *
 * Core mechanism from docs/TD/HICIPC模型3.0.md:
 *   call [entry_page] → bt [bitmap], ecx → jmp [business_page]
 *
 * Security model:
 *   - Entry page (RX): only visible service code to the caller
 *   - Business page: hidden from caller (page-fault-gated in Plan B)
 *   - Authorization: single bt instruction against per-service bitmap
 *   - Domain ID: stored in shared read-only page, set by kernel per domain
 *
 * Plan B (page-fault) flow:
 *   1. Caller: call [entry_page] → pushes return addr
 *   2. Entry:  bt check → jmp [business_page] → #PF
 *   3. Kernel: detects IPC3 fault → saves caller context
 *   4. Kernel: switches CR3, RSP → service domain context
 *   5. Service: executes, calls ipc3_return → restores caller
 */

#ifndef HIC_IPC3_H
#define HIC_IPC3_H

#include "types.h"
#include "capability.h"

/* ==================== Constants ==================== */

#define IPC3_MAX_SERVICES    128
#define IPC3_PAGE_SIZE       0x1000
#define IPC3_BITMAP_SIZE     32   /* 256 bits for HIC_DOMAIN_MAX */
#define IPC3_ENTRY_CODE_MAX  256  /* max code size in entry page */

/* Fixed virtual address for domain-ID data page (mapped RO in all domains) */
#define IPC3_DOMAIN_DATA_VA  0xFFFFF000ULL

/* ==================== Domain Data Page ==================== */

typedef struct __attribute__((packed)) ipc3_domain_data {
    u32 current_domain_id;           /* set by kernel before domain switch */
    u8  padding[IPC3_PAGE_SIZE - 4]; /* pad to 4KB */
} ipc3_domain_data_t;

/* ==================== Service Entry ==================== */

typedef u32 ipc3_service_id_t;
#define IPC3_SERVICE_INVALID ((ipc3_service_id_t)-1)

/* Authorization bitmap (one bit per domain) */
typedef struct ipc3_bitmap {
    u8 bits[IPC3_BITMAP_SIZE];
} ipc3_bitmap_t;

/* IPC 3.0 service descriptor (kernel-internal) */
typedef struct ipc3_service {
    phys_addr_t    entry_page_phys;    /* physical address of entry page */
    virt_addr_t    entry_page_virt;    /* mapped VA (identity) */
    virt_addr_t    business_addr;      /* business page entry point */
    virt_addr_t    service_stack;      /* service's stack pointer */
    domain_id_t    owner;              /* owning domain */
    bool           active;
    ipc3_bitmap_t  bitmap;             /* authorization bitmap */
} ipc3_service_t;

/* ==================== Saved Caller Context ==================== */

#define IPC3_CALL_STACK_DEPTH 16

typedef struct ipc3_saved_context {
    u64          r15, r14, r13, r12, r11, r10, r9, r8;
    u64          rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64          rip, cs, rflags, rsp, ss;
    phys_addr_t  cr3;
    domain_id_t  domain;
} ipc3_saved_context_t;

/* ==================== API ==================== */

/* Initialize IPC 3.0 subsystem */
void ipc3_init(void);

/* Register a service for IPC 3.0 access */
hic_status_t ipc3_register_service(domain_id_t owner,
                                   virt_addr_t business_entry,
                                   virt_addr_t service_stack,
                                   ipc3_service_id_t *out_id);

/* Set authorization: domain may call service */
hic_status_t ipc3_authorize(ipc3_service_id_t service_id, domain_id_t domain);

/* Revoke authorization */
hic_status_t ipc3_deauthorize(ipc3_service_id_t service_id, domain_id_t domain);

/* Get entry page virtual address (to give to callers) */
virt_addr_t ipc3_get_entry_va(ipc3_service_id_t service_id);

/*
 * Page fault handler for cross-domain jmp (Plan B).
 * Called from isr_14 assembly with stack frame access.
 *
 * @param cr2      fault address (from CR2)
 * @param err_code page fault error code
 * @param regs     pointer to saved regs on interrupt stack
 * @return true if fault was handled (IPC3 cross-domain call)
 */
bool ipc3_handle_pf(u64 cr2, u64 err_code, u64 *regs);

/*
 * IPC 3.0 return — service calls this to return to the caller domain.
 * Restores caller context saved during the cross-domain #PF.
 */
void ipc3_return(void);

/* Global IPC 3.0 domain data page (mapped in all domains) */
extern volatile ipc3_domain_data_t *g_ipc3_domain_data;

/* Pending CR3 for Plan B — set by ipc3_handle_pf, consumed by isr_14_handler asm */
extern volatile phys_addr_t g_ipc3_pending_cr3;

/* Return context pointer — set by ipc3_return C code, consumed by ipc3_return_asm */
extern volatile ipc3_saved_context_t *g_ipc3_return_ctx;

/* Unregister a service */
hic_status_t ipc3_unregister_service(ipc3_service_id_t service_id);

/* Map the domain data page into a specific domain's page table */
void ipc3_map_domain_data(domain_id_t domain);

#endif /* HIC_IPC3_H */
