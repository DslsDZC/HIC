<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# Core-0层：内核核心与仲裁者

Core-0是HIK系统的核心层，负责系统的可信计算基（TCB）功能。

## 概述

### 职责

Core-0承担以下核心职责：

1. **物理资源管理** - 管理所有物理内存、CPU时间、硬件中断
2. **能力系统内核** - 维护全局能力表，管理能力生命周期
3. **执行控制与调度** - 管理线程，调度CPU时间
4. **隔离强制实施** - 通过MMU和能力系统强制隔离
5. **异常处理** - 处理所有异常和硬件中断

### 设计目标

- **极简主义** - 代码规模<10,000行C代码（不含架构特定汇编）
- **可验证性** - 代码结构清晰，易于形式化验证
- **高性能** - 关键路径优化，确保低延迟
- **高可靠性** - 故障隔离，快速恢复

## 物理资源管理

### 内存管理

#### 物理帧管理

```c
/**
 * 物理内存帧位图
 * 每位对应一个4KB物理页
 */
typedef struct {
    u64 *bitmap;        /* 位图数组 */
    u64 total_frames;   /* 总帧数 */
    u64 free_frames;    /* 空闲帧数 */
    u64 used_frames;    /* 已用帧数 */
} pmm_t;

/* 物理内存管理器 */
static pmm_t g_pmm;
```

#### 内存分配接口

```c
/**
 * @brief 分配物理内存帧
 * @param owner 所有者域ID
 * @param count 帧数量
 * @param type 帧类型
 * @param out 输出物理地址
 * @return 状态码
 */
status_t pmm_alloc_frames(domain_id_t owner, u64 count,
                         frame_type_t type, phys_addr_t *out);

/**
 * @brief 释放物理内存帧
 * @param addr 物理地址
 * @param count 帧数量
 * @return 状态码
 */
status_t pmm_free_frames(phys_addr_t addr, u64 count);
```

#### 内存布局

```
物理内存布局：
0x00000000 - 0x000FFFFF: Core-0代码（只读）
0x00100000 - 0x001FFFFF: Core-0数据
0x00200000 - 0x002FFFFF: Core-0堆栈
0x00300000 - 0x003FFFFF: 审计日志缓冲区
0x00400000 - 0x004FFFFF: 能力表
0x00500000 - 0x005FFFFF: 线程控制块
0x00600000 - 0x00FFFFFF: 保留
0x01000000 - 0x01FFFFFF: Privileged-1服务区域
0x02000000 - 0x0FFFFFFF: 应用区域
0x10000000 - 0x1FFFFFFF: 设备MMIO区域
0x20000000 - 0xFFFFFFFF: 用户区域
```

### CPU时间管理

#### 调度器

```c
/**
 * 线程调度器
 */
typedef struct {
    thread_t *ready_queue[MAX_PRIORITY];  /* 就绪队列 */
    thread_t *current_thread;            /* 当前线程 */
    u64 context_switches;                /* 上下文切换次数 */
    u64 idle_ticks;                      /* 空闲时钟周期 */
} scheduler_t;

/* 全局调度器 */
static scheduler_t g_scheduler;
```

#### 调度策略

```c
/**
 * @brief 调度策略类型
 */
typedef enum {
    SCHED_FIFO,      /* 先进先出 */
    SCHED_RR,        /* 轮转调度 */
    SCHED_PRIORITY,  /* 优先级调度 */
    SCHED_DEADLINE,  /* 截止期限调度 */
} sched_policy_t;

/**
 * @brief 选择下一个线程
 * @return 线程指针
 */
thread_t *schedule_next_thread(void)
{
    switch (g_policy) {
    case SCHED_PRIORITY:
        return schedule_priority();
    case SCHED_RR:
        return schedule_round_robin();
    default:
        return schedule_fifo();
    }
}
```

### 中断管理

#### 中断描述符表

