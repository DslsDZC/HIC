/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 中断控制器服务
 * 提供高级中断管理 API
 */

#include "../Core-0/types.h"
#include "../Core-0/irq.h"
#include "../Core-0/audit.h"
#include "../Core-0/lib/console.h"
#include "../Core-0/lib/mem.h"

#define IRQ_ENDPOINT_REGISTER  0x4000
#define IRQ_ENDPOINT_UNREGISTER 0x4001
#define IRQ_ENDPOINT_ENABLE    0x4002
#define IRQ_ENDPOINT_DISABLE   0x4003

typedef struct irq_service_state {
    u64 register_count;
    u64 unregister_count;
    u64 enable_count;
    u64 disable_count;
} irq_service_state_t;

static irq_service_state_t g_irq_state;

hic_status_t irq_controller_service_init(void)
{
    console_puts("[IRQ-CTRL-SVC] Initializing IRQ controller service...\n");
    memzero(&g_irq_state, sizeof(g_irq_state));
    console_puts("[IRQ-CTRL-SVC] >>> IRQ Controller Service READY <<<\n");
    return HIC_SUCCESS;
}

hic_status_t irq_controller_service_start(void)
{
    console_puts("[IRQ-CTRL-SVC] Starting IRQ controller service...\n");
    console_puts("[IRQ-CTRL-SVC] Endpoints: register(0x4000), unregister(0x4001), enable(0x4002), disable(0x4003)\n");
    console_puts("[IRQ-CTRL-SVC] >>> IRQ Controller Service STARTED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t irq_controller_service_stop(void)
{
    console_puts("[IRQ-CTRL-SVC] Stopping IRQ controller service...\n");
    console_puts("[IRQ-CTRL-SVC] >>> IRQ Controller Service STOPPED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t irq_controller_service_cleanup(void)
{
    console_puts("[IRQ-CTRL-SVC] Cleaning up IRQ controller service...\n");
    console_puts("[IRQ-CTRL-SVC] Statistics:\n");
    console_puts("[IRQ-CTRL-SVC]   Register: ");
    console_putu64(g_irq_state.register_count);
    console_puts("\n");
    console_puts("[IRQ-CTRL-SVC]   Unregister: ");
    console_putu64(g_irq_state.unregister_count);
    console_puts("\n");
    console_puts("[IRQ-CTRL-SVC] >>> IRQ Controller Service CLEANED UP <<<\n");
    return HIC_SUCCESS;
}