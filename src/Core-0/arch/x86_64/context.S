/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64上下文切换汇编实现（多核支持）
 */

.section .text

/* ==================== 外部函数声明 ==================== */

/* CPU ID获取函数 */
.extern x86_64_get_cpu_id

/* ==================== Per-CPU数据结构偏移 ==================== */

/* percpu_data_t结构偏移（必须与percpu.h中定义一致） */
#define PERCPU_CPU_ID           0
#define PERCPU_IS_BSP           4
#define PERCPU_IS_ONLINE        5
#define PERCPU_STATE            6
#define PERCPU_STACK_BASE       8
#define PERCPU_STACK_SIZE       16
#define PERCPU_STACK_TOP        24
#define PERCPU_READY_QUEUE      32
#define PERCPU_READY_QUEUE_COUNT 32 + (8 * 256)
#define PERCPU_CURRENT_THREAD   PERCPU_READY_QUEUE_COUNT + 8
#define PERCPU_CONTEXT_SWITCHES PERCPU_CURRENT_THREAD + 8
#define PERCPU_IDLE_TICKS       PERCPU_CONTEXT_SWITCHES + 8
#define PERCPU_NEED_RESCHEDULE  PERCPU_IDLE_TICKS + 8
#define PERCPU_LOCAL_APIC       PERCPU_NEED_RESCHEDULE + 8
#define PERCPU_INTERRUPTS_HANDLED PERCPU_LOCAL_APIC + 8
#define PERCPU_IPI_RECEIVED     PERCPU_INTERRUPTS_HANDLED + 8
#define PERCPU_IPI_SENT         PERCPU_IPI_RECEIVED + 8
#define PERCPU_INIT_DONE        PERCPU_IPI_SENT + 8

/* percpu_data_t大小（用于计算per-CPU数组的偏移） */
#define PERCPU_SIZE             PERCPU_INIT_DONE + 4

.global x86_64_save_context
.type x86_64_save_context, @function

/* 保存x86_64上下文
 * 参数: rdi = 上下文结构指针
 */
x86_64_save_context:
    /* 保存通用寄存器 */
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rsi, 32(%rdi)
    movq %rbp, 40(%rdi)
    movq %rsp, 48(%rdi)
    movq %r8, 56(%rdi)
    movq %r9, 64(%rdi)
    movq %r10, 72(%rdi)
    movq %r11, 80(%rdi)
    movq %r12, 88(%rdi)
    movq %r13, 96(%rdi)
    movq %r14, 104(%rdi)
    movq %r15, 112(%rdi)

    /* 保存特殊寄存器 */
    movq (%rsp), %rdx      /* 返回地址 */
    movq %rdx, 120(%rdi)   /* RIP */
    pushfq
    popq %rdx
    movq %rdx, 128(%rdi)   /* RFLAGS */

    ret

.size x86_64_save_context, .-x86_64_save_context

.global x86_64_restore_context
.type x86_64_restore_context, @function

/* 恢复x86_64上下文
 * 参数: rdi = 上下文结构指针
 */
x86_64_restore_context:
    /* 恢复通用寄存器（除 rsp 外） */
    movq 0(%rdi), %rax
    movq 8(%rdi), %rbx
    movq 16(%rdi), %rcx
    movq 24(%rdi), %rdx
    movq 32(%rdi), %rsi       /* 恢复 rsi */
    movq 40(%rdi), %rbp
    movq 56(%rdi), %r8
    movq 64(%rdi), %r9
    movq 72(%rdi), %r10
    movq 80(%rdi), %r11
    movq 88(%rdi), %r12
    movq 96(%rdi), %r13
    movq 104(%rdi), %r14
    movq 112(%rdi), %r15

    /* 在原栈上恢复 RFLAGS（避免污染新栈） */
    movq 128(%rdi), %rdx
    pushq %rdx
    popfq

    /* 最后恢复 rsp 并跳转 */
    movq 48(%rdi), %rsp
    movq 120(%rdi), %rdx
    jmp *%rdx

.size x86_64_restore_context, .-x86_64_restore_context

.global context_switch
.type context_switch, @function

/* 外部函数：获取域页表 */
.extern domain_switch_get_pagetable
.extern domain_switch_set_current

/* 外部变量：全局域表 */
.extern g_domains

/* 域结构体偏移 (domain.h) */
#define DOMAIN_PAGE_TABLE_OFFSET 56  /* page_table 字段在 domain_t 中的偏移 */

