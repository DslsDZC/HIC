/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC IPC 3.0 — Entry-Page Self-Check Cross-Domain Call
 *
 * Core mechanism:
 *   call [entry_page] → bt [bitmap], ecx → jmp [business_page]
 *
 * Plan B (page-fault gated):
 *   1. Caller: call [entry_page] → pushes return addr
 *   2. Entry:  bt check → jmp [business_page] → #PF
 *   3. Kernel: saves caller context, switches CR3+RSP
 *   4. Service: executes, calls ipc3_return → restores caller
 *
 * Each entry page is a 4KB RX page containing:
 *   [0x00-0x1F]  authorization bitmap (32 bytes = 256 bits)
 *   [0x20-0x27]  business page address (8 bytes)
 *   [0x28+]      entry code (bt check + jmp)
 */

#include "ipc3.h"
#include "pmm.h"
#include "pagetable.h"
#include "domain_switch.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "atomic.h"

/* ==================== Internal State ==================== */

/* Service table — flat array, no allocation */
static ipc3_service_t g_services[IPC3_MAX_SERVICES];
static u32 g_service_count = 0;

/* Call stack for Plan B saved contexts */
static struct {
    ipc3_saved_context_t slots[IPC3_CALL_STACK_DEPTH];
    u32 top;
} g_call_stack;

/* Domain data page — mapped at IPC3_DOMAIN_DATA_VA in all domains */
volatile ipc3_domain_data_t *g_ipc3_domain_data = NULL;
static phys_addr_t g_domain_data_phys = 0;

/* Pending CR3 for assembly trampoline (set by ipc3_handle_pf) */
volatile phys_addr_t g_ipc3_pending_cr3 = 0;

/* Return context pointer (set by ipc3_return C code, used by asm trampoline) */
volatile ipc3_saved_context_t *g_ipc3_return_ctx = NULL;

static bool g_ipc3_initialized = false;

/* ==================== Entry Page Layout ==================== */

/* Layout of entry page data before code */
#define ENTRY_BITMAP_OFFSET   0x00
#define ENTRY_BUSADDR_OFFSET  0x20
#define ENTRY_CODE_OFFSET     0x28

/* ==================== Forward Declarations ==================== */

static hic_status_t build_entry_page(ipc3_service_t *svc);

/* ==================== Entry Page Code Generation ==================== */

/**
 * Build x86-64 machine code for the entry page.
 *
 * Code layout (all offsets from page start):
 *   0x28:  48 B8 00 F0 FF FF 00 00 00 00   mov rax, 0xFFFFF000
 *   0x32:  8B 08                             mov ecx, [rax]
 *   0x34:  0F A3 0D C5 FF FF FF             bt [rip-0x3B], ecx
 *   0x3B:  73 05                             jnc +5 (to reject at 0x42)
 *   0x3D:  FF 25 DD FF FF FF                jmp [rip-0x23] (to busaddr at 0x20)
 *   0x43:  FA                                cli
 *   0x44:  F4                                hlt
 *   0x45:  EB FD                             jmp -3 (back to hlt)
 */
