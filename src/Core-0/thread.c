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

/* 创建线程 */
hic_status_t thread_create(domain_id_t domain_id, virt_addr_t entry_point,
                          priority_t priority, thread_id_t *out)
{
    if (out == NULL || domain_id >= HIC_DOMAIN_MAX) {
        return HIC_ERROR_INVALID_PARAM;
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
    
    atomic_exit_critical(irq);
    
    /* 将线程加入调度队列 */
    thread_ready(free_slot);
    
    *out = free_slot;
    return HIC_SUCCESS;
}