/* 上下文切换函数（多核支持）
 * 参数: rdi = prev线程指针(thread_t*), rsi = next线程指针(thread_t*)
 *
 * thread_t 结构体偏移 (实际计算):
 *   0:  thread_id (u32, 4字节)
 *   4:  domain_id (u32, 4字节)
 *   8:  state (enum, 4字节)
 *   12: priority (u8, 1字节) + 3字节填充
 *   16: logical_core_id (u32, 4字节)
 *   20: core_affinity (u32, 4字节)
 *   24: stack_base (8字节)
 *   32: stack_size (8字节)
 *   40: stack_ptr (8字节)     <-- 这是我们要保存/恢复的
 *   48: arch_context (8字节)
 *   56: time_slice (8字节)
 *
 * 修复：添加 CR3 切换逻辑，确保跨域线程切换时正确切换页表
 */
context_switch:
    /* 检查 prev 是否为 NULL */
    testq %rdi, %rdi
    jz .L_first_switch

    /* ===== 保存当前线程上下文到 prev ===== */

    /* 保存调用者保存的寄存器 */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 保存栈指针到 prev->stack_ptr (偏移 40) */
    movq %rsp, 40(%rdi)
    
    jmp .L_switch_to_new

.L_first_switch:
    /* 首次调度：prev 为 NULL，不需要保存上下文
     * 但仍需保存返回地址，以便新线程完成后能返回到 schedule() 的调用者
     * 注意：此时 RSP 指向调用 context_switch 的返回地址
     */
    
    /* 保存 6 个 callee-saved 寄存器槽位（保持栈平衡）
     * 这些值在恢复时会被丢弃，但需要保持栈布局一致
     */
    subq $48, %rsp    /* 6 * 8 = 48 字节，为 callee-saved 寄存器预留空间 */

.L_switch_to_new:
    /* ===== CR3 切换逻辑（跨域线程切换）=====
     * 关键：在保存完当前线程上下文后、恢复新线程上下文前切换页表
     * 这样调度器 C 代码始终运行在 Core-0 页表下
     * 只有新线程开始执行时才运行在其私有页表中
     */
    
    /* 将 next 指针保存在 callee-saved 寄存器 %rbx 中
     * 避免 CR3 切换后访问旧栈（旧栈在新域页表中不可见） */
    movq %rsi, %rbx

    /* 获取 next 线程的 domain_id (偏移 4) */
    movl 4(%rbx), %edi           /* %edi = next->domain_id (zero-extended to 64-bit) */

    /* 检查域ID是否有效 */
    cmpq $256, %rdi              /* HIC_DOMAIN_MAX = 256 */
    jae .L_skip_cr3_switch       /* 无效域ID，跳过CR3切换 */

    /* 调用 domain_switch_get_pagetable(domain_id) */
    call domain_switch_get_pagetable

    /* 检查返回的页表是否有效 */
    testq %rax, %rax
    jz .L_skip_cr3_switch        /* NULL 页表，跳过切换（Core-0 域） */

    /* 在 %rax 中保留页表地址，使用 %rcx 和 %rdx 进行比较 */
    movq %cr3, %rcx
    movq $0xFFFFFFFFF000, %rdx   /* 页表地址掩码（高52位） */
    andq %rdx, %rcx              /* 清除低位标志位 */
    andq %rdx, %rax              /* 同样处理目标页表地址 */

    /* 比较是否需要切换 */
    cmpq %rcx, %rax
    je .L_skip_cr3_switch        /* 相同页表，无需切换 */

    /* 切换 CR3 - 新线程将运行在其私有页表中 */
    movq %rax, %cr3

.L_skip_cr3_switch:
    /* ===== 恢复线程上下文 =====
     * 通过 %rbx 访问 next 指针——该寄存器是 callee-saved，
     * CR3 切换后仍有效，且不涉及旧栈访问。 */

    /* 恢复栈指针从 next->stack_ptr (偏移 40) */
    movq 40(%rbx), %rsp

    /* 恢复调用者保存的寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    ret

.size context_switch, .-context_switch

/* x86_64_context_switch 别名 */
.global x86_64_context_switch
.set x86_64_context_switch, context_switch

/* ==================== 上下文初始化 ==================== */

/**
 * 初始化新线程上下文
 * 参数: rdi = ctx (上下文结构指针)
 *       rsi = entry (线程入口函数)
 *       rdx = stack (栈顶指针)
 */
.global x86_64_context_init
.type x86_64_context_init, @function
x86_64_context_init:
    /* 清零上下文结构 */
    pushq %rdi
    movq %rdi, %rcx
    movq $136 / 8, %r8         /* 上下文大小 / 8 */
    xorq %rax, %rax
1:
    movq %rax, (%rcx)
    addq $8, %rcx
    decq %r8
    jnz 1b
    popq %rdi

    /* 设置栈指针 (offset 48) */
    movq %rdx, 48(%rdi)        /* ctx->sp = stack */

    /* 设置指令指针 (offset 120) */
    movq %rsi, 120(%rdi)       /* ctx->pc = entry */

    /* 设置默认 RFLAGS (offset 128) - 启用中断 */
    movq $0x202, 128(%rdi)     /* IF=1, 保留位=1 */

    ret

.size x86_64_context_init, .-x86_64_context_init