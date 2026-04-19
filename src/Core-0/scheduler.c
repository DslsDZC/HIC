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
#include "domain_switch.h"
#include "pagetable.h"

/* ==================== 配置参数 ==================== */

#define MAX_THREADS           256
#define MAX_READY_THREADS     64

/* 线程数阈值 */
#define THREADS_THRESHOLD_SIMPLE   10   /* 使用精简方案 */
#define THREADS_THRESHOLD_CACHED   50   /* 使用缓存优化 */

/* 模式检查频率：每 N 次入队检查一次 */
#define MODE_CHECK_INTERVAL        100

/* 
 * 调试级别控制（策略：默认关闭以优化性能）
 * 0 = 关闭所有调试输出
 * 1 = 模式切换和错误信息
 * 2 = 详细调度信息（仅开发调试用）
 */
#ifndef SCHED_DEBUG_LEVEL
#define SCHED_DEBUG_LEVEL 0
#endif

/* 调试输出宏（机制：条件编译，零开销） */
#if SCHED_DEBUG_LEVEL >= 1
#define SCHED_LOG_BASIC(fmt, ...) console_puts(fmt)
#else
#define SCHED_LOG_BASIC(fmt, ...) ((void)0)
#endif

#if SCHED_DEBUG_LEVEL >= 2
#define SCHED_LOG_VERBOSE(fmt, ...) console_puts(fmt)
#else
#define SCHED_LOG_VERBOSE(fmt, ...) ((void)0)
#endif

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

/* 原模式：每核心独立优先级队列（AMP 强绑定） */
typedef struct {
    thread_id_t threads[MAX_READY_THREADS];
    volatile u32 head;
    volatile u32 tail;
    volatile u32 count;
} ready_queue_t;

/* 每核心独立队列：[core_id][priority] */
static ready_queue_t g_per_core_queues[MAX_LOGICAL_CORES][5];

/* 获取当前逻辑核心 ID */
static inline logical_core_id_t get_current_core_id(void) {
    return (logical_core_id_t)hal_get_cpu_id();
}

/* 空闲线程 */
thread_t idle_thread;

/* 空闲线程栈（全局静态，多核安全） */
static u64 g_idle_stack[1024] __attribute__((aligned(16)));  /* 8KB 栈 */

/* ==================== 前向声明 ==================== */

static thread_t *original_pick_next(void);
static thread_t *scheduler_pick_by_policy(logical_core_id_t core_id);
static void scheduler_update_runtime(logical_core_id_t core_id, u64 runtime);

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

    /* 初始化每核心独立队列 */
    for (u32 core = 0; core < MAX_LOGICAL_CORES; core++) {
        for (int prio = 0; prio < 5; prio++) {
            g_per_core_queues[core][prio].head = 0;
            g_per_core_queues[core][prio].tail = 0;
            g_per_core_queues[core][prio].count = 0;
            memzero((void*)g_per_core_queues[core][prio].threads, 
                    sizeof(g_per_core_queues[core][prio].threads));
        }
    }

    g_current_thread = NULL;

    /* 初始化空闲线程 */
    memzero(&idle_thread, sizeof(thread_t));
    idle_thread.thread_id = 0xFFFFFFFF;
    idle_thread.state = THREAD_STATE_READY;
    idle_thread.priority = HIC_PRIORITY_IDLE;
    idle_thread.logical_core_id = INVALID_LOGICAL_CORE;
    
    /* 使用全局静态栈（多核安全） */
    idle_thread.stack_base = (virt_addr_t)g_idle_stack;
    idle_thread.stack_size = sizeof(g_idle_stack);
    /* 栈顶指针：指向数组末尾 */
    u64 *stack_top = &g_idle_stack[1024];
    stack_top--;  /* 预留一个位置 */
    *stack_top = (u64)hal_halt;  /* idle 线程入口 = hal_halt */
    stack_top -= 6;  /* 为 callee-saved 寄存器预留空间 */
    idle_thread.stack_ptr = (virt_addr_t)stack_top;

    /* 默认使用精简模式 */
    g_perf.current_mode = SCHED_MODE_SIMPLE;
    g_perf.active_threads = 0;
    g_perf.mode_check_counter = 0;

    SCHED_LOG_BASIC("[SCHED] Scheduler initialized\n");
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
        SCHED_LOG_BASIC("[SCHED] Mode: ");
        SCHED_LOG_BASIC(mode_names[new_mode]);
        SCHED_LOG_BASIC("\n");
        g_perf.current_mode = new_mode;
    }
}

/* ==================== 精简模式实现（n ≤ 10）==================== */

