/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC Domain State Tracking
 *
 * IPC 3.0 replaces the old domain_switch IPC call path.
 * This module only tracks current domain and per-domain page tables.
 */

#include "domain_switch.h"
#include "pagetable.h"
#include "lib/mem.h"
#include "lib/console.h"

/* Current domain ID */
static domain_id_t g_current_domain = HIC_DOMAIN_CORE;

/* Per-domain page tables */
static page_table_t* g_domain_pagetables[HIC_DOMAIN_MAX];

void domain_switch_init(void)
{
    for (domain_id_t i = 0; i < HIC_DOMAIN_MAX; i++) {
        g_domain_pagetables[i] = NULL;
    }
    g_current_domain = HIC_DOMAIN_CORE;
    console_puts("[DOMAIN] Domain state tracking initialized\n");
}

domain_id_t domain_switch_get_current(void)
{
    return g_current_domain;
}

void domain_switch_set_current(domain_id_t domain)
{
    if (domain < HIC_DOMAIN_MAX) {
        g_current_domain = domain;
    }
}

page_table_t* domain_switch_get_pagetable(domain_id_t domain)
{
    if (domain >= HIC_DOMAIN_MAX) return NULL;
    return g_domain_pagetables[domain];
}

hic_status_t domain_switch_set_pagetable(domain_id_t domain, page_table_t *pagetable)
{
    if (domain >= HIC_DOMAIN_MAX) return HIC_ERROR_INVALID_PARAM;
    if (pagetable == NULL) return HIC_ERROR_INVALID_PARAM;

    g_domain_pagetables[domain] = pagetable;

    console_puts("[DOMAIN] Set pagetable for domain ");
    console_putu32(domain);
    console_puts(" = 0x");
    console_puthex64((u64)pagetable);
    console_puts("\n");

    return HIC_SUCCESS;
}
