/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC性能优化的中断处理（x86_64 专用）
 * 目标延迟：0.5-1微秒
 * 
 * FSGSBASE 优化：
 * - 如果 CPU 支持 FSGSBASE，使用 rdfsbase/wrfsbase 指令
 * - 否则回退到 MSR 访问（rdmsr/wrmsr）
 * - FSGSBASE: ~10 周期 vs MSR: ~100-200 周期
 * 
 * 注意：此文件是 x86_64 专用
 * - ARM64: 见 arch/arm64/ 目录
 * - RISC-V: 见 arch/riscv/ 目录
 */

/* FSGSBASE 全局标志（在 hardware_probe_arch.c 中定义） */
.global g_use_fsgsbase

.section .text

/* ==================== IRQ 入口存根宏 ==================== */
/**
 * 为每个 IRQ 创建单独的入口点
 * 必须先压入向量号，因为硬件中断不会自动压入向量号
 */
.macro IRQ_ENTRY num
.global irq_\num
irq_\num:
    pushq $\num      /* 压入 IRQ 向量号 */
    jmp isr_fast_stub
.endm

/* IRQ 入口点（向量 32-47） */
IRQ_ENTRY 32
IRQ_ENTRY 33
IRQ_ENTRY 34
IRQ_ENTRY 35
IRQ_ENTRY 36
IRQ_ENTRY 37
IRQ_ENTRY 38
IRQ_ENTRY 39
IRQ_ENTRY 40
IRQ_ENTRY 41
IRQ_ENTRY 42
IRQ_ENTRY 43
IRQ_ENTRY 44
IRQ_ENTRY 45
IRQ_ENTRY 46
IRQ_ENTRY 47

/* ==================== 快速中断处理存根 ==================== */
/* 快速中断处理存根（仅保存必要寄存器）
 * 注意：调用前必须已压入向量号（见上面的 IRQ_ENTRY 宏）
 * 
 * 重要：此存根仅用于 IRQ（硬件中断），不用于异常！
 * 异常使用 interrupts.S 中的 isr_common_stub，支持错误码。
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
     * 
     * 栈布局（从高地址到低地址，即从栈底到栈顶）：
     * 
     *   高地址 ─────────────────────────────
     *   RFLAGS        <- CPU压入（中断发生时）
     *   CS            <- CPU压入
     *   RIP           <- CPU压入（中断返回地址）
     *   向量号        <- IRQ_ENTRY宏压入 (pushq $num)
     *   RAX           <- pushq %rax
     *   RBX           <- pushq %rbx
     *   RCX           <- pushq %rcx
     *   RDX           <- pushq %rdx
     *   RSI           <- pushq %rsi
     *   RDI           <- pushq %rdi
     *   RBP           <- pushq %rbp <-- 当前 RSP
     *   低地址 ─────────────────────────────
     *
     * 偏移计算：
     *   向量号 = RSP + 7*8 = RSP + 56
     * 
     * 警告：若修改压入的寄存器数量，必须同步更新偏移！
     * 定义常量可避免此问题，但会增加代码复杂度。
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

/* 系统调用快速路径
 * 
 * 修复：添加FS基址保存/恢复，支持用户态TLS
 * 
 * syscall指令后的寄存器状态：
 *   RAX = 系统调用号（不变）
 *   RCX = 用户空间返回地址（被syscall保存）
 *   R11 = 用户空间RFLAGS（被syscall保存）
 *   RDI, RSI, RDX, R10, R8, R9 = 系统调用参数
 *
 * FS寄存器：用户态TLS基址，需要保存/恢复
 * GS寄存器：通过swapgs切换内核/用户基址
 */
.global syscall_fast_entry
syscall_fast_entry:
    /* 交换GS基址 */
    swapgs

    /* ===== 第一步：保存用户态关键寄存器 ===== */
    /* 使用栈帧布局：
     *   [FS基址]       <- RSP+0 (8字节)
     *   [RCX用户地址]  <- RSP+8
     *   [R11用户标志]  <- RSP+16
     *   [RBX]          <- RSP+24
     *   [RBP]          <- RSP+32
     *   [R12]          <- RSP+40
     *   [R13]          <- RSP+48
     *   [R14]          <- RSP+56
     *   [R15]          <- RSP+64
     */
    
    /* 分配栈空间：FS基址(8) + RCX(8) + R11(8) + callee-saved(48) = 72字节 */
    subq $72, %rsp
    
    /* 保存FS基址（用户态TLS）- FSGSBASE 优化路径
     * 注意：使用 R8 作为临时寄存器，避免覆盖 RAX（系统调用号）
     */
    movq g_use_fsgsbase(%rip), %r8
    testq %r8, %r8
    jz .fs_save_msr
    
    /* FSGSBASE 路径：~10 周期 */
    rdfsbase %r8
    movq %r8, 0(%rsp)
    jmp .fs_save_done
    
