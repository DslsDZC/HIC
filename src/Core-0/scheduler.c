/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Minimal Scheduler — Mechanism only, no policy.
 *
 * Per design docs/TD/执行流(面向未来设计).md:
 * - Core-0 provides context-switch mechanism and idle thread
 * - All scheduling POLICY decisions delegated to Privileged-1 layer
 * - exec_flow_dispatch() in exec_flow.c is the main entry for P1
 *
 * This module retains only what's needed for:
 *   1. Bootstrapping (initial idle thread, early boot context)
 *   2. Fallback (thread_exit → park to idle)
 *   3. Backward compat (scheduler_tick stub, schedule() fallback)
 */

#include "thread.h"
#include "types.h"
#include "atomic.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "logical_core.h"

/* ==================== External Assembly ==================== */

/* context_switch in arch/x86_64/context.S */
extern void context_switch(void *prev, void *next);

/* ==================== Global State ==================== */

/* Current running thread (NULL during early boot) */
thread_t *g_current_thread = NULL;

/* External thread table */
extern thread_t g_threads[MAX_THREADS];

/* ==================== Idle Thread ==================== */

thread_t idle_thread;

/* 8KB static stack for idle thread (multi-core safe) */
static u64 g_idle_stack[1024] __attribute__((aligned(16)));

/* ==================== Minimal Boot Scheduler ==================== */

/**
 * Simple round-robin scan for any READY thread.
 * Starts from the last scheduled thread + 1 to ensure fairness.
 * NO priority, NO policy, NO queues — pure mechanism.
 * Used only during boot before Privileged-1 scheduler takes over.
 */
static u32 g_last_scheduled = 0;

static thread_t *boot_pick_next(void)
{
    for (u32 offset = 1; offset <= MAX_THREADS; offset++) {
        u32 i = (g_last_scheduled + offset) % MAX_THREADS;
        thread_t *t = &g_threads[i];
        if (t->thread_id == i && t->state == THREAD_STATE_READY) {
            g_last_scheduled = i;
            return t;
        }
    }
    return &idle_thread;
}

/* ==================== Scheduler Init ==================== */

void scheduler_init(void)
{
    console_puts("[SCHED] Initializing minimal scheduler (mechanism only)\n");

    g_current_thread = NULL;

    /* Initialize idle thread */
    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    idle_thread.logical_core_id = INVALID_LOGICAL_CORE;

    /* Set up idle thread stack */
    idle_thread.stack_base = (virt_addr_t)g_idle_stack;
    idle_thread.stack_size = sizeof(g_idle_stack);
    u64 *stack_top = &g_idle_stack[1024];
    stack_top--;
    *stack_top = (u64)hal_halt;    /* idle entry = hal_halt */
    stack_top -= 6;                 /* callee-saved regs */
    idle_thread.stack_ptr = (virt_addr_t)stack_top;

    console_puts("[SCHED] Minimal scheduler initialized\n");
}

/* ==================== Schedule ==================== */

thread_t *schedule(void)
{
    bool irq_state = atomic_enter_critical();

    thread_t *prev = g_current_thread;

    /* Find next thread to run (boot scan: no policy) */
    thread_t *next = boot_pick_next();

    if (next == prev) {
        atomic_exit_critical(irq_state);
        return next;
    }

    /* Transition states */
    if (prev != NULL && prev->state == THREAD_STATE_RUNNING && prev != &idle_thread) {
        prev->state = THREAD_STATE_READY;
    }

    if (next != NULL) {
        next->state = THREAD_STATE_RUNNING;
    }
    g_current_thread = next;

    atomic_exit_critical(irq_state);

    /* Context switch */
    context_switch(prev, next);

    return next;
}

/* ==================== Scheduler Tick ==================== */

/**
 * Timer tick handler.
 * In the EFC design, time-slice management is handled by the
 * Privileged-1 scheduler. Core-0 provides this as a mechanism
 * notification point — policy decisions belong to the upper layer.
 */
void scheduler_tick(void)
{
    /* P1 scheduler handles preemption policy via its own timer mechanism */
}

/* ==================== Thread State Helpers (Backward Compat) ==================== */

void thread_yield(void)
{
    if (g_current_thread != NULL && g_current_thread != &idle_thread) {
        g_current_thread->state = THREAD_STATE_READY;
    }
    schedule();
}

hic_status_t thread_block(thread_id_t thread_id)
{
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;

    thread_t *thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;

    thread->state = THREAD_STATE_BLOCKED;

    if (g_current_thread == thread) {
        schedule();
    }

    return HIC_SUCCESS;
}

hic_status_t thread_wakeup(thread_id_t thread_id)
{
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;

    thread_t *thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;

    if (thread->state != THREAD_STATE_BLOCKED) return HIC_ERROR_INVALID_STATE;

    thread->state = THREAD_STATE_READY;

    return HIC_SUCCESS;
}

/**
 * Mark a thread as ready (for newly created threads).
 */
hic_status_t thread_ready(thread_id_t thread_id)
{
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;

    thread_t *thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;

    thread->state = THREAD_STATE_READY;

    return HIC_SUCCESS;
}

/* ==================== Stubs for Backward Compatibility ==================== */

thread_id_t scheduler_pick_next(void)
{
    thread_t *next = boot_pick_next();
    return next->thread_id;
}

void context_switch_to(thread_id_t next_thread)
{
    if (next_thread >= MAX_THREADS) return;

    thread_t *next = &g_threads[next_thread];

    thread_t *prev = g_current_thread;
    if (prev != NULL && prev->state == THREAD_STATE_RUNNING) {
        prev->state = THREAD_STATE_READY;
    }

    next->state = THREAD_STATE_RUNNING;
    g_current_thread = next;

    context_switch(prev, next);
}

void scheduler_get_perf(u64 *schedule_count, u64 *avg_cycles, u64 *max_cycles)
{
    (void)schedule_count; (void)avg_cycles; (void)max_cycles;
    /* No performance counters in minimal scheduler */
}

void scheduler_print_perf(void)
{
    console_puts("[SCHED] Minimal scheduler — no performance counters\n");
}

void thread_check_timeouts(void)
{
    /* Timeout management handled by Privileged-1 scheduler */
}