```c
/**
 * 中断门描述符
 */
typedef struct {
    u16 offset_low;    /* 偏移低16位 */
    u16 selector;      /* 段选择子 */
    u8  ist;           /* 中断栈表 */
    u8  type_attr;     /* 类型和属性 */
    u16 offset_mid;    /* 偏移中16位 */
    u32 offset_high;   /* 偏移高32位 */
    u32 reserved;      /* 保留 */
} __attribute__((packed)) idt_entry_t;

/* IDT数组 */
static idt_entry_t g_idt[256];
```

#### 中断路由表

```c
/**
 * 中断路由表项
 */
typedef struct {
    u32 irq_vector;        /* 中断向量号 */
    domain_id_t domain_id;  /* 处理服务域ID */
    void (*handler)(void);  /* 处理函数 */
    u32 priority;          /* 优先级 */
} irq_route_entry_t;

/* 中断路由表（构建时生成） */
static irq_route_entry_t g_irq_route_table[256];
```

## 能力系统

### 能力表

```c
/**
 * 能力表项
 */
typedef struct {
    cap_id_t id;              /* 能力ID */
    cap_type_t type;          /* 能力类型 */
    cap_rights_t rights;      /* 权限 */
    domain_id_t owner;        /* 所有者域ID */
    cap_flags_t flags;        /* 标志 */
    union {
        struct {
            phys_addr_t base;  /* 物理地址 */
            u64 size;         /* 大小 */
        } memory;
        struct {
            u16 base;         /* I/O端口基地址 */
            u16 count;        /* 端口数量 */
        } io_port;
        struct {
            cap_id_t endpoint; /* IPC端点ID */
        } ipc;
    } resource;
} cap_entry_t;

/* 全局能力表 */
static cap_entry_t g_cap_table[MAX_CAPABILITIES];
```

### 能力操作

#### 能力创建

```c
/**
 * @brief 创建新能力
 * @param owner 所有者域ID
 * @param type 能力类型
 * @param rights 权限
 * @param resource 资源描述
 * @param out 输出能力ID
 * @return 状态码
 */
status_t cap_create(domain_id_t owner, cap_type_t type,
                    cap_rights_t rights, void *resource,
                    cap_id_t *out)
{
    /* 分配能力ID */
    cap_id_t id = allocate_cap_id();
    if (id == HIK_INVALID_CAP_ID) {
        return HIK_ERROR_NO_RESOURCE;
    }

    /* 初始化能力 */
    cap_entry_t *entry = &g_cap_table[id];
    entry->id = id;
    entry->type = type;
    entry->rights = rights;
    entry->owner = owner;
    entry->flags = 0;

    /* 复制资源描述 */
    memcpy(&entry->resource, resource, sizeof(entry->resource));

    /* 记录审计日志 */
    AUDIT_LOG_CAP_CREATE(owner, id);

    *out = id;
    return HIK_SUCCESS;
}
```

#### 能力验证

```c
/**
 * @brief 验证能力
 * @param domain 域ID
 * @param cap_id 能力ID
 * @param required_rights 所需权限
 * @param out 输出能力指针
 * @return 状态码
 */
status_t cap_verify(domain_id_t domain, cap_id_t cap_id,
                    cap_rights_t required_rights,
                    cap_entry_t **out)
{
    /* 检查能力ID有效性 */
    if (cap_id >= MAX_CAPABILITIES) {
        return HIK_ERROR_INVALID_CAP;
    }

    cap_entry_t *entry = &g_cap_table[cap_id];

    /* 检查能力是否被撤销 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIK_ERROR_REVOKED;
    }

    /* 检查所有权 */
    if (entry->owner != domain) {
        return HIK_ERROR_PERMISSION;
    }

    /* 检查权限 */
    if ((entry->rights & required_rights) != required_rights) {
        return HIK_ERROR_PERMISSION;
    }

    /* 记录审计日志 */
    AUDIT_LOG_CAP_VERIFY(domain, cap_id, true);

    *out = entry;
    return HIK_SUCCESS;
}
```

