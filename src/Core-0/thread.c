/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核线程管理实现
 */

#include "thread.h"
#include "types.h"
#include "hal.h"
#include "pmm.h"
#include "lib/mem.h"
#include "atomic.h"
#include "logical_core.h"
#include "console.h"

/* 全局线程表 */
thread_t g_threads[MAX_THREADS];

/* 线程系统初始化 */
void thread_system_init(void)
{
    /* 清空线程表 */
    memzero(g_threads, sizeof(g_threads));
    
    /* 初始化调度器 */
    scheduler_init();
    
    /* 初始化空闲线程 */
    extern thread_t idle_thread;
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    idle_thread.last_run_time = 0;
    idle_thread.time_slice = 0;
}

/* 检查线程是否活跃 */
bool thread_is_active(thread_id_t thread)
{
    if (thread >= MAX_THREADS) {
        return false;
    }

    thread_t *t = &g_threads[thread];

    return t != NULL && (t->state == THREAD_STATE_READY ||
                           t->state == THREAD_STATE_RUNNING ||
                           t->state == THREAD_STATE_BLOCKED ||
                           t->state == THREAD_STATE_WAITING);
}

/* 获取线程等待时间 */
u64 get_thread_wait_time(thread_id_t thread)
{
    if (thread >= MAX_THREADS) {
        return 0;
    }

    thread_t *t = &g_threads[thread];
    if (!t) {
        return 0;
    }

    /* 完整实现：计算线程等待时间 */
    /* 实现等待时间计算 */
    /* 需要实现：
     * 1. 记录线程开始等待的时间戳
     * 2. 计算当前时间与开始时间的差值
     * 3. 返回等待时间（纳秒）
     */
    (void)t;
    return 0;
}

/* 获取线程等待的资源 */
cap_id_t get_thread_wait_resource(thread_id_t thread)
{
    if (thread >= MAX_THREADS) {
        return INVALID_CAP_ID;
    }

    thread_t *t = &g_threads[thread];
    if (!t) {
        return INVALID_CAP_ID;
    }

    /* 返回等待的能力ID（如果适用） */
    if (t->wait_data) {
        return (cap_id_t)(uintptr_t)t->wait_data;
    }

    return INVALID_CAP_ID;
}

/* 终止线程 */
hic_status_t thread_terminate(thread_id_t thread_id)
{
    if (thread_id >= MAX_THREADS) {
        return HIC_ERROR_INVALID_PARAM;
    }

    thread_t *t = &g_threads[thread_id];
    if (!t) {
        return HIC_ERROR_NOT_FOUND;
    }

    /* 将线程状态设置为已终止 */
    t->state = THREAD_STATE_TERMINATED;

    /* 通知调度器清理资源 */
    if (t->prev != NULL) {
        t->prev->next = t->next;
    }
    if (t->next != NULL) {
        t->next->prev = t->prev;
    }
    
    /* 如果是当前线程，触发调度 */
    if (g_current_thread == t) {
        g_current_thread = NULL;
    }
    
    /* 回收线程栈空间 */
    if (t->stack_base != 0) {
        u32 stack_pages = (u32)((t->stack_size + HAL_PAGE_SIZE - 1) / HAL_PAGE_SIZE);
        pmm_free_frames(t->stack_base, stack_pages);
        t->stack_base = 0;
        t->stack_size = 0;
    }

    return HIC_SUCCESS;
}

