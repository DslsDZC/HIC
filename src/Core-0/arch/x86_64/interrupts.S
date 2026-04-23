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
    push $0        /* 压入错误码占位符（无错误码异常需要此占位符） */
    push $\num    /* 压入中断向量号 */
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr_\num
isr_\num:
    /* 有错误码的异常：CPU已自动压入错误码，不需要再压入占位符 */
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
/* Page fault — custom handler with IPC 3.0 support */
.global isr_14
isr_14:
    push $14        /* 压入中断向量号 */
    jmp isr_14_handler

isr_14_handler:
    /* 保存所有通用寄存器（与 isr_common_stub 布局一致） */
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

    /* 读取 CR2（缺页地址） */
    mov %cr2, %rax

    /* 调用 ipc3_handle_pf(cr2, err_code, regs_ptr) */
    mov %rax, %rdi               /* arg1: CR2 */
    mov 128(%rsp), %rsi          /* arg2: error code (RSP+128) */
    mov %rsp, %rdx               /* arg3: 指向保存的寄存器数组 */
    call ipc3_handle_pf

    test %al, %al                /* IPC3 是否处理了此次缺页？ */
    jnz .ipc3_pf_handled

    /* 非 IPC3 缺页 — 进入正常分发 */
    mov 120(%rsp), %rdi          /* arg1: 中断向量号 */
    call irq_dispatch
    jmp .ipc3_pf_restore

.ipc3_pf_handled:
    /* IPC3 处理了缺页 — 加载待切换的 CR3 */
    movq g_ipc3_pending_cr3(%rip), %rax
    test %rax, %rax
    jz .ipc3_pf_restore          /* 安全保护 */
    mov %rax, %cr3               /* 切换到服务域页表 */
    movq $0, g_ipc3_pending_cr3(%rip)

.ipc3_pf_restore:
    /* 恢复寄存器（可能已被 ipc3_handle_pf 修改） */
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

    /* 清除中断向量号和错误码 */
    add $16, %rsp

    iretq
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

    /* 从栈中获取中断向量号（在保存的寄存器之后，偏移 15*8 = 120 字节） */
    mov 120(%rsp), %rdi    /* 获取中断向量号 */
    call irq_dispatch

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
    /* 保存通用寄存器 */
    movq %rax, 0(%rdi)
    movq %rbx, 8(%rdi)
    movq %rcx, 16(%rdi)
    movq %rdx, 24(%rdi)
    movq %rdi, 32(%rdi)   /* 保存 RDI 本身 */
    movq %rsi, 40(%rdi)
    movq %rbp, 48(%rdi)
    movq %rsp, 56(%rdi)
    movq %r8,  64(%rdi)
    movq %r9,  72(%rdi)
    movq %r10, 80(%rdi)
    movq %r11, 88(%rdi)
    movq %r12, 96(%rdi)
    movq %r13, 104(%rdi)
    movq %r14, 112(%rdi)
    movq %r15, 120(%rdi)

    /* 保存返回地址（在栈顶） */
    movq (%rsp), %rax
    movq %rax, 128(%rdi)

    /* 保存 RFLAGS */
    pushfq
    popq %rax
    movq %rax, 136(%rdi)

    ret

/* 上下文恢复函数（由HAL调用）
 * 参数: %rdi = 上下文结构指针
 */
.global arch_restore_context
arch_restore_context:
    /* 使用 RAX、RBX、RCX 作为临时寄存器 */
    movq 56(%rdi), %rax     /* RSP */
    movq 128(%rdi), %rbx    /* RIP */
    movq 32(%rdi), %rcx     /* RDI */

    /* 恢复所有通用寄存器（除 RDI、RSP、RAX、RBX、RCX 外）*/
    movq 8(%rdi), %rdx
    movq 40(%rdi), %rsi
    movq 48(%rdi), %rbp
    movq 64(%rdi), %r8
    movq 72(%rdi), %r9
    movq 80(%rdi), %r10
    movq 88(%rdi), %r11     /* 恢复 R11 */
    movq 96(%rdi), %r12     /* 恢复 R12 */
    movq 104(%rdi), %r13    /* 恢复 R13 */
    movq 112(%rdi), %r14
    movq 120(%rdi), %r15

    /* 恢复 RFLAGS（在原栈上） */
    movq 136(%rdi), %rdx
    pushq %rdx
    popfq

    /* 最后设置 RSP、RDI 并跳转 */
    movq %rax, %rsp
    movq %rcx, %rdi
    jmp *%rbx

/* ==================== IPC 3.0 Return Trampoline ==================== */
/*
 * Called from ipc3_return() after g_ipc3_return_ctx is set.
 * Restores full caller context from saved context:
 *   1. Restores callee-saved regs (r15-r8, rbp)
 *   2. Switches CR3 back to caller's page table
 *   3. Builds synthetic register frame on caller's stack
 *   4. Pops all caller registers from synthetic frame
 *   5. Returns to instruction after original call [entry_page]
 *
 * This function never returns — resumes in caller domain.
 */
.global ipc3_return_asm
ipc3_return_asm:
    cli

    /* Load return context pointer into rbx */
    movq g_ipc3_return_ctx(%rip), %rbx

    /* Restore r15-r8, rbp */
    movq 0(%rbx), %r15
    movq 8(%rbx), %r14
    movq 16(%rbx), %r13
    movq 24(%rbx), %r12
    movq 32(%rbx), %r11
    movq 40(%rbx), %r10
    movq 48(%rbx), %r9
    movq 56(%rbx), %r8
    movq 64(%rbx), %rbp

    /* Switch CR3 to caller's page table */
    movq 160(%rbx), %rax
    movq %rax, %cr3

    /* Get caller's original RSP (return address on top) */
    movq 144(%rbx), %rax

    /* Point RSP below return address for 6-register synthetic frame */
    subq $48, %rax
    movq %rax, %rsp

    /* Write register values to synthetic frame (rdi, rsi, rdx, rcx, rbx, rax) */
    movq 72(%rbx), %rax
    movq %rax, 0(%rsp)
    movq 80(%rbx), %rax
    movq %rax, 8(%rsp)
    movq 88(%rbx), %rax
    movq %rax, 16(%rsp)
    movq 96(%rbx), %rax
    movq %rax, 24(%rsp)
    movq 104(%rbx), %rax
    movq %rax, 32(%rsp)
    movq 112(%rbx), %rax
    movq %rax, 40(%rsp)

    /* Pop all registers from synthetic frame */
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    /* Return to caller (pops return address from original call [entry_page]) */
    ret