#### 能力传递

```c
/**
 * @brief 传递能力
 * @param from_domain 发送域
 * @param to_domain 接收域
 * @param cap_id 能力ID
 * @param sub_rights 子权限（可选）
 * @return 状态码
 */
status_t cap_transfer(domain_id_t from_domain, domain_id_t to_domain,
                     cap_id_t cap_id, cap_rights_t sub_rights)
{
    cap_entry_t *entry;

    /* 验证发送方能力 */
    status = cap_verify(from_domain, cap_id, CAP_PERM_TRANSFER, &entry);
    if (status != HIK_SUCCESS) {
        return status;
    }

    /* 创建新能力（派生） */
    cap_id_t new_id;
    status = cap_create(to_domain, entry->type,
                       sub_rights ? sub_rights : entry->rights,
                       &entry->resource, &new_id);
    if (status != HIK_SUCCESS) {
        return status;
    }

    /* 记录审计日志 */
    AUDIT_LOG_CAP_TRANSFER(from_domain, to_domain, cap_id);

    return HIK_SUCCESS;
}
```

## 执行控制

### 线程管理

#### 线程控制块

```c
/**
 * 线程控制块
 */
typedef struct {
    thread_id_t id;              /* 线程ID */
    domain_id_t domain_id;        /* 所属域ID */
    u8 priority;                 /* 优先级 */
    u8 state;                    /* 状态 */
    context_t context;            /* 上下文 */
    u64 runtime;                 /* 运行时间 */
    thread_t *next;              /* 链表指针 */
} thread_t;

/* 全局线程表 */
static thread_t *g_threads[MAX_THREADS];
```

#### 线程创建

```c
/**
 * @brief 创建新线程
 * @param domain_id 域ID
 * @param entry 入口函数
 * @param stack_size 栈大小
 * @param priority 优先级
 * @param out 输出线程ID
 * @return 状态码
 */
status_t thread_create(domain_id_t domain_id, void (*entry)(void),
                      u64 stack_size, u8 priority, thread_id_t *out)
{
    /* 分配线程ID */
    thread_id_t tid = allocate_thread_id();
    if (tid == HIK_INVALID_THREAD) {
        return HIK_ERROR_NO_RESOURCE;
    }

    /* 分配栈 */
    phys_addr_t stack_base;
    status = pmm_alloc_frames(domain_id, stack_size / PAGE_SIZE,
                             PAGE_FRAME_USER, &stack_base);
    if (status != HIK_SUCCESS) {
        return status;
    }

    /* 初始化TCB */
    thread_t *thread = g_threads[tid];
    thread->id = tid;
    thread->domain_id = domain_id;
    thread->priority = priority;
    thread->state = THREAD_STATE_READY;
    thread->runtime = 0;

    /* 初始化上下文 */
    init_context(&thread->context, entry, stack_base, stack_size);

    /* 添加到就绪队列 */
    add_to_ready_queue(thread);

    /* 记录审计日志 */
    AUDIT_LOG_THREAD_CREATE(domain_id, tid);

    *out = tid;
    return HIK_SUCCESS;
}
```

### 上下文切换

#### 上下文结构

```c
/**
 * 上下文结构
 */
typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8, r9, r10, r11;
    u64 r12, r13, r14, r15;
    u64 rip, rflags;
} context_t;
```

#### 上下文切换函数

