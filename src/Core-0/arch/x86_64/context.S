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

/* 上下文切换函数（多核支持）
 * 参数: rdi = prev线程指针(thread_t*), rsi = next线程指针(thread_t*)
 *
 * thread_t 结构体偏移:
 *   0:  thread_id
 *   8:  domain_id
 *   16: state
 *   24: priority
 *   32: stack_base
 *   40: stack_size
 *   48: stack_ptr     <-- 这是我们要保存/恢复的
 *   56: arch_context
 */
context_switch:
    /* 检查 prev 是否为 NULL */
    testq %rdi, %rdi
    jz .L_switch_to_new

    /* 保存当前线程上下文到 prev */

    /* 保存调用者保存的寄存器 */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 保存栈指针到 prev->stack_ptr (偏移 48) */
    movq %rsp, 48(%rdi)

.L_switch_to_new:
    /* 恢复next线程上下文 */

    /* 恢复栈指针从 next->stack_ptr (偏移 48) */
    movq 48(%rsi), %rsp

    /* 恢复调用者保存的寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    ret

.size context_switch, .-context_switch