static hic_status_t build_entry_page(ipc3_service_t *svc)
{
    u8 *page = (u8 *)svc->entry_page_virt;
    if (!page) return HIC_ERROR_INVALID_PARAM;

    /* Fill the entry code at ENTRY_CODE_OFFSET */
    u8 code[] = {
        /* 0x28: mov rax, 0xFFFFF000 */
        0x48, 0xB8, 0x00, 0xF0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        /* 0x32: mov ecx, [rax] */
        0x8B, 0x08,
        /* 0x34: bt [rip-0x3B], ecx  (RIP-relative to bitmap at 0x00, next_rip=0x3B) */
        0x0F, 0xA3, 0x0D, 0xC5, 0xFF, 0xFF, 0xFF,
        /* 0x3B: jnc +5 (to 0x42) */
        0x73, 0x05,
        /* 0x3D: jmp [rip-0x23] (RIP-relative to busaddr at 0x20, next_rip=0x43) */
        0xFF, 0x25, 0xDD, 0xFF, 0xFF, 0xFF,
        /* 0x43: cli; hlt; jmp -3 */
        0xFA, 0xF4, 0xEB, 0xFD,
    };

    memcopy(page + ENTRY_CODE_OFFSET, code, sizeof(code));

    /* Fill rest of entry page with ud2 (0F 0B) — crashes if execution ever reaches it */
    for (u32 i = ENTRY_CODE_OFFSET + sizeof(code); i < IPC3_PAGE_SIZE; i += 2) {
        page[i]     = 0x0F;
        page[i + 1] = 0x0B;
    }

    /* Fill bitmap (initially all zeros = no authorization) */
    memzero(page + ENTRY_BITMAP_OFFSET, IPC3_BITMAP_SIZE);

    /* Store business page address */
    *(u64 *)(page + ENTRY_BUSADDR_OFFSET) = svc->business_addr;

    return HIC_SUCCESS;
}

/* ==================== Domain Data Page Mapping ==================== */

void ipc3_map_domain_data(domain_id_t domain)
{
    if (g_domain_data_phys == 0) return;

    page_table_t *pt = domain_switch_get_pagetable(domain);
    if (pt) {
        pagetable_map(pt, IPC3_DOMAIN_DATA_VA, g_domain_data_phys,
                      IPC3_PAGE_SIZE, PERM_READ, MAP_TYPE_KERNEL);
    }
}

/* ==================== Initialization ==================== */

void ipc3_init(void)
{
    console_puts("[IPC3] Initializing IPC 3.0 subsystem\n");

    /* Allocate domain data page from Core-0 memory */
    phys_addr_t data_phys;
    hic_status_t status = pmm_alloc_frames(HIC_DOMAIN_CORE, 1, PAGE_FRAME_CORE, &data_phys);
    if (status != HIC_SUCCESS) {
        console_puts("[IPC3] FAILED to allocate domain data page\n");
        return;
    }

    /* Map domain data page at the fixed VA in Core-0 page table */
    page_table_t *core_pt = domain_switch_get_pagetable(HIC_DOMAIN_CORE);
    if (!core_pt) {
        console_puts("[IPC3] FAILED to get Core-0 page table\n");
        pmm_free_frames(data_phys, 1);
        return;
    }

    status = pagetable_map(core_pt, IPC3_DOMAIN_DATA_VA, data_phys,
                           IPC3_PAGE_SIZE, PERM_READ, MAP_TYPE_KERNEL);
    if (status != HIC_SUCCESS) {
        console_puts("[IPC3] FAILED to map domain data page\n");
        pmm_free_frames(data_phys, 1);
        return;
    }

    /* Initialize domain data */
    g_ipc3_domain_data = (volatile ipc3_domain_data_t *)IPC3_DOMAIN_DATA_VA;
    g_ipc3_domain_data->current_domain_id = HIC_DOMAIN_CORE;
    g_domain_data_phys = data_phys;

    /* Initialize call stack */
    memzero((void *)g_call_stack.slots, sizeof(g_call_stack.slots));
    g_call_stack.top = 0;

    /* Initialize service table */
    memzero(g_services, sizeof(g_services));
    g_service_count = 0;

    /* IPC3 state */
    g_ipc3_pending_cr3 = 0;
    g_ipc3_return_ctx = NULL;
    g_ipc3_initialized = true;

    console_puts("[IPC3] IPC 3.0 subsystem initialized (domain_data_page at 0x");
    console_puthex64(data_phys);
    console_puts(")\n");
}

/* ==================== Service Registration ==================== */

