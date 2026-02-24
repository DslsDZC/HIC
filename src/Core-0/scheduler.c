/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC调度器实现（无锁设计）
 * 遵循三层模型文档第2.1节：执行控制与调度
 * 
 * 无锁设计原则：
 * 1. 使用禁用中断保证调度操作的原子性
 * 2. 就绪队列在临界区内修改
 * 3. 当前线程指针只在中断上下文中修改
 */

#include "thread.h"
#include "types.h"
#include "formal_verification.h"
#include "atomic.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 当前运行的线程（原子操作保护） */
thread_t *g_current_thread = NULL;

/* 外部引用的线程表（定义在thread.c中） */
extern thread_t g_threads[MAX_THREADS];

/* 就绪队列（无锁设计 - 使用数组+索引） */
#define MAX_READY_THREADS 64
typedef struct {
    thread_id_t threads[MAX_READY_THREADS];
    volatile u32 head;
    volatile u32 tail;
    volatile u32 count;
} ready_queue_t;

static ready_queue_t ready_queues[5];  /* 5个优先级 */

/* 空闲线程 */
thread_t idle_thread;

/* 初始化调度器 */
void scheduler_init(void)
{
    console_puts("[SCHED] Initializing scheduler...\n");
    
    for (int i = 0; i < 5; i++) {
        ready_queues[i].head = 0;
        ready_queues[i].tail = 0;
        ready_queues[i].count = 0;
        memzero((void*)ready_queues[i].threads, sizeof(ready_queues[i].threads));
    }
    
    console_puts("[SCHED] Ready queues cleared (5 priority levels)\n");
    
    g_current_thread = NULL;
    console_puts("[SCHED] Current thread pointer reset\n");
    
    /* 初始化空闲线程 */
    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    
    console_puts("[SCHED] Idle thread initialized (ID: 0xFFFFFFFF)\n");
    console_puts("[SCHED] Scheduler initialized (lock-free)\n");
    console_puts("[SCHED] Ready for thread scheduling\n");
}

/* 添加线程到就绪队列（无锁实现） */
static void enqueue_thread(thread_t *thread)
{
    if (thread == NULL || thread->priority > 4) {
        return;
    }
    
    ready_queue_t *queue = &ready_queues[thread->priority];
    
    /* 检查队列是否已满 */
    if (queue->count >= MAX_READY_THREADS) {
        console_puts("[SCHED] ERROR: Ready queue full for priority ");
        console_putu64(thread->priority);
        console_puts("\n");
        return;
    }
    
    /* 添加到队列尾部 */
    u32 tail = queue->tail;
    queue->threads[tail] = thread->thread_id;
    queue->tail = (tail + 1) % MAX_READY_THREADS;
    queue->count++;
    
    thread->state = THREAD_STATE_READY;
}

/* 从就绪队列移除线程 */
__attribute__((unused)) static void dequeue_thread(thread_t *thread)
{
    if (thread == NULL) {
        return;
    }
    
    ready_queue_t *queue = &ready_queues[thread->priority];
    
    if (queue->head == queue->tail) {
        /* 队列为空 */
        return;
    }
    
    queue->head = (queue->head + 1) % MAX_READY_THREADS;
    queue->count--;
}

/* 选择下一个线程（无锁实现） */
static thread_t *pick_next_thread(void)
{
    /* 从高优先级到低优先级查找 */
    for (int prio = 4; prio >= 0; prio--) {
        ready_queue_t *queue = &ready_queues[prio];
        
        if (queue->count > 0) {
            /* 取出队列头部 */
            u32 head = queue->head;
            thread_id_t tid = queue->threads[head];
            queue->head = (head + 1) % MAX_READY_THREADS;
            queue->count--;
            
            /* 获取线程指针 */
            thread_t *thread = &g_threads[tid];
            
            /* 轮转调度：如果队列中还有线程，将其重新加入尾部 */
            if (queue->count > 0) {
                u32 tail = queue->tail;
                queue->threads[tail] = tid;
                queue->tail = (tail + 1) % MAX_READY_THREADS;
                queue->count++;
            }
            
            return thread;
        }
    }
    
    /* 没有就绪线程，返回空闲线程 */
    return &idle_thread;
}

/* 上下文切换（架构无关） */
extern void context_switch(thread_t *prev, thread_t *next);

