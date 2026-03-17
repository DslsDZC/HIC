/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC自适应调度器实现（优化版）
 * 
 * 根据线程数动态选择最优调度算法：
 * - n ≤ 10：精简的FIFO+O(n)查找（最佳性能）
 * - 10 < n ≤ 50：优先级缓存优化
 * - n > 50：原O(1)优先级队列（保证实时性）
 * 
 * 性能目标：调度延迟 < 100ns
 * 
 * 优化：
 * - 热路径移除调试输出
 * - 模式检查频率降低（每 100 次入队）
 */

#include "thread.h"
#include "types.h"
#include "formal_verification.h"
#include "atomic.h"
#include "hal.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "logical_core.h"

/* ==================== 配置参数 ==================== */

#define MAX_THREADS           256
#define MAX_READY_THREADS     64

/* 线程数阈值 */
#define THREADS_THRESHOLD_SIMPLE   10   /* 使用精简方案 */
#define THREADS_THRESHOLD_CACHED   50   /* 使用缓存优化 */

/* 模式检查频率：每 N 次入队检查一次 */
#define MODE_CHECK_INTERVAL        100

/* ==================== 调度模式 ==================== */

typedef enum {
    SCHED_MODE_SIMPLE,    /* 精简模式：FIFO+O(n)查找 */
    SCHED_MODE_CACHED,    /* 缓存模式：优先级缓存 */
    SCHED_MODE_ORIGINAL,  /* 原模式：O(1)优先级队列 */
} sched_mode_t;

/* ==================== 性能监控 ==================== */

typedef struct {
    u64 schedule_count;         /* 调度次数 */
    u64 schedule_total_cycles;  /* 调度总周期 */
    u64 schedule_max_cycles;    /* 最大调度周期 */
    u64 pick_count;             /* pick_next调用次数 */
    u64 pick_max_compare;       /* 最大比较次数 */
    u64 enqueue_count;          /* 入队次数 */
    u64 dequeue_count;          /* 出队次数 */
    sched_mode_t current_mode;  /* 当前调度模式 */
    u32 active_threads;         /* 活跃线程数 */
    u32 mode_check_counter;     /* 模式检查计数器 */
} scheduler_perf_t;

static scheduler_perf_t g_perf = {0};

/* ==================== 核心数据结构 ==================== */

/* 当前运行的线程（原子操作保护） */
thread_t *g_current_thread = NULL;

/* 外部引用的线程表（定义在thread.c中） */
extern thread_t g_threads[MAX_THREADS];

/* 精简模式：单队列FIFO */
typedef struct {
    thread_id_t threads[MAX_READY_THREADS];
    volatile u32 head;
    volatile u32 tail;
    volatile u32 count;
} simple_fifo_t;

static simple_fifo_t g_simple_queue;

/* 缓存模式：优先级缓存（5个优先级，每个缓存最高优先级线程） */
typedef struct {
    thread_id_t best_thread[5];  /* 每个优先级的最高优先级线程 */
    bool valid[5];              /* 缓存是否有效 */
} priority_cache_t;

static priority_cache_t g_priority_cache;

/* 原模式：5个优先级队列 */
typedef struct {
    thread_id_t threads[MAX_READY_THREADS];
    volatile u32 head;
    volatile u32 tail;
    volatile u32 count;
} ready_queue_t;

static ready_queue_t g_ready_queues[5];

/* 空闲线程 */
thread_t idle_thread;

/* ==================== 调度器初始化 ==================== */

