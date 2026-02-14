/**
 * x86-64上下文切换汇编实现
 */

.section .text

.global context_switch
.type context_switch, @function

/* 上下文切换函数
 * 参数: rdi = prev线程, rsi = next线程
 */
context_switch:
    /* 保存当前线程上下文 */
    
    /* 保存通用寄存器 */
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* 保存调用者保存的寄存器 */
    movq %rdi, (%rsi)    /* 保存prev指针到next->prev */
    
    /* 恢复next线程上下文 */
    
    /* 恢复调用者保存的寄存器 */
    movq (%rsi), %rdi    /* 恢复next->prev */
    
    /* 恢复通用寄存器 */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    
    /* 恢复栈指针 */
    movq 120(%rsi), %rsp  /* 从TCB恢复rsp */
    
    /* 跳转到下一个线程 */
    movq 112(%rsi), %rip  /* 从TCB恢复rip */
    jmp *%rax

.size context_switch, .-context_switch