/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64上下文切换汇编实现
 */

.section .text

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
    leaq 120(%rdi), %rax
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
    /* 恢复通用寄存器 */
    movq 0(%rdi), %rax
    movq 8(%rdi), %rbx
    movq 16(%rdi), %rcx
    movq 24(%rdi), %rdx
    movq 40(%rdi), %rbp
    movq 48(%rdi), %rsp
    movq 56(%rdi), %r8
    movq 64(%rdi), %r9
    movq 72(%rdi), %r10
    movq 80(%rdi), %r11
    movq 88(%rdi), %r12
    movq 96(%rdi), %r13
    movq 104(%rdi), %r14
    movq 112(%rdi), %r15

    /* 恢复RFLAGS */
    movq 128(%rdi), %rdx
    pushq %rdx
    popfq

    /* 跳转到保存的RIP */
    movq 120(%rdi), %rdx
    jmp *%rdx

.size x86_64_restore_context, .-x86_64_restore_context

.global context_switch
.type context_switch, @function

/* 上下文切换函数
 * 参数: rdi = prev线程上下文指针, rsi = next线程上下文指针
 */
context_switch:
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

    /* 保存返回地址到 prev */
    movq 8(%rsp), %rax
    movq %rax, 8(%rdi)

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

.size context_switch, .-context_switch