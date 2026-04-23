/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Execution Flow Capability (EFC) Interface
 *
 * Core-0 mechanism primitives for execution flow management.
 * Implements the design from docs/TD/执行流(面向未来设计).md:
 * - EFC = TYPE_THREAD capability slot, inlining thread context
 * - Capability tree IS the execution flow tree
 * - Core-0 provides mechanism only; scheduling policy delegated to Privileged-1
 *
 * Mechanism/Policy separation:
 * - Core-0:   create/destroy/dispatch/block/wake/get_state/yield
 * - P1 layer: decide WHEN to dispatch WHICH EFC (scheduling policy)
 */

#ifndef HIC_KERNEL_EXEC_FLOW_H
#define HIC_KERNEL_EXEC_FLOW_H

#include "types.h"
#include "capability.h"
#include "thread.h"

/* EFC identifier = capability ID of TYPE_THREAD */
typedef cap_id_t exec_flow_id_t;
#define EXEC_FLOW_INVALID ((exec_flow_id_t)-1)

/* EFC states (mirrors thread_state_t) */
typedef enum {
    EXEC_FLOW_READY       = 0,
    EXEC_FLOW_RUNNING     = 1,
    EXEC_FLOW_BLOCKED     = 2,
    EXEC_FLOW_WAITING     = 3,
    EXEC_FLOW_TERMINATED  = 4,
} exec_flow_state_t;

/*
 * ==================== Mechanism Primitives ====================
 */

/**
 * Create an execution flow capability.
 * Derives a TYPE_THREAD capability, allocates stack, initializes context.
 *
 * @param domain  owning domain
 * @param entry   entry point (virtual address)
 * @param out     output: EFC ID (capability ID)
 */
hic_status_t exec_flow_create(domain_id_t domain, virt_addr_t entry,
                              exec_flow_id_t *out);

/**
 * Destroy an execution flow.
 * Revokes the EFC capability, reclaims stack and context pages.
 */
hic_status_t exec_flow_destroy(exec_flow_id_t efc);

/**
 * Dispatch an EFC to a logical core.
 *
 * Core mechanism: saves current execution context, loads target EFC context.
 * This is a pure context-switch primitive — no scheduling decision.
 * The caller (Privileged-1 scheduler) decides WHICH EFC to dispatch WHEN.
 *
 * After dispatch, this function "returns" in the target EFC's context.
 * When an EFC is later dispatched back to the caller, this function
 * returns HIC_SUCCESS in the caller's context.
 *
 * @param efc    target execution flow to dispatch
 * @param lcore  target logical core
 */
hic_status_t exec_flow_dispatch(exec_flow_id_t efc, logical_core_id_t lcore);

/**
 * Block the current execution flow.
 * Marks it BLOCKED; returns so the upper-layer scheduler can dispatch another.
 */
hic_status_t exec_flow_block(exec_flow_id_t efc);

/**
 * Wake a blocked execution flow.
 * Marks it READY; the upper-layer scheduler decides when to dispatch.
 */
hic_status_t exec_flow_wake(exec_flow_id_t efc);

/**
 * Get execution flow state.
 */
hic_status_t exec_flow_get_state(exec_flow_id_t efc, exec_flow_state_t *state);

/**
 * Yield the current execution flow.
 * Marks it READY; returns so the caller can dispatch another.
 */
void exec_flow_yield(void);

/*
 * ==================== Global State ====================
 */

/* Currently running EFC on this core (EXEC_FLOW_INVALID if none) */
extern exec_flow_id_t g_current_efc;

#endif /* HIC_KERNEL_EXEC_FLOW_H */
