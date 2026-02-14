/**
 * HIK调度器实现
 * 遵循三层模型文档第2.1节：执行控制与调度
 */

#include "thread.h"
#include "types.h"
#include "formal_verification.h"
#include "lib/mem.h"
#include "lib/console.h"

/* 当前运行的线程 */
thread_t *g_current_thread = NULL;

/* 全局线程表 */
thread_t *g_threads[MAX_THREADS] = {0};

/* 就绪队列（按优先级） */
static thread_t *ready_queues[5];  /* 5个优先级 */
static u64 runqueue_counts[5];

/* 空闲线程 */
static thread_t idle_thread;

/* 初始化调度器 */
void scheduler_init(void)
{
    for (int i = 0; i < 5; i++) {
        ready_queues[i] = NULL;
        runqueue_counts[i] = 0;
    }
    
    g_current_thread = NULL;
    
    /* 初始化空闲线程 */
    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIK_PRIORITY_IDLE;
    
    console_puts("[SCHED] Scheduler initialized\n");
}

/* 添加线程到就绪队列 */
static void enqueue_thread(thread_t *thread)
{
    if (thread == NULL || thread->priority > 4) {
        return;
    }
    
    /* 添加到队列尾部 */
    thread->next = NULL;
    thread->prev = NULL;
    
    thread_t **queue = &ready_queues[thread->priority];
    
    if (*queue == NULL) {
        *queue = thread;
    } else {
        thread_t *last = *queue;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = thread;
        thread->prev = last;
    }
    
    runqueue_counts[thread->priority]++;
    thread->state = THREAD_STATE_READY;
}

/* 从就绪队列移除线程 */
static void dequeue_thread(thread_t *thread)
{
    if (thread == NULL) {
        return;
    }
    
    if (thread->prev != NULL) {
        thread->prev->next = thread->next;
    } else {
        /* 队列头 */
        ready_queues[thread->priority] = thread->next;
    }
    
    if (thread->next != NULL) {
        thread->next->prev = thread->prev;
    }
    
    runqueue_counts[thread->priority]--;
    thread->next = NULL;
    thread->prev = NULL;
}

/* 选择下一个线程 */
static thread_t *pick_next_thread(void)
{
    /* 从高优先级到低优先级查找 */
    for (int prio = 4; prio >= 0; prio--) {
        if (ready_queues[prio] != NULL) {
            thread_t *thread = ready_queues[prio];
            /* 移到队列尾部（轮转） */
            ready_queues[prio] = thread->next;
            if (thread->next != NULL) {
                thread->next->prev = NULL;
            }
            
            /* 添加到尾部 */
            thread_t **tail = &ready_queues[prio];
            while (*tail != NULL) {
                tail = &(*tail)->next;
            }
            *tail = thread;
            thread->prev = *tail;
            thread->next = NULL;
            
            return thread;
        }
    }
    
    /* 没有就绪线程，返回空闲线程 */
    return &idle_thread;
}

/* 上下文切换（架构无关） */
extern void context_switch(thread_t *prev, thread_t *next);

/* 主调度函数 */
void schedule(void)
{
    thread_t *prev = g_current_thread;
    thread_t *next = pick_next_thread();
    
    if (next == prev) {
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
    
    /* 记录线程切换审计日志 */
    if (prev != NULL) {
        AUDIT_LOG_THREAD_SWITCH(prev->thread_id, next->thread_id, next->thread_id);
    }
    
    g_current_thread = next;
    
    /* 调用形式化验证 */
    if (fv_check_all_invariants() != FV_SUCCESS) {
        console_puts("[SCHED] Invariant violation detected after thread switch!\n");
    }
    
    /* 执行上下文切换 */
    context_switch(prev, next);
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
hik_status_t thread_block(thread_id_t thread_id)
{
    /* 完整实现：线程阻塞逻辑 */
    thread_t* thread = get_thread(thread_id);
    domain_id_t domain = thread ? thread->domain_id : 0;
    
    if (thread == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    if (thread->state == THREAD_STATE_BLOCKED) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    /* 从运行队列移除 */
    if (thread->state == THREAD_STATE_RUNNING) {
        dequeue_thread(thread);
    }
    
    /* 设置为阻塞状态 */
    thread->state = THREAD_STATE_BLOCKED;
    thread->block_reason = BLOCK_REASON_WAIT;
    thread->block_time = hal_get_timestamp();  /* 使用HAL接口 */
    
    /* 记录审计日志 */
    // AUDIT_LOG_THREAD_STATE_CHANGE(domain, thread_id, THREAD_STATE_BLOCKED);
    
    console_puts("[SCHED] Thread blocked: ");
    console_putu64(thread_id);
    console_puts("\n");
    
    /* 如果阻塞的是当前线程，触发调度 */
    if (g_current_thread == thread) {
        schedule();
    }
    
    return HIK_SUCCESS;
}

/* 唤醒线程 */
hik_status_t thread_wakeup(thread_id_t thread_id)
{
    /* 完整实现：线程唤醒逻辑 */
    thread_t* thread = get_thread(thread_id);
    domain_id_t domain = thread ? thread->domain_id : 0;
    
    if (thread == NULL) {
        return HIK_ERROR_INVALID_PARAM;
    }
    
    if (thread->state != THREAD_STATE_BLOCKED) {
        return HIK_ERROR_INVALID_STATE;
    }
    
    /* 重置时间片 */
    thread->time_slice = 100;
    thread->state = THREAD_STATE_READY;
    thread->block_reason = BLOCK_REASON_NONE;
    
    /* 加入就绪队列 */
    enqueue_thread(thread);
    
    /* 记录审计日志 */
    // AUDIT_LOG_THREAD_STATE_CHANGE(domain, thread_id, THREAD_STATE_READY);
    
    console_puts("[SCHED] Thread woken up: ");
    console_putu64(thread_id);
    console_puts("\n");
    
    return HIK_SUCCESS;
}
