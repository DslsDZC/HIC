/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC性能优化的中断处理（优化版）
 * 目标延迟：0.5-1微秒
 */

/* 快速中断处理存根（仅保存必要寄存器）
 * 注意：每个中断入口必须先压入向量号，然后调用此存根
 * 示例：
 *   push $0x20  ; 中断向量号
 *   jmp isr_fast_stub
 */
.global isr_fast_stub
isr_fast_stub:
    /* 仅保存关键寄存器（快速路径） */
    pushq %rax           /* RAX: 系统调用号/返回值 */
    pushq %rbx           /* RBX: 被调用者保存 */
    pushq %rcx           /* RCX: 用于iretq返回地址 */
    pushq %rdx           /* RDX: 参数 */
    pushq %rsi           /* RSI: 参数 */
    pushq %rdi           /* RDI: 参数 */
    pushq %rbp           /* RBP: 栈指针 */

    /* 从栈中获取IRQ向量号
     * 栈布局（从下到上）：
     *   [向量号]       <- 被中断入口压入
     *   [返回地址]     <- CPU压入
     *   [RFLAGS]       <- CPU压入
     *   [CS]           <- CPU压入
     *   [RIP]          <- CPU压入
     *   [错误码]       <- 可选（某些异常）
     *   [RAX]          <- pushq %rax
     *   [RBX]          <- pushq %rbx
     *   [RCX]          <- pushq %rcx
     *   [RDX]          <- pushq %rdx
     *   [RSI]          <- pushq %rsi
     *   [RDI]          <- pushq %rdi
     *   [RBP]          <- pushq %rbp
     *   [RSP]          <- 当前栈顶
     *
     * 向量号位于 RSP + 7*8 = RSP + 56
     */
    movq 56(%rsp), %rdi

    /* 调用中断分发函数 */
    call irq_dispatch

    /* 恢复寄存器 */
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* 恢复向量号（弹栈，iretq时CPU会处理剩余的RIP, CS, RFLAGS） */
    addq $8, %rsp

    iretq

/* 系统调用快速路径 */
.global syscall_fast_entry
syscall_fast_entry:
    /* 使用syscall/sysret指令优化 */
    /* 寄存器约定（syscall指令进入后）：
     * RAX = 系统调用号
     * RDI = 参数1
     * RSI = 参数2
     * RDX = 参数3
     * R10 = 参数4（不是RCX！）
     * R8  = 参数5
     * R9  = 参数6
     * RCX = 用户空间返回地址（由syscall指令自动保存）
     * R11 = RFLAGS（由syscall指令自动保存）
     */

    /* 保存被调用者保存的寄存器 */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 保存syscall特殊寄存器 */
    pushq %rcx           /* 用户空间返回地址 */
    pushq %r11           /* 用户空间RFLAGS */

    /* 传递参数给C函数
     * 方法：在栈上创建寄存器快照，然后传递结构体指针
     * 这样避免寄存器冲突，且简化C函数实现
     */
    subq $64, %rsp       /* 分配64字节栈空间 */

    /* 保存所有系统调用相关寄存器到栈 */
    movq %rax, 0(%rsp)   /* 系统调用号 */
    movq %rdi, 8(%rsp)   /* 参数1 */
    movq %rsi, 16(%rsp)  /* 参数2 */
    movq %rdx, 24(%rsp)  /* 参数3 */
    movq %r10, 32(%rsp)  /* 参数4 */
    movq %r8,  40(%rsp)  /* 参数5 */
    movq %r9,  48(%rsp)  /* 参数6 */

    /* 将栈指针作为参数传递给C函数 */
    movq %rsp, %rdi

    /* 调用系统调用处理函数
     * C函数原型：void syscall_handler(void *regs)
     */
    call syscall_handler

    /* 释放栈空间 */
    addq $64, %rsp

    /* 恢复syscall特殊寄存器 */
    popq %r11
    popq %rcx

    /* 恢复被调用者保存的寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    /* 返回值在RAX中，返回用户空间 */
    sysretq

/* 快速上下文切换
 * 真正的线程切换实现，类似于 context.S 中的 context_switch
 * 参数:
 *   %rdi = prev 线程上下文指针
 *   %rsi = next 线程上下文指针
 */
.global context_switch_fast
context_switch_fast:
    /* 保存当前线程上下文到 prev */

    /* 保存调用者保存的寄存器 */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* 保存栈指针到 prev */
    movq %rsp, (%rdi)

    /* 保存返回地址到 prev（偏移 48，因为压入了 6 个寄存器） */
    movq 48(%rsp), %rax
    movq %rax, 8(%rdi)

    /* 调用调度器选择下一个线程 */
    call schedule
    /* 注意：schedule 返回后，%rsi 应该指向新的线程上下文 */

    /* 恢复next线程上下文 */

    /* 恢复栈指针 */
    movq (%rsi), %rsp

    /* 恢复返回地址 */
    movq 8(%rsi), %rax
    movq %rax, 8(%rsp)

    /* 恢复调用者保存的寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    ret