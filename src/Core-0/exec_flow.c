/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Execution Flow Capability (EFC) Implementation
 *
 * Core-0 mechanism primitives for execution flow management.
 *
 * Design:
 * - EFC = TYPE_THREAD capability slot, thread context inlined via thread_t
 * - exec_flow_dispatch() is the ONLY context-switch entry point for P1
 * - Block/wake/yield are pure state transitions — no scheduling decisions
 * - Scheduling policy is delegated entirely to the Privileged-1 layer
 *
 * Thread lifecycle managed through capability system:
 * - cap_create_thread() creates the capability pointing to a thread_t
 * - cap_revoke() / exec_flow_destroy() reclaims all resources
 * - The capability acts as the unforgeable handle to the execution flow
 */

#include "exec_flow.h"
#include "thread.h"
#include "capability.h"
#include "lib/console.h"

/* ==================== Forward Declarations ==================== */

/* context_switch assembly in arch/x86_64/context.S */
extern void context_switch(void *prev, void *next);

/* ==================== Global State ==================== */

/* Current EFC (EXEC_FLOW_INVALID if not in EFC context) */
exec_flow_id_t g_current_efc = EXEC_FLOW_INVALID;

/* ==================== Internal Helpers ==================== */

/* Check if a capability ID refers to a valid, non-revoked TYPE_THREAD cap */
static inline bool efc_is_valid(exec_flow_id_t efc) {
    if (efc >= CAP_TABLE_SIZE) return false;
    cap_entry_t *entry = &g_global_cap_table[efc];
    return entry->cap_id == efc &&
           !(entry->flags & CAP_FLAG_REVOKED) &&
           (entry->rights & CAP_TYPE_THREAD);
}

/* Extract thread_id from an EFC capability entry */
static inline thread_id_t efc_get_thread(exec_flow_id_t efc) {
    if (efc >= CAP_TABLE_SIZE) return INVALID_THREAD;
    return g_global_cap_table[efc].thread_efc.thread_id;
}

/* ==================== Mechanism Primitives ==================== */

hic_status_t exec_flow_create(domain_id_t domain, virt_addr_t entry,
                              exec_flow_id_t *out)
{
    if (!out || domain >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }

    /* Create underlying thread (mechanism: stack alloc, context init) */
    thread_id_t tid;
    hic_status_t status = thread_create(domain, entry, HIC_PRIORITY_NORMAL, &tid);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* Create TYPE_THREAD capability pointing to this thread */
    cap_id_t cap;
    status = cap_create_thread(domain, tid, &cap);
    if (status != HIC_SUCCESS) {
        thread_terminate(tid);
        return status;
    }

    *out = cap;

    console_puts("[EFC] Created EFC ");
    console_putu32(cap);
    console_puts(" -> thread ");
    console_putu32(tid);
    console_puts(" in domain ");
    console_putu32(domain);
    console_puts("\n");

    return HIC_SUCCESS;
}

hic_status_t exec_flow_destroy(exec_flow_id_t efc)
{
    if (!efc_is_valid(efc)) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_id_t tid = efc_get_thread(efc);
    if (tid != INVALID_THREAD && tid < MAX_THREADS) {
        thread_terminate(tid);
    }

    /* Revoke the capability — marks entry as free */
    cap_revoke(efc);

    return HIC_SUCCESS;
}

hic_status_t exec_flow_dispatch(exec_flow_id_t efc, logical_core_id_t lcore)
{
    (void)lcore;  /* For now: dispatch to current core */

    if (!efc_is_valid(efc)) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_id_t tid = efc_get_thread(efc);
    if (tid >= MAX_THREADS) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_t *next = &g_threads[tid];
    if (next->state == THREAD_STATE_TERMINATED) {
        return HIC_ERROR_INVALID_STATE;
    }

    /* Save current thread state */
    thread_t *prev = g_current_thread;
    if (prev != NULL && prev->state == THREAD_STATE_RUNNING) {
        prev->state = THREAD_STATE_READY;
    }

    /* Mark target as running */
    next->state = THREAD_STATE_RUNNING;
    g_current_thread = next;
    g_current_efc = efc;

    /* Context switch: returns in the target EFC's context */
    context_switch(prev, next);

    /*
     * When we reach here, we've been dispatched back to this context
     * (or the caller was re-dispatched by the P1 scheduler).
     * The 'prev' pointer is whatever was running before us.
     */
    return HIC_SUCCESS;
}

hic_status_t exec_flow_block(exec_flow_id_t efc)
{
    if (!efc_is_valid(efc)) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_id_t tid = efc_get_thread(efc);
    if (tid >= MAX_THREADS) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_t *t = &g_threads[tid];

    /* Update state */
    t->state = THREAD_STATE_BLOCKED;
    g_current_efc = EXEC_FLOW_INVALID;

    return HIC_SUCCESS;
}

hic_status_t exec_flow_wake(exec_flow_id_t efc)
{
    if (!efc_is_valid(efc)) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_id_t tid = efc_get_thread(efc);
    if (tid >= MAX_THREADS) {
        return HIC_ERROR_CAP_INVALID;
    }

    thread_t *t = &g_threads[tid];
    if (t->state != THREAD_STATE_BLOCKED && t->state != THREAD_STATE_WAITING) {
        return HIC_ERROR_INVALID_STATE;
    }

    /* Transition to READY — P1 scheduler decides when to dispatch */
    t->state = THREAD_STATE_READY;

    return HIC_SUCCESS;
}

hic_status_t exec_flow_get_state(exec_flow_id_t efc, exec_flow_state_t *state)
{
    if (!state) return HIC_ERROR_INVALID_PARAM;
    if (!efc_is_valid(efc)) return HIC_ERROR_CAP_INVALID;

    thread_id_t tid = efc_get_thread(efc);
    if (tid >= MAX_THREADS) return HIC_ERROR_CAP_INVALID;

    *state = (exec_flow_state_t)(g_threads[tid].state);
    return HIC_SUCCESS;
}

void exec_flow_yield(void)
{
    if (g_current_thread != NULL && g_current_thread != &idle_thread) {
        g_current_thread->state = THREAD_STATE_READY;
    }
    g_current_efc = EXEC_FLOW_INVALID;
}