void scheduler_init(void)
{
    console_puts("[SCHED] Initializing adaptive scheduler...\n");

    /* 初始化精简模式队列 */
    g_simple_queue.head = 0;
    g_simple_queue.tail = 0;
    g_simple_queue.count = 0;
    memzero((void*)g_simple_queue.threads, sizeof(g_simple_queue.threads));

    /* 初始化缓存模式 */
    for (int i = 0; i < 5; i++) {
        g_priority_cache.best_thread[i] = 0;
        g_priority_cache.valid[i] = false;
    }

    /* 初始化原模式队列 */
    for (int i = 0; i < 5; i++) {
        g_ready_queues[i].head = 0;
        g_ready_queues[i].tail = 0;
        g_ready_queues[i].count = 0;
        memzero((void*)g_ready_queues[i].threads, sizeof(g_ready_queues[i].threads));
    }

    g_current_thread = NULL;

    /* 初始化空闲线程 */
    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    idle_thread.logical_core_id = INVALID_LOGICAL_CORE;
    
    /* 为 idle_thread 分配栈空间（静态分配，避免动态内存） */
    static u64 idle_stack[1024];  /* 8KB 栈 */
    idle_thread.stack_base = (virt_addr_t)idle_stack;
    idle_thread.stack_size = sizeof(idle_stack);
    /* 栈顶指针：指向数组末尾，为 ret 预留一个位置 */
    u64 *stack_top = &idle_stack[1024];
    stack_top--;  /* 预留一个位置 */
    *stack_top = (u64)hal_halt;  /* idle 线程入口 = hal_halt */
    stack_top -= 6;  /* 为 callee-saved 寄存器预留空间 */
    idle_thread.stack_ptr = (virt_addr_t)stack_top;

    /* 默认使用精简模式 */
    g_perf.current_mode = SCHED_MODE_SIMPLE;
    g_perf.active_threads = 0;
    g_perf.mode_check_counter = 0;

    console_puts("[SCHED] Scheduler initialized (mode: SIMPLE)\n");
    console_puts("[SCHED] idle_thread stack: 0x");
    console_puthex64(idle_thread.stack_ptr);
    console_puts("\n");
}

/* ==================== 模式选择逻辑 ==================== */

static void update_sched_mode(void)
{
    u32 active_count = 0;
    
    /* 统计活跃线程数 */
    for (u32 i = 0; i < MAX_THREADS; i++) {
        if (g_threads[i].thread_id == (thread_id_t)i &&
            (g_threads[i].state == THREAD_STATE_READY || 
             g_threads[i].state == THREAD_STATE_RUNNING)) {
            active_count++;
        }
    }
    
    g_perf.active_threads = active_count;

    /* 根据线程数选择模式 */
    sched_mode_t new_mode;
    
    if (active_count <= THREADS_THRESHOLD_SIMPLE) {
        new_mode = SCHED_MODE_SIMPLE;
    } else if (active_count <= THREADS_THRESHOLD_CACHED) {
        new_mode = SCHED_MODE_CACHED;
    } else {
        new_mode = SCHED_MODE_ORIGINAL;
    }
    
    /* 模式切换时输出日志 */
    if (new_mode != g_perf.current_mode) {
        const char *mode_names[] = {"SIMPLE", "CACHED", "ORIGINAL"};
        console_puts("[SCHED] Mode: ");
        console_puts(mode_names[new_mode]);
        console_puts(" (threads: ");
        console_putu32(active_count);
        console_puts(")\n");
        g_perf.current_mode = new_mode;
    }
}

/* ==================== 精简模式实现（n ≤ 10）==================== */

static void simple_enqueue(thread_t *thread)
{
    if (thread == NULL) return;
    if (g_simple_queue.count >= MAX_READY_THREADS) return;

    u32 tail = g_simple_queue.tail;
    g_simple_queue.threads[tail] = thread->thread_id;
    g_simple_queue.tail = (tail + 1) % MAX_READY_THREADS;
    g_simple_queue.count++;
    thread->state = THREAD_STATE_READY;
    g_perf.enqueue_count++;
}

static thread_t *simple_pick_next(void)
{
    if (g_simple_queue.count == 0) return &idle_thread;

    /* O(n)查找最高优先级 */
    u32 best_idx = 0;
    u8 best_prio = 0;
    u32 compare_count = 0;

    for (u32 i = 0; i < g_simple_queue.count; i++) {
        u32 pos = (g_simple_queue.head + i) % MAX_READY_THREADS;
        thread_id_t tid = g_simple_queue.threads[pos];
        
        if (tid >= MAX_THREADS) continue;

        thread_t *t = &g_threads[tid];
        
        if (t->priority > best_prio) {
            best_prio = t->priority;
            best_idx = i;
        }
        compare_count++;
    }

    if (compare_count > g_perf.pick_max_compare) {
        g_perf.pick_max_compare = compare_count;
    }

    /* 取出线程 */
    u32 pos = (g_simple_queue.head + best_idx) % MAX_READY_THREADS;
    thread_id_t tid = g_simple_queue.threads[pos];
    thread_t *thread = &g_threads[tid];

    /* 移除线程（移动数组元素） */
    for (u32 i = best_idx; i < g_simple_queue.count - 1; i++) {
        u32 curr = (g_simple_queue.head + i) % MAX_READY_THREADS;
        u32 next = (g_simple_queue.head + i + 1) % MAX_READY_THREADS;
        g_simple_queue.threads[curr] = g_simple_queue.threads[next];
    }
    g_simple_queue.count--;
    g_simple_queue.tail = (g_simple_queue.tail - 1 + MAX_READY_THREADS) % MAX_READY_THREADS;
    g_perf.dequeue_count++;

    return thread;
}

