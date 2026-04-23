/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Domain State Tracking
 *
 * IPC 3.0 replaces the old domain_switch IPC call path.
 * This module now only tracks:
 *   - Current domain ID
 *   - Per-domain page table pointers
 *
 * Cross-domain calls go through IPC 3.0 (entry page → bt → jmp → #PF → service).
 */

#ifndef HIC_KERNEL_DOMAIN_SWITCH_H
#define HIC_KERNEL_DOMAIN_SWITCH_H

#include "types.h"
#include "domain.h"
#include "pagetable.h"

/* Initialize domain state tracking */
void domain_switch_init(void);

/* Get/set current domain ID */
domain_id_t domain_switch_get_current(void);
void domain_switch_set_current(domain_id_t domain);

/* Get/set per-domain page table */
page_table_t* domain_switch_get_pagetable(domain_id_t domain);
hic_status_t domain_switch_set_pagetable(domain_id_t domain, page_table_t *pagetable);

/* Domain-level pagetable switch (used by context.S assembly) */
void pagetable_switch(page_table_t *root);

#endif /* HIC_KERNEL_DOMAIN_SWITCH_H */