```assembly
/**
 * @file context.S
 * @brief 上下文切换汇编实现
 */

.section .text
.global context_switch

/**
 * @brief 上下文切换
 * @param %rdi 旧上下文指针
 * @param %rsi 新上下文指针
 */
context_switch:
    /* 保存旧上下文 */
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rsi, 32(%rdi)
    movq %rdi, 40(%rdi)
    movq %rbp, 48(%rdi)
    movq %rsp, 56(%rdi)
    movq %r8, 64(%rdi)
    movq %r9, 72(%rdi)
    movq %r10, 80(%rdi)
    movq %r11, 88(%rdi)
    movq %r12, 96(%rdi)
    movq %r13, 104(%rdi)
    movq %r14, 112(%rdi)
    movq %r15, 120(%rdi)
    movq (%rsp), %rax      /* 返回地址 */
    movq %rax, 128(%rdi)
    pushfq
    popq %rax
    movq %rax, 136(%rdi)

    /* 恢复新上下文 */
    movq 0(%rsi), %rax
    movq 8(%rsi), %rbx
    movq 16(%rsi), %rcx
    movq 24(%rsi), %rdx
    movq 40(%rsi), %rdi      /* %rsi是第三个参数 */
    movq 48(%rsi), %rbp
    movq 56(%rsi), %rsp
    movq 64(%rsi), %r8
    movq 72(%rsi), %r9
    movq 80(%rsi), %r10
    movq 88(%rsi), %r11
    movq 96(%rsi), %r12
    movq 104(%rsi), %r13
    movq 112(%rsi), %r14
    movq 120(%rsi), %r15
    movq 136(%rsi), %rax
    pushq %rax
    popfq
    movq 128(%rsi), %rax
    ret
```

## 异常处理

### 异常向量

```c
/**
 * 异常处理函数类型
 */
typedef void (*exception_handler_t)(exception_frame_t *frame);

/**
 * 异常处理表
 */
static exception_handler_t g_exception_handlers[32];
```

### 异常处理流程

```
异常发生：
1. CPU保存上下文
2. 跳转到Core-0异常入口
3. 查找异常处理函数
4. 调用处理函数
5. 恢复上下文
6. 返回
```

### 异常处理示例

```c
/**
 * @brief 页面错误处理
 * @param frame 异常帧
 */
void handle_page_fault(exception_frame_t *frame)
{
    /* 获取故障地址 */
    u64 fault_addr = read_cr2();

    /* 查找所属域 */
    domain_id_t domain = find_domain_by_address(fault_addr);
    if (domain == HIK_INVALID_DOMAIN) {
        /* 非法访问，终止线程 */
        terminate_current_thread();
        return;
    }

    /* 检查是否在能力授权范围内 */
    if (!check_capability(domain, fault_addr, CAP_PERM_READ)) {
        /* 权限不足，终止线程 */
        terminate_current_thread();
        return;
    }

    /* 通知域处理页面错误 */
    notify_domain_exception(domain, EXCEPT_PAGE_FAULT, fault_addr);
}
```

## 系统调用

### 系统调用表

```c
/**
 * 系统调用函数类型
 */
typedef status_t (*syscall_handler_t)(syscall_frame_t *frame);

/**
 * 系统调用表
 */
static syscall_handler_t g_syscall_table[256];
```

### 系统调用处理

```c
/**
 * @brief 系统调用入口
 * @param frame 系统调用帧
 */
void syscall_entry(syscall_frame_t *frame)
{
    /* 获取系统调用号 */
    u64 syscall_num = frame->rax;

    /* 验证系统调用号 */
    if (syscall_num >= 256 || g_syscall_table[syscall_num] == NULL) {
        frame->rax = HIK_ERROR_INVALID_SYSCALL;
        return;
    }

    /* 调用处理函数 */
    status = g_syscall_table[syscall_num](frame);

    /* 记录审计日志 */
    domain_id_t domain = get_current_domain();
    AUDIT_LOG_SYSCALL(domain, syscall_num, (status == HIK_SUCCESS));
}
```

### 常用系统调用