/* ==================== 缓存模式实现（10 < n ≤ 50）==================== */

static void cached_enqueue(thread_t *thread)
{
    if (thread == NULL) return;
    
    ready_queue_t *queue = &g_ready_queues[thread->priority];
    
    if (queue->count >= MAX_READY_THREADS) return;

    u32 tail = queue->tail;
    queue->threads[tail] = thread->thread_id;
    queue->tail = (tail + 1) % MAX_READY_THREADS;
    queue->count++;
    thread->state = THREAD_STATE_READY;
    
    /* 更新缓存 */
    g_priority_cache.best_thread[thread->priority] = thread->thread_id;
    g_priority_cache.valid[thread->priority] = true;
    
    g_perf.enqueue_count++;
}

static thread_t *cached_pick_next(void)
{
    /* O(1)：从缓存查找最高优先级 */
    for (int prio = 4; prio >= 0; prio--) {
        if (!g_priority_cache.valid[prio]) continue;
        
        thread_id_t tid = g_priority_cache.best_thread[prio];
        if (tid >= MAX_THREADS) continue;
        
        thread_t *thread = &g_threads[tid];
        if (thread->state != THREAD_STATE_READY) continue;
        
        /* 从队列中移除 */
        ready_queue_t *queue = &g_ready_queues[prio];
        if (queue->count == 0) continue;
        
        queue->head = (queue->head + 1) % MAX_READY_THREADS;
        queue->count--;
        
        /* 更新缓存 */
        g_priority_cache.valid[prio] = false;
        for (u32 i = queue->head; i != queue->tail; i = (i + 1) % MAX_READY_THREADS) {
            thread_id_t next_tid = queue->threads[i];
            if (next_tid < MAX_THREADS) {
                g_priority_cache.best_thread[prio] = next_tid;
                g_priority_cache.valid[prio] = true;
                break;
            }
        }
        
        g_perf.dequeue_count++;
        return thread;
    }

    return &idle_thread;
}

/* ==================== 原模式实现（n > 50）==================== */

static void original_enqueue(thread_t *thread)
{
    if (thread == NULL || thread->priority > 4) return;

    ready_queue_t *queue = &g_ready_queues[thread->priority];

    if (queue->count >= MAX_READY_THREADS) return;

    u32 tail = queue->tail;
    queue->threads[tail] = thread->thread_id;
    queue->tail = (tail + 1) % MAX_READY_THREADS;
    queue->count++;
    thread->state = THREAD_STATE_READY;
    g_perf.enqueue_count++;
}

static thread_t *original_pick_next(void)
{
    /* O(1)：从高优先级队列取线程 */
    for (int prio = 4; prio >= 0; prio--) {
        ready_queue_t *queue = &g_ready_queues[prio];

        if (queue->count > 0) {
            u32 head = queue->head;
            thread_id_t tid = queue->threads[head];
            queue->head = (head + 1) % MAX_READY_THREADS;
            queue->count--;

            thread_t *thread = &g_threads[tid];

            /* 轮转调度 */
            if (queue->count > 0) {
                u32 tail = queue->tail;
                queue->threads[tail] = tid;
                queue->tail = (tail + 1) % MAX_READY_THREADS;
                queue->count++;
            }

            g_perf.dequeue_count++;
            return thread;
        }
    }

    return &idle_thread;
}

/* ==================== 统一接口（优化：缓存模式检查）==================== */

static void enqueue_thread(thread_t *thread)
{
    /* 每 MODE_CHECK_INTERVAL 次入队才检查模式 */
    if (++g_perf.mode_check_counter >= MODE_CHECK_INTERVAL) {
        g_perf.mode_check_counter = 0;
        update_sched_mode();
    }
    
    switch (g_perf.current_mode) {
        case SCHED_MODE_SIMPLE:
            simple_enqueue(thread);
            break;
        case SCHED_MODE_CACHED:
            cached_enqueue(thread);
            break;
        case SCHED_MODE_ORIGINAL:
            original_enqueue(thread);
            break;
    }
}