hic_status_t ipc3_register_service(domain_id_t owner,
                                   virt_addr_t business_entry,
                                   virt_addr_t service_stack,
                                   ipc3_service_id_t *out_id)
{
    if (!g_ipc3_initialized) return HIC_ERROR_INVALID_STATE;
    if (g_service_count >= IPC3_MAX_SERVICES) return HIC_ERROR_NO_RESOURCE;

    bool irq_state = atomic_enter_critical();

    ipc3_service_id_t id = (ipc3_service_id_t)g_service_count;
    ipc3_service_t *svc = &g_services[id];

    /* Allocate physical page for entry page from Core-0 (accessible to all domains) */
    phys_addr_t entry_phys;
    hic_status_t status = pmm_alloc_frames(HIC_DOMAIN_CORE, 1, PAGE_FRAME_CORE, &entry_phys);
    if (status != HIC_SUCCESS) {
        atomic_exit_critical(irq_state);
        return status;
    }

    /* Map entry page identity in Core-0 page table as supervisor RX */
    page_table_t *pt = domain_switch_get_pagetable(HIC_DOMAIN_CORE);
    if (!pt) {
        pmm_free_frames(entry_phys, 1);
        atomic_exit_critical(irq_state);
        return HIC_ERROR_INVALID_STATE;
    }

    status = pagetable_map(pt, entry_phys, entry_phys,
                           IPC3_PAGE_SIZE, PERM_RX, MAP_TYPE_IDENTITY);
    if (status != HIC_SUCCESS) {
        pmm_free_frames(entry_phys, 1);
        atomic_exit_critical(irq_state);
        return status;
    }

    /* Fill in service descriptor */
    svc->entry_page_phys = entry_phys;
    svc->entry_page_virt = entry_phys;  /* identity-mapped */
    svc->business_addr = business_entry;
    svc->service_stack = service_stack;
    svc->owner = owner;
    svc->active = true;
    memzero(&svc->bitmap, sizeof(svc->bitmap));

    /* Build entry page (write bitmap, business addr, code) */
    status = build_entry_page(svc);
    if (status != HIC_SUCCESS) {
        pagetable_unmap(pt, entry_phys, IPC3_PAGE_SIZE);
        pmm_free_frames(entry_phys, 1);
        svc->active = false;
        atomic_exit_critical(irq_state);
        return status;
    }

    g_service_count++;
    *out_id = id;

    atomic_exit_critical(irq_state);

    console_puts("[IPC3] Registered service id=");
    console_putu32(id);
    console_puts(" owner=");
    console_putu32(owner);
    console_puts(" entry=0x");
    console_puthex64(entry_phys);
    console_puts(" business=0x");
    console_puthex64(business_entry);
    console_puts("\n");

    return HIC_SUCCESS;
}

hic_status_t ipc3_unregister_service(ipc3_service_id_t service_id)
{
    if (!g_ipc3_initialized) return HIC_ERROR_INVALID_STATE;
    if (service_id >= IPC3_MAX_SERVICES) return HIC_ERROR_INVALID_PARAM;

    bool irq_state = atomic_enter_critical();

    ipc3_service_t *svc = &g_services[service_id];
    if (!svc->active) {
        atomic_exit_critical(irq_state);
        return HIC_ERROR_INVALID_PARAM;
    }

    /* Unmap entry page from Core-0 page table */
    page_table_t *pt = domain_switch_get_pagetable(HIC_DOMAIN_CORE);
    if (pt) {
        pagetable_unmap(pt, svc->entry_page_phys, IPC3_PAGE_SIZE);
    }

    /* Free the physical page */
    pmm_free_frames(svc->entry_page_phys, 1);

    /* Mark inactive */
    memzero(svc, sizeof(ipc3_service_t));
    svc->active = false;

    atomic_exit_critical(irq_state);

    return HIC_SUCCESS;
}

/* ==================== Authorization ==================== */

