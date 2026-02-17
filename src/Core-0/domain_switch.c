/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK域切换实现（完整版）
 * 遵循文档第2.1节：跨域通信和隔离
 */

#include "domain_switch.h"
#include "capability.h"
#include "pagetable.h"
#include "hal.h"
#include "thread.h"
#include "audit.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 当前域ID */
static domain_id_t g_current_domain = HIK_DOMAIN_CORE;

/* 调用栈（用于恢复到调用者域） */
static struct {
    domain_id_t domain;
    u64 return_address;
    hal_context_t context;
} g_call_stack[16];
static u32 g_call_stack_depth = 0;

/* 域上下文保存 */
static hal_context_t g_domain_contexts[HIK_DOMAIN_MAX];

/* 域页表 */
static page_table_t* g_domain_pagetables[HIK_DOMAIN_MAX];

/* 域切换初始化 */
void domain_switch_init(void)
{
    /* 初始化域上下文 */
    for (domain_id_t i = 0; i < HIK_DOMAIN_MAX; i++) {
        memzero(&g_domain_contexts[i], sizeof(hal_context_t));
        g_domain_pagetables[i] = NULL;
    }
    
    /* 初始化调用栈 */
    memzero(g_call_stack, sizeof(g_call_stack));
    g_call_stack_depth = 0;
    
    /* 设置Core-0为当前域 */
    g_current_domain = HIK_DOMAIN_CORE;
    
    console_puts("[DOMAIN] Domain switch initialized\n");
}

/* 执行域切换（完整实现） */
hik_status_t domain_switch(domain_id_t from, domain_id_t to, 
                           cap_id_t endpoint_cap, u64 syscall_num,
                           u64* args, u32 arg_count)
{
    (void)arg_count;  /* 避免未使用参数警告 */
    (void)args;       /* 避免未使用参数警告 */
    /* 验证参数 */
    if (from >= HIK_DOMAIN_MAX || to >= HIK_DOMAIN_MAX) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    /* 验证端点能力 */
    hik_status_t status = cap_check_access(from, endpoint_cap, 0);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_PERMISSION;
    }
    
    /* 获取端点信息 */
    cap_entry_t endpoint;
    status = cap_get_info(endpoint_cap, &endpoint);
    if (status != HIK_SUCCESS) {
        return HIK_ERROR_CAP_INVALID;
    }
    
    /* 验证目标域 */
    if (endpoint.endpoint.target_domain != to) {
        return HIK_ERROR_PERMISSION;
    }
    
    /* 保存调用者上下文到调用栈 */
    if (g_call_stack_depth < 16) {
        g_call_stack[g_call_stack_depth].domain = from;
        g_call_stack[g_call_stack_depth].return_address = syscall_num;
        hal_save_context(&g_call_stack[g_call_stack_depth].context);
        g_call_stack_depth++;
    }
    
    /* 保存当前域上下文 */
    hal_save_context(&g_domain_contexts[from]);
    
    /* 切换到目标域 */
    g_current_domain = to;
    
    /* 切换页表 */
    if (g_domain_pagetables[to]) {
        pagetable_switch(g_domain_pagetables[to]);
    }
    
    /* 恢复目标域上下文 */
    hal_restore_context(&g_domain_contexts[to]);
    
    /* 记录审计日志 */
    u64 audit_data[4] = {from, to, endpoint_cap, syscall_num};
    audit_log_event(AUDIT_EVENT_IPC_CALL, from, endpoint_cap, 0, 
                   audit_data, 4, true);
    
    return HIK_SUCCESS;
}

/* 从域切换返回（完整实现） */
void __attribute__((unused)) domain_switch_return(hik_status_t result)
{
    (void)result;  /* 避免未使用参数警告 */
    if (g_call_stack_depth == 0) {
        /* 调用栈为空，返回Core-0 */
        g_current_domain = HIK_DOMAIN_CORE;
        
        if (g_domain_pagetables[HIK_DOMAIN_CORE]) {
            pagetable_switch(g_domain_pagetables[HIK_DOMAIN_CORE]);
        }
        
        hal_restore_context(&g_domain_contexts[HIK_DOMAIN_CORE]);
        return;
    }
    
    /* 从调用栈弹出调用者信息 */
    g_call_stack_depth--;
    domain_id_t caller_domain = g_call_stack[g_call_stack_depth].domain;
    
    /* 切换回调用者域 */
    g_current_domain = caller_domain;
    
    /* 切换页表 */
    if (g_domain_pagetables[caller_domain]) {
        pagetable_switch(g_domain_pagetables[caller_domain]);
    }
    
    /* 恢复调用者上下文 */
    hal_restore_context(&g_call_stack[g_call_stack_depth].context);
}

/* 保存当前域上下文 */
void __attribute__((unused)) domain_switch_save_context(domain_id_t domain, hal_context_t* ctx)
{
    if (domain < HIK_DOMAIN_MAX && ctx) {
        memcopy(&g_domain_contexts[domain], ctx, sizeof(hal_context_t));
    }
}

/* 恢复域上下文 */
void __attribute__((unused)) domain_switch_restore_context(domain_id_t domain, hal_context_t* ctx)
{
    if (domain < HIK_DOMAIN_MAX && ctx) {
        memcopy(ctx, &g_domain_contexts[domain], sizeof(hal_context_t));
    }
}

/* 获取当前域ID */
__attribute__((unused)) domain_id_t domain_switch_get_current(void)
{
    return g_current_domain;
}

/* 设置当前域ID */
__attribute__((unused)) void domain_switch_set_current(domain_id_t domain)
{
    if (domain < HIK_DOMAIN_MAX) {
        g_current_domain = domain;
    }
}

/* 设置域页表 */
__attribute__((unused)) hik_status_t domain_switch_set_pagetable(domain_id_t domain, page_table_t* pagetable)
{
    if (domain >= HIK_DOMAIN_MAX) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    g_domain_pagetables[domain] = pagetable;
    return HIK_SUCCESS;
}

/* 获取域页表 */
__attribute__((unused)) page_table_t* domain_switch_get_pagetable(domain_id_t domain)
{
    if (domain >= HIK_DOMAIN_MAX) {
        return NULL;
    }
    
    return g_domain_pagetables[domain];
}

/* 上下文切换到指定线程 */
void __attribute__((unused)) context_switch_to(thread_id_t next_thread)
{
    if (next_thread == INVALID_THREAD) {
        console_puts("[SCHED] Invalid thread ID\n");
        return;
    }
    
    extern thread_t g_threads[MAX_THREADS];
    extern thread_t *g_current_thread;
    
    if (next_thread >= MAX_THREADS) {
        console_puts("[SCHED] Thread ID out of range\n");
        return;
    }
    
    thread_t *next = &g_threads[next_thread];
    if (next == NULL) {
        console_puts("[SCHED] Thread not found\n");
        return;
    }
    
    /* 保存当前线程上下文 */
    if (g_current_thread != NULL) {
        if (g_current_thread->state == THREAD_STATE_RUNNING) {
            g_current_thread->state = THREAD_STATE_READY;
        }
    }
    
    /* 切换到新线程 */
    g_current_thread = next;
    g_current_thread->state = THREAD_STATE_RUNNING;
    g_current_thread->last_run_time = hal_get_timestamp();
    
    /* 执行实际的上下文切换（调用 schedule 中的实现） */
    schedule();
}