static thread_t *pick_next_thread(void)
{
    g_perf.pick_count++;
    
    switch (g_perf.current_mode) {
        case SCHED_MODE_SIMPLE:
            return simple_pick_next();
        case SCHED_MODE_CACHED:
            return cached_pick_next();
        case SCHED_MODE_ORIGINAL:
            return original_pick_next();
        default:
            return &idle_thread;
    }
}

/* ==================== 主调度函数 ==================== */

extern void context_switch(thread_t *prev, thread_t *next);

/* 外部引用：逻辑核心调度函数 */
extern logical_core_id_t logical_core_schedule_select(thread_t *thread);
extern void logical_core_schedule_notify(logical_core_id_t logical_core_id,
                                         thread_id_t thread_id,
                                         bool starting);
extern logical_core_t g_logical_cores[];

void schedule(void)
{
    u64 start_cycles = hal_get_timestamp();
    g_perf.schedule_count++;

    bool irq_state = atomic_enter_critical();
    
    thread_t *prev = (thread_t*)g_current_thread;
    thread_t *next = pick_next_thread();
    
    /* 调试输出：前10次调度 */
    static u32 debug_counter = 0;
    debug_counter++;
    if (debug_counter <= 10) {
        console_puts("[SCHED] #");
        console_putu32(debug_counter);
        console_puts(" queue=");
        console_putu32(g_simple_queue.count);
        console_puts(" prev=");
        if (prev == NULL) console_puts("NULL");
        else if (prev == &idle_thread) console_puts("IDLE");
        else { console_putu64(prev->thread_id); }
        console_puts(" next=");
        if (next == &idle_thread) console_puts("IDLE");
        else { 
            console_putu64(next->thread_id);
            console_puts(" lcore=");
            console_putu64(next->logical_core_id);
            console_puts(" stack_ptr=");
            console_puthex64(next->stack_ptr);
        }
        console_puts("\n");
    }
    
    if (next == prev) {
        atomic_exit_critical(irq_state);
        return;
    }
    
    /* 通知逻辑核心系统：线程停止运行 */
    if (prev != NULL && prev != &idle_thread) {
        if (prev->logical_core_id != INVALID_LOGICAL_CORE && 
            prev->logical_core_id < MAX_LOGICAL_CORES) {
            logical_core_schedule_notify(prev->logical_core_id, prev->thread_id, false);
        }
    }
    
    if (prev != NULL && prev->state == THREAD_STATE_RUNNING) {
        prev->state = THREAD_STATE_READY;
        if (prev != &idle_thread) {
            enqueue_thread(prev);
        }
    }
    
    /* 检查线程是否绑定到逻辑核心 */
    if (next != &idle_thread && next->logical_core_id != INVALID_LOGICAL_CORE) {
        logical_core_t *core = &g_logical_cores[next->logical_core_id];
        
        /* 调试输出：逻辑核心状态 */
        if (debug_counter == 0) {
            console_puts("[SCHED] Thread ");
            console_putu64(next->thread_id);
            console_puts(" lcore ");
            console_putu64(next->logical_core_id);
            console_puts(" state: ");
            console_putu32(core->state);
            console_puts("\n");
        }
        
        if (core->state != LOGICAL_CORE_STATE_ALLOCATED &&
            core->state != LOGICAL_CORE_STATE_ACTIVE &&
            core->state != LOGICAL_CORE_STATE_MIGRATING) {
            /* 核心不可用，跳过此线程 */
            console_puts("[SCHED] WARN: lcore ");
            console_putu64(next->logical_core_id);
            console_puts(" unavailable (state=");
            console_putu32(core->state);
            console_puts("), skipping thread\n");
            next->state = THREAD_STATE_READY;
            enqueue_thread(next);
            next = &idle_thread;
        }
    }
    
    next->state = THREAD_STATE_RUNNING;
    next->last_run_time = hal_get_timestamp();
    g_current_thread = next;
    
    /* 通知逻辑核心系统：线程开始运行 */
    if (next != &idle_thread && next->logical_core_id != INVALID_LOGICAL_CORE &&
        next->logical_core_id < MAX_LOGICAL_CORES) {
        logical_core_schedule_notify(next->logical_core_id, next->thread_id, true);
    }
    
    atomic_exit_critical(irq_state);
    
    context_switch(prev, next);

    u64 end_cycles = hal_get_timestamp();
    u64 cycles = end_cycles - start_cycles;
    g_perf.schedule_total_cycles += cycles;
    if (cycles > g_perf.schedule_max_cycles) {
        g_perf.schedule_max_cycles = cycles;
    }
}

