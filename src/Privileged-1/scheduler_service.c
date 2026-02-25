/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 调度器服务
 * 提供高级线程调度 API
 */

#include "../Core-0/types.h"
#include "../Core-0/thread.h"
#include "../Core-0/audit.h"
#include "privileged_service.h"
#include "../Core-0/lib/console.h"
#include "../Core-0/lib/mem.h"

#define SCHED_ENDPOINT_CREATE    0x2000
#define SCHED_ENDPOINT_TERMINATE 0x2001
#define SCHED_ENDPOINT_YIELD     0x2002

typedef struct sched_message {
    u32    type;
    u32    domain_id;
    u32    thread_id;
    u32    priority;
    u32    entry_point;
} sched_message_t;

typedef struct sched_service_state {
    u64 create_count;
    u64 terminate_count;
    u64 yield_count;
} sched_service_state_t;

static sched_service_state_t g_sched_state;

static hic_status_t sched_create_handler(sched_message_t *msg)
{
    if (!msg) return HIC_ERROR_INVALID_PARAM;
    g_sched_state.create_count++;
    /* 调用 Core-0 的线程创建（如果有实现） */
    return HIC_SUCCESS;
}

static hic_status_t sched_terminate_handler(sched_message_t *msg)
{
    if (!msg) return HIC_ERROR_INVALID_PARAM;
    g_sched_state.terminate_count++;
    /* 调用 Core-0 的线程终止 */
    return HIC_SUCCESS;
}

static hic_status_t sched_yield_handler(sched_message_t *msg)
{
    if (!msg) return HIC_ERROR_INVALID_PARAM;
    g_sched_state.yield_count++;
    thread_yield();
    return HIC_SUCCESS;
}

static hic_status_t scheduler_endpoint_handler(u32 endpoint_id, void *message, void *response __attribute__((unused)))
{
    sched_message_t *msg = (sched_message_t*)message;
    if (!msg) return HIC_ERROR_INVALID_PARAM;
    
    switch (endpoint_id) {
        case SCHED_ENDPOINT_CREATE: return sched_create_handler(msg);
        case SCHED_ENDPOINT_TERMINATE: return sched_terminate_handler(msg);
        case SCHED_ENDPOINT_YIELD: return sched_yield_handler(msg);
        default: return HIC_ERROR_INVALID_PARAM;
    }
}

hic_status_t scheduler_service_init(void)
{
    console_puts("[SCHED-SVC] Initializing scheduler service...\n");
    memzero(&g_sched_state, sizeof(g_sched_state));
    console_puts("[SCHED-SVC] >>> Scheduler Service READY <<<\n");
    return HIC_SUCCESS;
}

hic_status_t scheduler_service_start(void)
{
    console_puts("[SCHED-SVC] Starting scheduler service...\n");
    console_puts("[SCHED-SVC] Endpoints: create(0x2000), terminate(0x2001), yield(0x2002)\n");
    console_puts("[SCHED-SVC] >>> Scheduler Service STARTED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t scheduler_service_stop(void)
{
    console_puts("[SCHED-SVC] Stopping scheduler service...\n");
    console_puts("[SCHED-SVC] >>> Scheduler Service STOPPED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t scheduler_service_cleanup(void)
{
    console_puts("[SCHED-SVC] Cleaning up scheduler service...\n");
    console_puts("[SCHED-SVC] Statistics:\n");
    console_puts("[SCHED-SVC]   Create: ");
    console_putu64(g_sched_state.create_count);
    console_puts("\n");
    console_puts("[SCHED-SVC]   Terminate: ");
    console_putu64(g_sched_state.terminate_count);
    console_puts("\n");
    console_puts("[SCHED-SVC]   Yield: ");
    console_putu64(g_sched_state.yield_count);
    console_puts("\n");
    console_puts("[SCHED-SVC] >>> Scheduler Service CLEANED UP <<<\n");
    return HIC_SUCCESS;
}