static void simple_enqueue(thread_t *thread)
{
    if (thread == NULL) {
        return;
    }
    if (g_simple_queue.count >= MAX_READY_THREADS) {
        return;
    }

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
    u32 best_idx = MAX_READY_THREADS;  /* 无效索引 */
    u8 best_prio = 0;
    u32 compare_count = 0;
    
    for (u32 i = 0; i < g_simple_queue.count; i++) {
        u32 pos = (g_simple_queue.head + i) % MAX_READY_THREADS;
        thread_id_t tid = g_simple_queue.threads[pos];
        
        if (tid >= MAX_THREADS) continue;

        thread_t *t = &g_threads[tid];
        
        /* 跳过非 READY 状态的线程 */
        if (t->state != THREAD_STATE_READY) {
            continue;
        }
        
        if (t->priority > best_prio) {
            best_prio = t->priority;
            best_idx = i;
        }
        compare_count++;
    }

    if (compare_count > g_perf.pick_max_compare) {
        g_perf.pick_max_compare = compare_count;
    }

    /* 没有找到可运行的线程 */
    if (best_idx >= MAX_READY_THREADS) {
        return &idle_thread;
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
    
    /* AMP: 使用线程绑定的逻辑核心队列 */
    logical_core_id_t core_id = thread->logical_core_id;
    if (core_id >= MAX_LOGICAL_CORES) {
        core_id = 0;  /* 回退到核心 0 */
    }
    
    ready_queue_t *queue = &g_per_core_queues[core_id][thread->priority];
    
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
    /* AMP: 只从当前逻辑核心的队列选择 */
    logical_core_id_t core_id = get_current_core_id();
    if (core_id >= MAX_LOGICAL_CORES) {
        core_id = 0;
    }
    
    /* O(1)：从缓存查找最高优先级 */
    for (int prio = 4; prio >= 0; prio--) {
        if (!g_priority_cache.valid[prio]) continue;
        
        thread_id_t tid = g_priority_cache.best_thread[prio];
        if (tid >= MAX_THREADS) continue;
        
        thread_t *thread = &g_threads[tid];
        
        /* AMP: 跳过不绑定到当前核心的线程 */
        if (thread->logical_core_id != core_id) continue;
        
        if (thread->state != THREAD_STATE_READY) continue;
        
        /* 从队列中移除 */
        ready_queue_t *queue = &g_per_core_queues[core_id][prio];
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

    /* AMP: 使用线程绑定的逻辑核心队列 */
    logical_core_id_t core_id = thread->logical_core_id;
    if (core_id >= MAX_LOGICAL_CORES) {
        core_id = 0;  /* 回退到核心 0 */
    }
    
    ready_queue_t *queue = &g_per_core_queues[core_id][thread->priority];

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
    /* AMP: 只从当前逻辑核心的队列选择 */
    logical_core_id_t core_id = get_current_core_id();
    if (core_id >= MAX_LOGICAL_CORES) {
        core_id = 0;
    }
    
    /* O(1)：从高优先级队列取线程 */
    for (int prio = 4; prio >= 0; prio--) {
        ready_queue_t *queue = &g_per_core_queues[core_id][prio];

        /* 遍历队列找到第一个 READY 状态的线程 */
        while (queue->count > 0) {
            u32 head = queue->head;
            thread_id_t tid = queue->threads[head];
            queue->head = (head + 1) % MAX_READY_THREADS;
            queue->count--;

            if (tid >= MAX_THREADS) continue;
            
            thread_t *thread = &g_threads[tid];

            /* AMP: 再次验证线程绑定到当前核心 */
            if (thread->logical_core_id != core_id) {
                continue;
            }

            /* 防御性检查：跳过非 READY 状态的线程 */
            if (thread->state != THREAD_STATE_READY) {
                continue;
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
    
    /* AMP: 获取当前逻辑核心 ID */
    logical_core_id_t core_id = get_current_core_id();
    extern logical_core_t g_logical_cores[];
    logical_core_t *core = (core_id < MAX_LOGICAL_CORES) ? &g_logical_cores[core_id] : NULL;
    
    /* 策略分发：根据核心的策略选择下一个线程 */
    thread_t *next;
    if (core != NULL && core->sched_policy != SCHED_POLICY_SHARED) {
        /* 能力驱动策略 */
        next = scheduler_pick_by_policy(core_id);
    } else {
        /* 默认：共享模式 */
        next = pick_next_thread();
    }
    
    /* 调试日志：显示队列状态 */
    console_puts("[SCHED] queue_count=");
    console_putu32(g_simple_queue.count);
    console_puts(", prev_tid=");
    console_putu32(prev ? prev->thread_id : 999);
    console_puts(", next_tid=");
    console_putu32(next ? next->thread_id : 999);
    console_puts(", next_prio=");
    console_putu32(next ? next->priority : 99);
    if (core != NULL) {
        console_puts(", policy=");
        console_putu32(core->sched_policy);
    }
    console_puts("\n");
    
    if (next == prev) {
        atomic_exit_critical(irq_state);
        return;
    }
    
    /* 通知逻辑核心系统：线程停止运行 */
    if (prev != NULL && prev != &idle_thread) {
        if (prev->logical_core_id != INVALID_LOGICAL_CORE && 
            prev->logical_core_id < MAX_LOGICAL_CORES) {
            logical_core_schedule_notify(prev->logical_core_id, prev->thread_id, false);
            
            /* 更新运行时间（配额追踪） */
            u64 runtime = hal_get_timestamp() - prev->last_run_time;
            scheduler_update_runtime(prev->logical_core_id, runtime);
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
        logical_core_t *target_core = &g_logical_cores[next->logical_core_id];
        
        if (target_core->state != LOGICAL_CORE_STATE_ALLOCATED &&
            target_core->state != LOGICAL_CORE_STATE_ACTIVE &&
            target_core->state != LOGICAL_CORE_STATE_MIGRATING) {
            /* 核心不可用，跳过此线程 */
            SCHED_LOG_BASIC("[SCHED] WARN: lcore unavailable\n");
            next->state = THREAD_STATE_READY;
            enqueue_thread(next);
            next = &idle_thread;
        }
    }
    
    next->state = THREAD_STATE_RUNNING;
    next->last_run_time = hal_get_timestamp();
    g_current_thread = next;
    
    /* 更新逻辑核心的运行线程 */
    if (core != NULL) {
        core->running_thread = next->thread_id;
        core->last_schedule_time = hal_get_timestamp();
    }
    
    /* 通知逻辑核心系统：线程开始运行 */
    if (next != &idle_thread && next->logical_core_id != INVALID_LOGICAL_CORE &&
        next->logical_core_id < MAX_LOGICAL_CORES) {
        logical_core_schedule_notify(next->logical_core_id, next->thread_id, true);
    }
    
    /* 注意：页表切换由 context_switch 内部处理
     * 调度器 C 代码始终运行在 Core-0 页表下，确保能安全访问内核全局数据
     * 只有新线程开始执行时才切换到其私有页表
     */
    
    atomic_exit_critical(irq_state);
    
    context_switch(prev, next);
    
    /* 这里的代码在新线程上下文中执行 */

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

/* ==================== 策略分发（能力驱动调度） ==================== */

/**
 * 根据逻辑核心的调度策略选择下一个线程
 * 
 * 能力系统决定策略，调度器执行策略：
 * - EXCLUSIVE: 独占模式，直接返回绑定线程（无抢占）
 * - QUOTA: 配额模式，检查配额后返回线程
 * - SHARED: 共享模式，使用标准调度算法
 * - IDLE: 空闲模式，返回空闲线程
 */
thread_t *scheduler_pick_by_policy(logical_core_id_t core_id)
{
    if (core_id >= MAX_LOGICAL_CORES) {
        return &idle_thread;
    }
    
    extern logical_core_t g_logical_cores[];
    logical_core_t *core = &g_logical_cores[core_id];
    
    switch (core->sched_policy) {
        case SCHED_POLICY_EXCLUSIVE:
            /* 独占模式：只运行绑定的线程，无抢占 */
            if (core->running_thread != INVALID_THREAD) {
                thread_t *thread = &g_threads[core->running_thread];
                if (thread->state == THREAD_STATE_READY ||
                    thread->state == THREAD_STATE_RUNNING) {
                    return thread;
                }
            }
            /* 没有独占线程，运行空闲线程 */
            return &idle_thread;
            
        case SCHED_POLICY_QUOTA:
            /* 配额模式：检查配额后决定是否运行 */
            {
                u64 now = hal_get_timestamp();
                
                /* 检查是否需要开启新周期 */
                if (now >= core->sched_deadline) {
                    core->sched_deadline = now + core->sched_period;
                    core->sched_runtime = 0;
                }
                
                /* 检查配额是否用尽 */
                if (core->sched_runtime >= core->quota.allocated_time) {
                    /* 配额用尽，运行空闲线程 */
                    return &idle_thread;
                }
                
                /* 配额充足，使用标准调度 */
                return original_pick_next();
            }
            
        case SCHED_POLICY_SHARED:
            /* 共享模式：标准优先级调度 */
            return original_pick_next();
            
        case SCHED_POLICY_IDLE:
        default:
            /* 空闲模式：只运行空闲线程 */
            return &idle_thread;
    }
}

/* 更新逻辑核心的运行时间（配额追踪） */
void scheduler_update_runtime(logical_core_id_t core_id, u64 runtime)
{
    if (core_id >= MAX_LOGICAL_CORES) return;
    
    extern logical_core_t g_logical_cores[];
    logical_core_t *core = &g_logical_cores[core_id];
    
    if (core->sched_policy == SCHED_POLICY_QUOTA) {
        core->sched_runtime += runtime;
        core->quota.used_time += runtime;
    }
}

/* 设置逻辑核心的调度策略 */
hic_status_t scheduler_set_policy(logical_core_id_t core_id, 
                                   sched_policy_t policy,
                                   u64 period_ns)
{
    if (core_id >= MAX_LOGICAL_CORES) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    extern logical_core_t g_logical_cores[];
    logical_core_t *core = &g_logical_cores[core_id];
    
    bool irq = atomic_enter_critical();
    
    core->sched_policy = policy;
    core->sched_period = period_ns;
    core->sched_deadline = hal_get_timestamp() + period_ns;
    core->sched_runtime = 0;
    
    atomic_exit_critical(irq);
    
    return HIC_SUCCESS;
}