/* ==================== 其他调度器函数 ==================== */

thread_id_t scheduler_pick_next(void)
{
    thread_t *next = pick_next_thread();
    return next->thread_id;
}

void scheduler_tick(void)
{
    if (g_current_thread == NULL) return;
    
    g_current_thread->cpu_time_used++;
    
    if (g_current_thread->time_slice > 0) {
        g_current_thread->time_slice--;
    } else {
        g_current_thread->time_slice = 100;
        schedule();
    }
}

void thread_yield(void)
{
    if (g_current_thread != NULL && g_current_thread != &idle_thread) {
        g_current_thread->time_slice = 0;
    }
    schedule();
}

hic_status_t thread_block(thread_id_t thread_id) {
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;
    
    thread_t* thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;
    
    thread->state = THREAD_STATE_BLOCKED;
    
    if (g_current_thread == thread) schedule();
    
    return HIC_SUCCESS;
}

hic_status_t thread_wakeup(thread_id_t thread_id) {
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;
    
    thread_t* thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;
    
    if (thread->state != THREAD_STATE_BLOCKED) return HIC_ERROR_INVALID_STATE;
    
    thread->time_slice = 100;
    thread->state = THREAD_STATE_READY;
    enqueue_thread(thread);
    
    return HIC_SUCCESS;
}

/**
 * 将新创建的线程加入调度队列
 */
hic_status_t thread_ready(thread_id_t thread_id) {
    if (thread_id >= MAX_THREADS) return HIC_ERROR_INVALID_PARAM;
    
    thread_t* thread = &g_threads[thread_id];
    if (thread == NULL) return HIC_ERROR_INVALID_PARAM;
    
    if (thread->state != THREAD_STATE_READY) {
        thread->state = THREAD_STATE_READY;
    }
    
    thread->time_slice = 100;
    enqueue_thread(thread);
    
    return HIC_SUCCESS;
}

void thread_check_timeouts(void) {
    u64 current_time = hal_get_timestamp();
    
    for (u32 i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &g_threads[i];
        
        if (t && (t->state == THREAD_STATE_BLOCKED || t->state == THREAD_STATE_WAITING)) {
            if (current_time - t->last_run_time > 5000000) {
                thread_wakeup(i);
            }
        }
    }
}

/* ==================== 性能监控 ==================== */

void scheduler_get_perf(u64 *schedule_count, u64 *avg_cycles, u64 *max_cycles) {
    if (schedule_count) *schedule_count = g_perf.schedule_count;
    if (avg_cycles && g_perf.schedule_count > 0) {
        *avg_cycles = g_perf.schedule_total_cycles / g_perf.schedule_count;
    }
    if (max_cycles) *max_cycles = g_perf.schedule_max_cycles;
}

void scheduler_print_perf(void)
{
    const char *mode_names[] = {"SIMPLE", "CACHED", "ORIGINAL"};
    
    console_puts("[SCHED] Performance:\n");
    console_puts("  Mode: ");
    console_puts(mode_names[g_perf.current_mode]);
    console_puts(", Threads: ");
    console_putu32(g_perf.active_threads);
    console_puts("\n");
    
    console_puts("  Schedule: ");
    console_puthex64(g_perf.schedule_count);
    console_puts(" (avg ");
    if (g_perf.schedule_count > 0) {
        console_puthex64(g_perf.schedule_total_cycles / g_perf.schedule_count);
    }
    console_puts(" cycles, max ");
    console_puthex64(g_perf.schedule_max_cycles);
    console_puts(")\n");
    
    console_puts("  Enqueue: ");
    console_puthex64(g_perf.enqueue_count);
    console_puts(", Dequeue: ");
    console_puthex64(g_perf.dequeue_count);
    console_puts("\n");
}