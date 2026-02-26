/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC性能优化的中断处理（优化版）
 * 目标延迟：0.5-1微秒
 */

/* 快速中断处理存根（仅保存必要寄存器） */
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
    
    /* 从栈中获取IRQ向量号 */
    movq 8(%rsp), %rdi  /* 栈上第9个qword是向量号 */
    
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
    
    iretq

/* 系统调用快速路径 */
.global syscall_fast_entry
syscall_fast_entry:
    /* 使用syscall/sysret指令优化 */
    /* 寄存器约定：
     * RAX = 系统调用号
     * RDI = 参数1
     * RSI = 参数2
     * RDX = 参数3
     * RCX = 用户空间返回地址（由syscall指令自动保存）
     * R11 = RFLAGS（由syscall指令自动保存）
     */
    
    /* 保存RCX和R11（syscall指令会改变它们） */
    pushq %rcx
    pushq %r11
    
    /* 调用系统调用处理函数 */
    call syscall_handler
    
    /* 恢复RCX和R11 */
    popq %r11
    popq %rcx
    
    /* 返回用户空间 */
    sysretq

/* 性能优化的上下文切换 */
.global context_switch_fast
context_switch_fast:
    /* 仅切换必要寄存器 */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* 调用调度器 */
    call schedule
    
    /* 恢复新线程的寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    ret