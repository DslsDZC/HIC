/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64 中断处理程序
 */

.section .text

/* 中断处理程序存根 */
.macro ISR_NOERRCODE num
.global isr_\num
isr_\num:
    push $0        /* 压入错误码占位符 */
    push $\num    /* 压入中断向量号 */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr_\num
isr_\num:
    push $\num    /* 压入中断向量号 */
    jmp isr_common_stub
.endm

/* 异常处理程序 */
ISR_NOERRCODE 0   /* Division by zero */
ISR_NOERRCODE 1   /* Debug */
ISR_NOERRCODE 2   /* Non-maskable Interrupt */
ISR_NOERRCODE 3   /* Breakpoint */
ISR_NOERRCODE 4   /* Into detected overflow */
ISR_NOERRCODE 5   /* Out of bounds */
ISR_NOERRCODE 6   /* Invalid opcode */
ISR_NOERRCODE 7   /* No coprocessor */
ISR_ERRCODE   8   /* Double fault */
ISR_NOERRCODE 9   /* Coprocessor segment overrun */
ISR_ERRCODE   10  /* Bad TSS */
ISR_ERRCODE   11  /* Segment not present */
ISR_ERRCODE   12  /* Stack fault */
ISR_ERRCODE   13  /* General protection fault */
ISR_ERRCODE   14  /* Page fault */
ISR_NOERRCODE 15  /* Unknown interrupt */
ISR_NOERRCODE 16  /* Coprocessor fault */
ISR_ERRCODE   17  /* Alignment check */
ISR_NOERRCODE 18  /* Machine check */
ISR_NOERRCODE 19  /* SIMD floating-point exception */
ISR_NOERRCODE 20  /* Virtualization exception */
ISR_NOERRCODE 30  /* Security exception */

/* 系统调用 */
ISR_NOERRCODE 128  /* System call */

/* 中断处理公共存根 */
isr_common_stub:
    /* 保存所有寄存器 */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* 调用C语言中断处理函数 */
    mov %rsp, %rdi    /* 传递栈指针作为参数 */
    call irq_handler

    /* 恢复寄存器 */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    /* 恢复错误码和中断向量号 */
    add $16, %rsp

    /* 返回 */
    iretq

/* TSS 加载函数 */
.global tss_load
tss_load:
    mov %di, %ax
    ltr %ax
    ret

/* IDT 加载函数 */
.global idt_load
idt_load:
    lidt (%rdi)
    ret

/* 上下文保存函数（由HAL调用） */
.global arch_save_context
arch_save_context:
    /* 保存寄存器到context结构 */
    mov %rdi, %rsi          /* context指针 */
    
    /* 保存通用寄存器 */
    push %rax
    popq 0(%rsi)
    push %rbx
    popq 8(%rsi)
    push %rcx
    popq 16(%rsi)
    push %rdx
    popq 24(%rsi)
    push %rsi
    push %rdi
    popq 40(%rsi)
    push %rbp
    popq 48(%rsi)
    push %r8
    popq 56(%rsi)
    push %r9
    popq 64(%rsi)
    push %r10
    popq 72(%rsi)
    push %r11
    popq 80(%rsi)
    push %r12
    popq 88(%rsi)
    push %r13
    popq 96(%rsi)
    push %r14
    popq 104(%rsi)
    push %r15
    popq 112(%rsi)
    
    /* 保存RSP（跳过返回地址） */
    mov (%rsp), %rax        /* 保存返回地址 */
    mov %rax, 120(%rsi)
    
    /* 保存RFLAGS */
    pushfq
    pop %rax
    mov %rax, 128(%rsi)
    
    ret

/* 上下文恢复函数（由HAL调用） */
.global arch_restore_context
arch_restore_context:
    mov %rdi, %rsi          /* context指针 */
    
    /* 恢复RFLAGS */
    mov 128(%rsi), %rax
    push %rax
    popfq
    
    /* 恢复RIP */
    mov 120(%rsi), %rax
    
    /* 恢复RSP（调整栈指针） */
    mov %rsi, %rdx
    add $8, %rdx           /* 跳过返回地址 */
    
    /* 恢复通用寄存器 */
    movq 112(%rsi), %r15
    movq 104(%rsi), %r14
    movq 96(%rsi), %r13
    movq 88(%rsi), %r12
    movq 80(%rsi), %r11
    movq 72(%rsi), %r10
    movq 64(%rsi), %r9
    movq 56(%rsi), %r8
    movq 48(%rsi), %rbp
    movq 40(%rsi), %rdi
    movq 32(%rsi), %rsi
    movq 24(%rsi), %rdx
    movq 16(%rsi), %rcx
    movq 8(%rsi), %rbx
    movq 0(%rsi), %rax
    
    /* 跳转到保存的RIP */
    ret