```c
/**
 * @brief 能力查询
 */
status_t syscall_cap_query(syscall_frame_t *frame)
{
    cap_id_t cap_id = frame->rdi;
    cap_entry_t *entry;

    status = cap_verify(get_current_domain(), cap_id, 0, &entry);
    if (status != HIK_SUCCESS) {
        return status;
    }

    /* 返回能力信息 */
    frame->rax = entry->type;
    frame->rbx = entry->rights;
    frame->rcx = entry->flags;

    return HIK_SUCCESS;
}

/**
 * @brief IPC调用
 */
status_t syscall_ipc_call(syscall_frame_t *frame)
{
    cap_id_t endpoint_cap = frame->rdi;
    void *message = (void *)frame->rsi;
    u64 size = frame->rdx;

    /* 验证端点能力 */
    cap_entry_t *cap;
    status = cap_verify(get_current_domain(), endpoint_cap,
                       CAP_PERM_CALL, &cap);
    if (status != HIK_SUCCESS) {
        return status;
    }

    /* 查找目标域 */
    domain_id_t target_domain = cap->resource.ipc.endpoint;

    /* 调用目标服务 */
    status = invoke_service(target_domain, message, size);

    /* 记录审计日志 */
    AUDIT_LOG_IPC_CALL(get_current_domain(), endpoint_cap, true);

    return status;
}
```

## 性能优化

### 快速路径

```c
/**
 * @brief 快速系统调用路径
 * @param frame 系统调用帧
 */
__attribute__((always_inline))
static inline void fast_syscall(syscall_frame_t *frame)
{
    /* 内联能力验证 */
    if (LIKELY(frame->rax < SYSCALL_FAST_MAX)) {
        /* 直接调用处理函数 */
        frame->rax = g_fast_syscall_table[frame->rax](frame);
        return;
    }

    /* 慢速路径 */
    syscall_entry(frame);
}
```

### 缓存优化

```c
/**
 * @brief 热路径数据结构
 * __attribute__((aligned(64))) - 缓存行对齐
 */
typedef struct __attribute__((aligned(64))) {
    cap_entry_t caps[64];      /* 能力表缓存 */
    thread_t *ready[64];       /* 就绪队列缓存 */
    u64 padding[8];            /* 避免伪共享 */
} hot_path_t;

static hot_path_t g_hot_path;
```

## 安全特性

### 最小特权原则

Core-0只运行必要的代码：
- 不实现文件系统
- 不实现网络协议栈
- 不实现图形服务
- 这些功能由Privileged-1服务提供

### 形式化验证

Core-0的核心不变式都有数学证明：
- 能力守恒
- 域隔离
- 资源配额
- 类型安全

详见 [形式化验证](./15-FormalVerification.md)。

### 审计日志

所有关键操作都记录审计日志：
- 能力操作
- 系统调用
- 线程调度
- 异常处理

详见 [审计日志](./14-AuditLogging.md)。

## 调试支持

### 调试接口

```c
/**
 * @brief 调试系统调用
 */
status_t syscall_debug(syscall_frame_t *frame)
{
    u32 cmd = frame->edi;
    u64 arg1 = frame->rsi;
    u64 arg2 = frame->rdx;

    switch (cmd) {
    case DEBUG_DUMP_CAPS:
        debug_dump_capabilities();
        break;
    case DEBUG_DUMP_THREADS:
        debug_dump_threads();
        break;
    case DEBUG_DUMP_MEMORY:
        debug_dump_memory_map();
        break;
    default:
        return HIK_ERROR_INVALID_PARAM;
    }

    return HIK_SUCCESS;
}
```

### 性能监控

```c
/**
 * @brief 性能统计
 */
typedef struct {
    u64 syscalls;           /* 系统调用次数 */
    u64 context_switches;   /* 上下文切换次数 */
    u64 interrupts;         /* 中断次数 */
    u64 page_faults;        /* 页面错误次数 */
    u64 capability_checks;  /* 能力验证次数 */
} perf_stats_t;

static perf_stats_t g_perf_stats;
```

## 参考资料

- [架构设计](./02-Architecture.md)
- [能力系统](./11-CapabilitySystem.md)
- [物理内存管理](./12-PhysicalMemory.md)
- [形式化验证](./15-FormalVerification.md)

---

*最后更新: 2026-02-14*