.fs_save_msr:
    /* MSR 路径：~100-200 周期（兼容旧 CPU）
     * 注意：rdmsr 使用 EDX:EAX，需要先保存 RAX（系统调用号）
     */
    movq %rax, 72(%rsp)         /* 临时保存系统调用号到栈顶之上 */
    movl $0xC0000100, %ecx      /* IA32_FS_BASE MSR */
    rdmsr                       /* EDX:EAX = FS base */
    movl %eax, 0(%rsp)          /* 保存低32位 */
    movl %edx, 4(%rsp)          /* 保存高32位 */
    movq 72(%rsp), %rax         /* 恢复系统调用号 */
    
.fs_save_done:
    /* 保存用户态返回地址和RFLAGS */
    movq %rcx, 8(%rsp)          /* 用户空间返回地址 */
    movq %r11, 16(%rsp)         /* 用户空间RFLAGS */
    
    /* 保存被调用者保存的寄存器 */
    movq %rbx, 24(%rsp)
    movq %rbp, 32(%rsp)
    movq %r12, 40(%rsp)
    movq %r13, 48(%rsp)
    movq %r14, 56(%rsp)
    movq %r15, 64(%rsp)

    /* ===== 第二步：准备调用C处理函数 ===== */
    /* 分配参数结构体空间 */
    subq $64, %rsp              /* 64字节用于参数结构体 */
    
    /* 保存系统调用参数到结构体 */
    movq %rax, 0(%rsp)          /* 系统调用号 */
    movq %rdi, 8(%rsp)          /* 参数1 */
    movq %rsi, 16(%rsp)         /* 参数2 */
    movq %rdx, 24(%rsp)         /* 参数3 */
    movq %r10, 32(%rsp)         /* 参数4 */
    movq %r8,  40(%rsp)         /* 参数5 */
    movq %r9,  48(%rsp)         /* 参数6 */
    
    /* 将结构体指针传给C函数 */
    movq %rsp, %rdi
    
    /* 调用系统调用处理函数 */
    call syscall_handler
    
    /* ===== 第三步：恢复并返回用户态 ===== */
    /* 返回值在RAX中 */
    
    /* 释放参数结构体 */
    addq $64, %rsp
    
    /* 恢复被调用者保存的寄存器 */
    movq 24(%rsp), %rbx
    movq 32(%rsp), %rbp
    movq 40(%rsp), %r12
    movq 48(%rsp), %r13
    movq 56(%rsp), %r14
    movq 64(%rsp), %r15
    
    /* 恢复用户态返回地址和RFLAGS */
    movq 8(%rsp), %rcx          /* 用户空间返回地址 */
    movq 16(%rsp), %r11         /* 用户空间RFLAGS */
    
    /* 恢复FS基址（用户态TLS）- FSGSBASE 优化路径 */
    movq g_use_fsgsbase(%rip), %r8
    testq %r8, %r8
    jz .fs_restore_msr
    
    /* FSGSBASE 路径：~10 周期 */
    movq 0(%rsp), %r8
    wrfsbase %r8
    jmp .fs_restore_done
    
.fs_restore_msr:
    /* MSR 路径：~100-200 周期（兼容旧 CPU）
     * 注意：wrmsr 使用 EDX:EAX，但此时 RAX 已经是返回值，需要保存
     */
    pushq %rax                  /* 保存返回值 */
    movl 0(%rsp), %eax          /* 低32位（偏移已调整） */
    movl 4(%rsp), %edx          /* 高32位 */
    movl $0xC0000100, %ecx      /* IA32_FS_BASE MSR */
    wrmsr
    popq %rax                   /* 恢复返回值 */
    
.fs_restore_done:
    /* 释放栈空间 */
    addq $72, %rsp
    
    /* 恢复GS基址 */
    swapgs
    
    /* 返回用户空间 */
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