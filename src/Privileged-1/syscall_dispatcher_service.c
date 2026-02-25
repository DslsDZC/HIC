/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 系统调用分发器服务 (Privileged-1)
 * 提供高级系统调用分发 API
 */

#include "../Core-0/types.h"
#include "../Core-0/syscall.h"
#include "../Core-0/audit.h"
#include "../Core-0/lib/console.h"
#include "../Core-0/lib/mem.h"

#define SYSCALL_ENDPOINT_IPC        0x5000
#define SYSCALL_ENDPOINT_CAP        0x5001
#define SYSCALL_ENDPOINT_DOMAIN     0x5002
#define SYSCALL_ENDPOINT_THREAD     0x5003

typedef struct syscall_service_state {
    u64 ipc_count;
    u64 cap_count;
    u64 domain_count;
    u64 thread_count;
} syscall_service_state_t;

static syscall_service_state_t g_syscall_state;

hic_status_t syscall_dispatcher_service_init(void)
{
    console_puts("[SYSCALL-DISP-SVC] Initializing syscall dispatcher service...\n");
    memzero(&g_syscall_state, sizeof(g_syscall_state));
    console_puts("[SYSCALL-DISP-SVC] >>> Syscall Dispatcher Service READY <<<\n");
    return HIC_SUCCESS;
}

hic_status_t syscall_dispatcher_service_start(void)
{
    console_puts("[SYSCALL-DISP-SVC] Starting syscall dispatcher service...\n");
    console_puts("[SYSCALL-DISP-SVC] Endpoints: ipc(0x5000), cap(0x5001), domain(0x5002), thread(0x5003)\n");
    console_puts("[SYSCALL-DISP-SVC] >>> Syscall Dispatcher Service STARTED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t syscall_dispatcher_service_stop(void)
{
    console_puts("[SYSCALL-DISP-SVC] Stopping syscall dispatcher service...\n");
    console_puts("[SYSCALL-DISP-SVC] >>> Syscall Dispatcher Service STOPPED <<<\n");
    return HIC_SUCCESS;
}

hic_status_t syscall_dispatcher_service_cleanup(void)
{
    console_puts("[SYSCALL-DISP-SVC] Cleaning up syscall dispatcher service...\n");
    console_puts("[SYSCALL-DISP-SVC] Statistics:\n");
    console_puts("[SYSCALL-DISP-SVC]   IPC: ");
    console_putu64(g_syscall_state.ipc_count);
    console_puts("\n");
    console_puts("[SYSCALL-DISP-SVC]   Cap: ");
    console_putu64(g_syscall_state.cap_count);
    console_puts("\n");
    console_puts("[SYSCALL-DISP-SVC] >>> Syscall Dispatcher Service CLEANED UP <<<\n");
    return HIC_SUCCESS;
}
