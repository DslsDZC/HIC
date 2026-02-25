/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64中断处理汇编存根
 */

.section .text

/* 中断处理存根 */
.macro ISR_NOERRCODE num
.global isr_\num
isr_\num:
    pushq $0          /* 错误码占位符（用于统一堆栈布局） */
    pushq $\num       /* 中断向量 */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr_\num
isr_\num:
    /* CPU自动压入错误码，不需要手动压入 */
    pushq $\num       /* 中断向量 */
    jmp isr_common_stub
.endm

/* 异常处理程序 */
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

/* IRQ处理程序 (32-47) */
.set irq, 32
.rept 16
ISR_NOERRCODE irq
.set irq, irq+1
.endr

/* 系统调用 (128) */
.global isr_128
isr_128:
    pushq $0
    pushq $128
    jmp isr_common_stub

/* 系统调用处理（特殊处理） */
.global isr_128_stub
isr_128_stub:
    /* 系统调用从用户态到内核态，需要切换栈 */
    /* 完整实现：系统调用上下文切换 */
    
    /* 保存用户栈指针 */
    mov %rsp, %r10
    and $-16, %rsp       /* 对齐栈 */
    
    /* 保存所有寄存器 */
    pushq %rax           /* RAX: 系统调用号 */
    pushq %rbx           /* RBX: 参数1 */
    pushq %rcx           /* RCX: 参数2 */
    pushq %rdx           /* RDX: 参数3 */
    pushq %rsi           /* RSI: 参数4 */
    pushq %rdi           /* RDI: 参数5 */
    pushq %r8            /* R8: 参数6 */
    pushq %r9            /* R9: 保留 */
    pushq %r10           /* R10: 用户栈指针 */
    pushq %r11           /* R11: RFLAGS */
    pushq %r12           /* R12: 保留 */
    pushq %r13           /* R13: 保留 */
    pushq %r14           /* R14: 保留 */
    pushq %r15           /* R15: 保留 */
    
    /* 加载内核栈 */
    mov %rsp, %r12       /* 保存当前栈指针 */
    mov tss_rsp0(%rip), %rsp  /* 切换到内核栈 */
    
    /* 调用系统调用处理函数 */
    mov %r12, %rdi       /* 传递寄存器保存区域指针 */
    call syscall_handler
    
    /* 恢复寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    /* 恢复用户栈 */
    mov %rax, %rsp       /* RAX中应该是用户栈指针 */
    
    /* 返回用户态 */
    iretq

/* 通用中断处理存根 */
.global isr_common_stub
isr_common_stub:
    /* 保存所有寄存器 */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* 保存数据段 */
    movw %ds, %ax
    pushq %rax
    movw %es, %ax
    pushq %rax
    movw %fs, %ax
    pushq %rax
    movw %gs, %ax
    pushq %rax
    
    /* 加载内核数据段 */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    /* 调用C处理函数 */
    movq %rsp, %rdi    /* 传递栈指针 */
    call interrupt_handler
    
    /* 恢复数据段 */
    popq %rax
    movw %ax, %gs
    popq %rax
    movw %ax, %fs
    popq %rax
    movw %ax, %es
    popq %rax
    movw %ax, %ds
    
    /* 恢复所有寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    /* 清理错误码和向量 */
    addq $16, %rsp
    
    iretq