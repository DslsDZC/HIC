/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

# HIC内核入口点（x86_64）
# Bootloader跳转到这个入口点

.section .text.start
.global _start
.global kernel_entry

.extern kernel_start

/* boot_info 结构体偏移常量 */
.set BOOT_INFO_OFFSET_STACK_TOP, 0x698

/* UART 寄存器常量 */
.set UART_LSR, 0x3FD        /* 线路状态寄存器 */
.set UART_THR, 0x3F8        /* 发送保持寄存器 */
.set UART_THRE, 0x20        /* 发送保持寄存器空标志 */

# 内核入口点
# 参数: %rdi = hic_boot_info_t* boot_info
_start:
kernel_entry:
    # 保存调用者上下文（在串口输出之前）
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    # 保存boot_info指针
    mov %rdi, %r15

    # 输出调试信息：'K' 表示内核入口
    mov $'K', %al
    call .serial_putchar
    
    # 输出调试信息：'1' 表示检查boot_info指针
    mov $'1', %al
    call .serial_putchar
    
    # 检查boot_info指针是否为0
    test %r15, %r15
    jnz .boot_info_valid
    
    # boot_info指针为0，输出'E'表示错误
    mov $'E', %al
    call .serial_putchar
    jmp .kernel_panic_loop

.boot_info_valid:
    # 输出调试信息：'2' 表示加载GDT
    mov $'2', %al
    call .serial_putchar
    
    # 加载 GDT
    lgdt gdt_ptr
    
    # 输出调试信息：'3' 表示设置栈指针
    mov $'3', %al
    call .serial_putchar
    
    # 重新设置栈指针（使用boot_info中的栈）
    mov %r15, %rdi
    mov BOOT_INFO_OFFSET_STACK_TOP(%rdi), %rsp
    
    # 栈指针对齐（16字节对齐，符合x86-64 ABI）
    and $~0xF, %rsp
    
    # 输出调试信息：'4' 表示跳转到kernel_start
    mov $'4', %al
    call .serial_putchar
    
    # 调用C语言内核入口点
    mov %r15, %rdi        # boot_info作为第一个参数
    call kernel_start
    
    # 如果kernel_start返回，输出错误信息并停止
    cli                   # 禁用中断
.kernel_panic_loop:
    hlt                   # 停止CPU
    jmp .kernel_panic_loop

# 串口输出字符函数（内联）
# 输入: %al = 字符
.serial_putchar:
    push %rdx
    push %rax
    mov $UART_LSR, %dx        # 串口线路状态寄存器
.serial_putchar_wait:
    in %dx, %al               # 读取状态
    test $UART_THRE, %al      # 检查发送保持寄存器是否为空
    jz .serial_putchar_wait
    mov $UART_THR, %dx        # 串口数据寄存器
    pop %rax                  # 恢复字符
    out %al, %dx              # 输出字符
    pop %rdx
    ret

/* 数据段 */
.section .data

/* GDT指针（完整实现） */
gdt_ptr:
    .word gdt_end - gdt - 1  /* GDT限制 */
    .quad gdt                /* GDT基址 */

/* GDT（完整实现） */
gdt:
    .quad 0x0000000000000000                    /* 0: NULL描述符 */
    .quad 0x00af9a000000ffff                    /* 1: 内核代码段（64位） */
    .quad 0x00cf92000000ffff                    /* 2: 内核数据段 */
    .quad 0x00cffa000000ffff                    /* 3: 用户代码段（64位） */
    .quad 0x00cff2000000ffff                    /* 4: 用户数据段 */
    .quad 0x0000000000000000                    /* 5: TSS描述符（占位） */
gdt_end: