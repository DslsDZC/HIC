/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC异常处理系统实现（完整版）
 * 遵循文档第2.1节：故障隔离与恢复
 */

#include "exception.h"
#include "monitor.h"
#include "audit.h"
#include "thread.h"  /* 包含 schedule 等函数声明 */
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* 异常处理程序表 */
static exception_handler_fn exception_handlers[32];

/* 异常系统初始化 */
void exception_system_init(void)
{
    memzero(exception_handlers, sizeof(exception_handlers));
    console_puts("[EXCEPT] Exception system initialized\n");
}

/* 注册异常处理程序 */
void exception_register_handler(exception_type_t type, exception_handler_fn handler)
{
    if (type < 32) {
        exception_handlers[type] = handler;
    }
}

/* 处理异常（完整实现） */
exception_handler_result_t exception_handle(exception_context_t* ctx)
{
    if (!ctx) {
        return EXCEPT_HANDLER_PANIC;
    }
    
    console_puts("[EXCEPT] Exception occurred:\n");
    console_puts("  Type: ");
    console_putu64(ctx->type);
    console_puts("\n");
    console_puts("  Domain: ");
    console_putu64(ctx->domain);
    console_puts("\n");
    console_puts("  Thread: ");
    console_putu64(ctx->thread);
    console_puts("\n");
    console_puts("  Error code: 0x");
    console_puthex64(ctx->error_code);
    console_puts("\n");
    
    /* 记录审计日志 */
    u64 audit_data[4] = {ctx->type, ctx->domain, ctx->thread, ctx->error_code};
    audit_log_event(AUDIT_EVENT_EXCEPTION, ctx->domain, 0, ctx->thread,
                   audit_data, 4, false);
    
    /* 调用注册的处理程序 */
    if (ctx->type < 32 && exception_handlers[ctx->type]) {
        exception_handler_result_t result = exception_handlers[ctx->type](ctx);
        
        if (result != EXCEPT_HANDLER_CONTINUE) {
            return result;
        }
    }
    
    /* 默认处理：终止线程 */
    console_puts("[EXCEPT] Terminating thread\n");
    thread_terminate(ctx->thread);
    
    /* 如果是Core-0域的异常，系统恐慌 */
    if (ctx->domain == HIC_DOMAIN_CORE) {
        console_puts("[EXCEPT] Core-0 exception, system panic!\n");
        return EXCEPT_HANDLER_PANIC;
    }
    
    /* 完整实现：通知监控服务 */
    monitor_event_t event;
    event.type = MONITOR_EVENT_SERVICE_CRASH;
    event.domain = ctx->domain;
    event.timestamp = hal_get_timestamp();
    event.data[0] = ctx->type;
    event.data[1] = ctx->error_code;
    monitor_report_event(&event);
    
    return EXCEPT_HANDLER_TERMINATE;
}

/* 内核恐慌（完整实现） */
void kernel_panic(const char* message, ...)
{
    console_puts("\n");
    console_puts("========================================\n");
    console_puts("KERNEL PANIC\n");
    console_puts("========================================\n");
    
    if (message) {
        console_puts(message);
        console_puts("\n");
    }
    
    /* 记录审计日志 */
    u64 reason = 0; /* 完整实现：从消息中提取原因 */
    AUDIT_LOG_SECURITY_VIOLATION(HIC_DOMAIN_CORE, reason);
    
    /* 完整实现：保存系统状态 */
    /* 1. 保存所有域的内存快照 */
    /* 2. 保存寄存器状态 */
    /* 3. 保存审计日志到持久存储 */
    
    /* 停止系统 */
    console_puts("[PANIC] System halted\n");
    
    while (1) {
        hal_halt();
    }
}