hic_status_t ipc3_authorize(ipc3_service_id_t service_id, domain_id_t domain)
{
    if (!g_ipc3_initialized) return HIC_ERROR_INVALID_STATE;
    if (service_id >= IPC3_MAX_SERVICES) return HIC_ERROR_INVALID_PARAM;
    if (domain >= HIC_DOMAIN_MAX) return HIC_ERROR_INVALID_PARAM;

    ipc3_service_t *svc = &g_services[service_id];
    if (!svc->active) return HIC_ERROR_INVALID_PARAM;

    bool irq_state = atomic_enter_critical();

    /* Update bitmap in entry page */
    u8 *bitmap = (u8 *)svc->entry_page_virt + ENTRY_BITMAP_OFFSET;
    u32 byte_idx = domain / 8;
    u32 bit_idx  = domain % 8;
    bitmap[byte_idx] |= (1U << bit_idx);

    /* Also update the local copy */
    svc->bitmap.bits[byte_idx] |= (1U << bit_idx);

    atomic_exit_critical(irq_state);

    return HIC_SUCCESS;
}

hic_status_t ipc3_deauthorize(ipc3_service_id_t service_id, domain_id_t domain)
{
    if (!g_ipc3_initialized) return HIC_ERROR_INVALID_STATE;
    if (service_id >= IPC3_MAX_SERVICES) return HIC_ERROR_INVALID_PARAM;
    if (domain >= HIC_DOMAIN_MAX) return HIC_ERROR_INVALID_PARAM;

    ipc3_service_t *svc = &g_services[service_id];
    if (!svc->active) return HIC_ERROR_INVALID_PARAM;

    bool irq_state = atomic_enter_critical();

    u8 *bitmap = (u8 *)svc->entry_page_virt + ENTRY_BITMAP_OFFSET;
    u32 byte_idx = domain / 8;
    u32 bit_idx  = domain % 8;
    bitmap[byte_idx] = (u8)(bitmap[byte_idx] & ~(1U << bit_idx));

    svc->bitmap.bits[byte_idx] = (u8)(svc->bitmap.bits[byte_idx] & ~(1U << bit_idx));

    atomic_exit_critical(irq_state);

    return HIC_SUCCESS;
}

/* ==================== Entry Page Access ==================== */

virt_addr_t ipc3_get_entry_va(ipc3_service_id_t service_id)
{
    if (service_id >= IPC3_MAX_SERVICES) return 0;
    ipc3_service_t *svc = &g_services[service_id];
    if (!svc->active) return 0;
    return svc->entry_page_virt;
}

/* ==================== Plan B — Page Fault Handler ==================== */

/**
 * Handle page fault for IPC 3.0 cross-domain calls (Plan B).
 *
 * Called from the custom isr_14_handler in interrupts.S with:
 *   rdi = CR2 (fault address)
 *   rsi = error code
 *   rdx = pointer to saved regs on interrupt stack
 *
 * Stack layout at regs (15 pushes + vector + errcode + CPU frame):
 *   regs[0]   = r15    regs[8]  = rbp   regs[15] = vector # (14)
 *   regs[1]   = r14    regs[9]  = rdi   regs[16] = error code
 *   regs[2]   = r13    regs[10] = rsi   regs[17] = RIP
 *   regs[3]   = r12    regs[11] = rdx   regs[18] = CS
 *   regs[4]   = r11    regs[12] = rcx   regs[19] = RFLAGS
 *   regs[5]   = r10    regs[13] = rbx   regs[20] = RSP
 *   regs[6]   = r9     regs[14] = rax   regs[21] = SS
 *   regs[7]   = r8
 *
 * @return true if fault was handled as IPC3 cross-domain call
 */