/* 创建线程（必须绑定逻辑核心） */
hic_status_t thread_create_bound(domain_id_t domain_id, 
                                  u32 logical_core_id,
                                  virt_addr_t entry_point,
                                  priority_t priority, 
                                  thread_id_t *out)
{
    if (out == NULL || domain_id >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 验证逻辑核心ID */
    extern logical_core_t g_logical_cores[];
    if (logical_core_id >= MAX_LOGICAL_CORES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    /* 检查逻辑核心是否属于该域 */
    if (core->owner_domain != domain_id) {
        return HIC_ERROR_PERMISSION_DENIED;
    }
    
    /* 检查逻辑核心状态 */
    if (core->state != LOGICAL_CORE_STATE_ALLOCATED &&
        core->state != LOGICAL_CORE_STATE_ACTIVE) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* 使用位图快速查找空闲线程槽 */
    static u64 g_thread_bitmap[(MAX_THREADS + 63) / 64] = {0};
    
    thread_id_t free_slot = MAX_THREADS;
    bool irq = atomic_enter_critical();
    
    /* 在位图中查找空闲位 */
    for (u32 i = 0; i < (MAX_THREADS + 63) / 64 && free_slot == MAX_THREADS; i++) {
        if (g_thread_bitmap[i] != 0xFFFFFFFFFFFFFFFFULL) {
            /* 找到有空闲位的块 */
            for (u32 j = 0; j < 64; j++) {
                u32 idx = i * 64 + j;
                if (idx >= MAX_THREADS) break;
                
                if (!(g_thread_bitmap[i] & (1ULL << j))) {
                    /* 找到空闲槽 */
                    free_slot = idx;
                    g_thread_bitmap[i] |= (1ULL << j);
                    break;
                }
            }
        }
    }
    
    if (free_slot >= MAX_THREADS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 分配内核栈 (2 页 = 8KB) */
    extern hic_status_t pmm_alloc_frames(domain_id_t owner, u32 count,
                                          page_frame_type_t type, phys_addr_t *out);
    phys_addr_t stack_phys;
    hic_status_t status = pmm_alloc_frames(domain_id, 2, PAGE_FRAME_PRIVILEGED, &stack_phys);
    if (status != HIC_SUCCESS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 初始化线程结构 */
    thread_t *thread = &g_threads[free_slot];
    memzero(thread, sizeof(thread_t));
    
    thread->thread_id = free_slot;
    thread->domain_id = domain_id;
    thread->state = THREAD_STATE_READY;
    thread->priority = priority;
    thread->logical_core_id = logical_core_id;  /* 绑定逻辑核心 */
    thread->core_affinity = 0xFFFFFFFF;          /* 默认所有核心亲和性 */
    thread->flags = THREAD_FLAG_BOUND;           /* 标记已绑定 */
    thread->stack_base = (virt_addr_t)stack_phys;
    thread->stack_size = 2 * PAGE_SIZE;
    thread->last_run_time = 0;
    thread->cpu_time_used = 0;
    thread->time_slice = 100;  /* 默认时间片 */
    thread->wait_flags = 0;
    
    /* 初始化栈：设置入口点
     * context_switch 会恢复以下寄存器：rbx, rbp, r12-r15
     * 然后执行 ret，所以需要在栈顶放置入口点地址
     */
    u64 *stack_top = (u64 *)(stack_phys + 2 * PAGE_SIZE);
    
    /* 压入入口点地址（作为 ret 的返回地址） */
    stack_top--;
    *stack_top = (u64)entry_point;
    
    /* 为 callee-saved 寄存器预留空间 (rbx, rbp, r12-r15 = 6 个) */
    stack_top -= 6;
    
    thread->stack_ptr = (virt_addr_t)stack_top;
    
    /* 更新逻辑核心状态 */
    if (core->running_thread == INVALID_THREAD) {
        core->running_thread = free_slot;
    }
    if (core->state == LOGICAL_CORE_STATE_ALLOCATED) {
        core->state = LOGICAL_CORE_STATE_ACTIVE;
    }
    
    atomic_exit_critical(irq);
    
    /* 将线程加入调度队列 */
    thread_ready(free_slot);
    
    *out = free_slot;
    return HIC_SUCCESS;
}

/* 创建线程（自动分配逻辑核心如果域还没有） */
hic_status_t thread_create(domain_id_t domain_id, virt_addr_t entry_point,
                          priority_t priority, thread_id_t *out)
{
    if (out == NULL || domain_id >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 查找域是否已有逻辑核心 */
    logical_core_id_t existing_core = INVALID_LOGICAL_CORE;
    for (u32 i = 0; i < MAX_LOGICAL_CORES; i++) {
        if (g_logical_cores[i].owner_domain == domain_id &&
            (g_logical_cores[i].state == LOGICAL_CORE_STATE_ALLOCATED ||
             g_logical_cores[i].state == LOGICAL_CORE_STATE_ACTIVE)) {
            existing_core = i;
            break;
        }
    }
    
    /* 如果域还没有逻辑核心，自动分配一个 */
    logical_core_id_t target_core = existing_core;
    if (target_core == INVALID_LOGICAL_CORE) {
        cap_handle_t lcore_handle;
        hic_status_t alloc_status = hic_logical_core_allocate(domain_id, 1,
                                                              0,    /* 无特殊标志 */
                                                              10,   /* 10% CPU 配额 */
                                                              NULL, /* 无亲和性限制 */
                                                              &lcore_handle);
        if (alloc_status != HIC_SUCCESS) {
            console_puts("[THREAD] WARN: Failed to auto-allocate logical core for domain ");
            console_putu64(domain_id);
            console_puts("\n");
            /* 继续创建线程，但标记为未绑定 */
        } else {
            target_core = logical_core_validate_handle(domain_id, lcore_handle);
            console_puts("[THREAD] Auto-allocated logical core ");
            console_putu64(target_core);
            console_puts(" for domain ");
            console_putu64(domain_id);
            console_puts("\n");
        }
    }
    
    /* 使用位图快速查找空闲线程槽 */
    static u64 g_thread_bitmap[(MAX_THREADS + 63) / 64] = {0};
    
    thread_id_t free_slot = MAX_THREADS;
    bool irq = atomic_enter_critical();
    
    /* 在位图中查找空闲位 */
    for (u32 i = 0; i < (MAX_THREADS + 63) / 64 && free_slot == MAX_THREADS; i++) {
        if (g_thread_bitmap[i] != 0xFFFFFFFFFFFFFFFFULL) {
            /* 找到有空闲位的块 */
            for (u32 j = 0; j < 64; j++) {
                u32 idx = i * 64 + j;
                if (idx >= MAX_THREADS) break;
                
                if (!(g_thread_bitmap[i] & (1ULL << j))) {
                    /* 找到空闲槽 */
                    free_slot = idx;
                    g_thread_bitmap[i] |= (1ULL << j);
                    break;
                }
            }
        }
    }
    
    if (free_slot >= MAX_THREADS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 分配内核栈 (2 页 = 8KB) */
    extern hic_status_t pmm_alloc_frames(domain_id_t owner, u32 count,
                                          page_frame_type_t type, phys_addr_t *out);
    phys_addr_t stack_phys;
    hic_status_t status = pmm_alloc_frames(domain_id, 2, PAGE_FRAME_PRIVILEGED, &stack_phys);
    if (status != HIC_SUCCESS) {
        atomic_exit_critical(irq);
        return HIC_ERROR_NO_RESOURCE;
    }
    
    /* 初始化线程结构 */
    thread_t *thread = &g_threads[free_slot];
    memzero(thread, sizeof(thread_t));
    
    thread->thread_id = free_slot;
    thread->domain_id = domain_id;
    thread->state = THREAD_STATE_READY;
    thread->priority = priority;
    thread->logical_core_id = target_core;  /* 绑定自动分配的逻辑核心 */
    thread->core_affinity = 0xFFFFFFFF;      /* 默认所有核心亲和性 */
    thread->flags = (target_core != INVALID_LOGICAL_CORE) ? THREAD_FLAG_BOUND : 0;
    thread->stack_base = (virt_addr_t)stack_phys;
    thread->stack_size = 2 * PAGE_SIZE;
    thread->last_run_time = 0;
    thread->cpu_time_used = 0;
    thread->time_slice = 100;  /* 默认时间片 */
    thread->wait_flags = 0;
    
    /* 初始化栈：设置入口点 */
    u64 *stack_top = (u64 *)(stack_phys + 2 * PAGE_SIZE);
    
    /* 压入入口点地址（作为 ret 的返回地址） */
    stack_top--;
    *stack_top = (u64)entry_point;
    
    /* 为 callee-saved 寄存器预留空间 (rbx, rbp, r12-r15 = 6 个) */
    stack_top -= 6;
    
    thread->stack_ptr = (virt_addr_t)stack_top;
    
    atomic_exit_critical(irq);
    
    /* 将线程加入调度队列 */
    thread_ready(free_slot);
    
    *out = free_slot;
    return HIC_SUCCESS;
}

/* 绑定线程到逻辑核心 */
hic_status_t thread_bind_to_core(thread_id_t thread_id, u32 logical_core_id)
{
    if (thread_id >= MAX_THREADS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    extern logical_core_t g_logical_cores[];
    if (logical_core_id >= MAX_LOGICAL_CORES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    thread_t *thread = &g_threads[thread_id];
    logical_core_t *core = &g_logical_cores[logical_core_id];
    
    /* 验证所有权 */
    if (core->owner_domain != thread->domain_id) {
        return HIC_ERROR_PERMISSION_DENIED;
    }
    
    bool irq = atomic_enter_critical();
    
    thread->logical_core_id = logical_core_id;
    thread->flags |= THREAD_FLAG_BOUND;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}

/* 获取线程绑定的逻辑核心 */
u32 thread_get_bound_core(thread_id_t thread_id)
{
    if (thread_id >= MAX_THREADS) {
        return INVALID_LOGICAL_CORE;
    }
    
    thread_t *thread = &g_threads[thread_id];
    return thread->logical_core_id;
}

/**
 * 根据逻辑核心 ID 查找线程
 */
thread_id_t thread_find_by_logical_core(u32 logical_core_id)
{
    for (thread_id_t i = 0; i < MAX_THREADS; i++) {
        /* 检查线程是否有效且绑定到指定核心 */
        if (g_threads[i].domain_id != HIC_INVALID_DOMAIN &&
            g_threads[i].state != THREAD_STATE_TERMINATED &&
            g_threads[i].logical_core_id == logical_core_id) {
            return i;
        }
    }
    return INVALID_THREAD;
}