/* 主调度函数（无锁实现） */
void schedule(void)
{
    /* 进入临界区（禁用中断保证原子性） */
    bool irq_state = atomic_enter_critical();
    
    thread_t *prev = (thread_t*)g_current_thread;
    thread_t *next = pick_next_thread();
    
    if (next == prev) {
        /* 退出临界区 */
        atomic_exit_critical(irq_state);
        return;  /* 不需要切换 */
    }
    
    if (prev != NULL && prev->state == THREAD_STATE_RUNNING) {
        prev->state = THREAD_STATE_READY;
        if (prev != &idle_thread) {
            enqueue_thread(prev);
        }
    }
    
    next->state = THREAD_STATE_RUNNING;
    next->last_run_time = hal_get_timestamp();  /* 使用HAL接口 */
    
    /* 原子更新当前线程指针 */
    g_current_thread = next;
    
    /* 退出临界区 */
    atomic_exit_critical(irq_state);
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[SCHED] Invariant violation detected after thread switch!\n");
    }
    
    /* 执行上下文切换 */
    context_switch(prev, next);
}

/* 选择下一个要运行的线程 */
thread_id_t scheduler_pick_next(void)
{
    /* 按优先级从高到低查找就绪线程 */
    for (int prio = HIC_PRIORITY_REALTIME; prio >= HIC_PRIORITY_IDLE; prio--) {
        ready_queue_t *queue = &ready_queues[prio];
        
        if (queue->count == 0) {
            continue;
        }
        
        for (u32 i = queue->head; i != queue->tail; i = (i + 1) % MAX_READY_THREADS) {
            thread_id_t thread_id = queue->threads[i];
            if (thread_id != 0 && thread_id < MAX_THREADS) {
                thread_t *thread = &g_threads[thread_id];
                if (thread->state == THREAD_STATE_READY && thread->domain_id != HIC_DOMAIN_CORE) {
                    return thread->thread_id;
                }
            }
        }
    }
    
    /* 没有就绪线程，返回空闲线程 */
    return idle_thread.thread_id;
}

/* 调度器时钟tick */
void scheduler_tick(void)
{
    if (g_current_thread == NULL) {
        return;
    }
    
    g_current_thread->cpu_time_used++;
    
    /* 时间片到期 */
    if (g_current_thread->time_slice > 0) {
        g_current_thread->time_slice--;
    } else {
        /* 重新分配时间片 */
        g_current_thread->time_slice = 100;  /* 默认时间片 */
        schedule();
    }
}

/* 让出CPU */
void thread_yield(void)
{
    if (g_current_thread != NULL && g_current_thread != &idle_thread) {
        g_current_thread->time_slice = 0;  /* 强制重新调度 */
    }
    schedule();
}

/* 阻塞线程 */
hic_status_t thread_block(thread_id_t thread_id) {
    if (thread_id >= MAX_THREADS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    thread_t* thread = &g_threads[thread_id];
    
    if (thread == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 从就绪队列中移除 */
    thread->state = THREAD_STATE_BLOCKED;
    
    /* 记录审计日志 */
    // AUDIT_LOG_THREAD_STATE_CHANGE(domain, thread_id, THREAD_STATE_BLOCKED);
    
    console_puts("[SCHED] Thread blocked: ");
    console_putu64(thread_id);
    console_puts("\n");
    
    /* 如果阻塞的是当前线程，触发调度 */
    if (g_current_thread == thread) {
        schedule();
    }
    
    return HIC_SUCCESS;
}

/* 唤醒线程 */
hic_status_t thread_wakeup(thread_id_t thread_id) {
    if (thread_id >= MAX_THREADS) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    thread_t* thread = &g_threads[thread_id];
    
    if (thread == NULL) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    if (thread->state != THREAD_STATE_BLOCKED) {
        return HIC_ERROR_INVALID_STATE;
    }
    
    /* 重置时间片 */
    thread->time_slice = 100;
    thread->state = THREAD_STATE_READY;
    
    /* 加入就绪队列 */
    enqueue_thread(thread);
    
    console_puts("[SCHED] Thread woken up: ");
    console_putu64(thread_id);
    console_puts("\n");
    
    return HIC_SUCCESS;
}

/* 检查线程超时 */
void thread_check_timeouts(void) {
    u64 current_time = hal_get_timestamp();
    
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_threads[i];
        
        if (t && (t->state == THREAD_STATE_BLOCKED || t->state == THREAD_STATE_WAITING)) {
            /* 检查是否超时（超时时间：5秒） */
            if (current_time - t->last_run_time > 5000000) {
                /* 超时，唤醒线程 */
                thread_wakeup(i);
            }
        }
    }
}