bool ipc3_handle_pf(u64 cr2, u64 err_code, u64 *regs)
{
    (void)err_code;

    if (!g_ipc3_initialized) return false;

    /* Step 1: find which service's business page was faulted on */
    ipc3_service_t *svc = NULL;

    for (u32 i = 0; i < g_service_count; i++) {
        if (g_services[i].active && g_services[i].business_addr == cr2) {
            svc = &g_services[i];
            break;
        }
    }
    if (!svc) return false;

    /* Step 2: verify the fault originated from within this service's entry page */
    u64 fault_rip = regs[17];
    if (fault_rip < svc->entry_page_virt ||
        fault_rip >= svc->entry_page_virt + IPC3_PAGE_SIZE) {
        /* Not an IPC3 call — a real fault within the business page region */
        return false;
    }

    /* Step 3: re-verify authorization (the bt in entry page already checked,
     * but we double-check because the bitmap could have changed between
     * the bt and the jmp — TOCTOU mitigated by atomicity of entry page */
    u32 domain_id = g_ipc3_domain_data->current_domain_id;
    if (domain_id >= HIC_DOMAIN_MAX) return false;

    u32 byte_idx = domain_id / 8;
    u32 bit_idx  = domain_id % 8;
    u8 *bitmap = (u8 *)svc->entry_page_virt + ENTRY_BITMAP_OFFSET;
    if (!(bitmap[byte_idx] & (1U << bit_idx))) {
        /* Authorization changed between bt and jmp — deny */
        return false;
    }

    /* Step 4: save caller context onto the call stack */
    if (g_call_stack.top >= IPC3_CALL_STACK_DEPTH) {
        /* Call stack overflow — should never happen with trusted services */
        console_puts("[IPC3] ERROR: call stack overflow!\n");
        return false;
    }

    ipc3_saved_context_t *ctx = &g_call_stack.slots[g_call_stack.top];

    ctx->r15 = regs[0];  ctx->r14 = regs[1];  ctx->r13 = regs[2];
    ctx->r12 = regs[3];  ctx->r11 = regs[4];  ctx->r10 = regs[5];
    ctx->r9  = regs[6];  ctx->r8  = regs[7];  ctx->rbp = regs[8];
    ctx->rdi = regs[9];  ctx->rsi = regs[10]; ctx->rdx = regs[11];
    ctx->rcx = regs[12]; ctx->rbx = regs[13]; ctx->rax = regs[14];

    ctx->rip = regs[17];  ctx->cs  = regs[18];
    ctx->rflags = regs[19];
    ctx->rsp = regs[20];  ctx->ss  = regs[21];

    /* Capture caller's CR3 (physical address of page table root) */
    page_table_t *caller_pt = domain_switch_get_pagetable(domain_id);
    ctx->cr3 = (phys_addr_t)caller_pt;
    ctx->domain = domain_id;

    g_call_stack.top++;

    /* Step 5: modify the interrupt frame to return to service domain */

    /* Set RIP to business page entry point */
    regs[17] = svc->business_addr;

    /* Set RSP to service's registered stack */
    regs[20] = svc->service_stack;

    /* CS/SS remain the same (all ring 0 in HIC) */
    regs[18] = 0x08;     /* GDT_KERNEL_CS << 3 */
    regs[21] = 0x10;     /* kernel SS */

    /* Enable interrupts for the service */
    regs[19] = 0x202;    /* RFLAGS with IF=1 */

    /* Zero out general registers for the service, set RDI = caller domain */
    for (int i = 0; i < 15; i++) {
        regs[i] = 0;
    }
    regs[9] = domain_id;  /* rdi = caller domain ID as service arg */

    /* Step 6: set pending CR3 so the assembly trampoline switches page tables */
    page_table_t *svc_pt = domain_switch_get_pagetable(svc->owner);
    g_ipc3_pending_cr3 = (phys_addr_t)svc_pt;

    return true;
}

/* ==================== IPC3 Return (C entry) ==================== */

/**
 * Called by service code to return to the caller domain.
 * Saves the return context pointer for the assembly trampoline,
 * which never returns — execution resumes in the caller.
 */
void ipc3_return(void)
{
    bool irq_state = hal_disable_interrupts();

    if (g_call_stack.top == 0) {
        /* No saved context — nothing to return to */
        console_puts("[IPC3] ERROR: ipc3_return with empty call stack, halting\n");
        hal_halt();
        /* not reached */
    }

    g_call_stack.top--;
    ipc3_saved_context_t *ctx = &g_call_stack.slots[g_call_stack.top];

    /* Store context pointer for the assembly trampoline */
    g_ipc3_return_ctx = ctx;

    /* The assembly trampoline restores caller context and never returns.
     * Declared in interrupts.S */
    extern void ipc3_return_asm(void);
    ipc3_return_asm();

    /* NOT REACHED */
    hal_halt();
}
