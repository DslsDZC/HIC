/**
 * HIK线程管理头文件
 * 遵循三层模型文档第2.1节：执行控制与调度
 */

#ifndef HIK_KERNEL_THREAD_H
#define HIK_KERNEL_THREAD_H

#include "types.h"
#include "domain.h"
#include "hal.h"

/* 线程状态 */
typedef enum {
    THREAD_STATE_READY,       /* 就绪 */
    THREAD_STATE_RUNNING,     /* 运行中 */
    THREAD_STATE_BLOCKED,     /* 阻塞 */
    THREAD_STATE_WAITING,     /* 等待 */
    THREAD_STATE_TERMINATED,  /* 已终止 */
} thread_state_t;

/* 线程控制块 */
typedef struct thread {
    thread_id_t    thread_id;     /* 线程ID */
    domain_id_t    domain_id;     /* 所属域 */
    thread_state_t state;         /* 线程状态 */
    priority_t     priority;      /* 优先级 */
    
    /* 栈信息 */
    virt_addr_t    stack_base;    /* 栈基址 */
    size_t         stack_size;    /* 栈大小 */
    virt_addr_t    stack_ptr;     /* 当前栈指针 */
    
    /* 上下文信息（架构特定） */
    void *arch_context;           /* 架构上下文指针 */
    
    /* 调度信息 */
    u64    time_slice;            /* 时间片 */
    u64    cpu_time_used;         /* 已用CPU时间 */
    u64    last_run_time;         /* 上次运行时间戳 */
    
    /* 队列链接 */
    struct thread *next;          /* 下一个线程 */
    struct thread *prev;          /* 上一个线程 */
    
    /* 等待信息 */
    u32    wait_flags;
    void  *wait_data;
    
    /* 标志 */
    u32    flags;
#define THREAD_FLAG_KERNEL    (1U << 0)  /* 内核线程 */
#define THREAD_FLAG_USER      (1U << 1)  /* 用户线程 */
    
} thread_t;

/* 线程管理接口 */
void thread_system_init(void);

/* 创建线程 */
hik_status_t thread_create(domain_id_t domain_id, virt_addr_t entry_point,
                          priority_t priority, thread_id_t *out);

/* 终止线程 */
hik_status_t thread_terminate(thread_id_t thread_id);

/* 让出CPU */
void thread_yield(void);

/* 阻塞/唤醒 */
hik_status_t thread_block(thread_id_t thread_id);
hik_status_t thread_wakeup(thread_id_t thread_id);

/* 调度器接口（由core实现） */
void schedule(void);
void scheduler_tick(void);

/* 当前线程 */
extern thread_t *g_current_thread;

#endif /* HIK_KERNEL_THREAD_H */