# HIK内核入口点（x86_64）
# Bootloader跳转到这个入口点

.section .text
.global _start
.global kernel_entry

.extern kernel_start

# 内核入口点
# 参数: %rdi = hik_boot_info_t* boot_info
_start:
kernel_entry:
    # 保存调用者上下文
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    # 保存boot_info指针
    mov %rdi, %r15

    # 设置新的GDT（如果需要）
    # lgdt gdt_ptr

    # 设置新的段选择器
    # mov $0x10, %ax      # 数据段选择器
    # mov %ax, %ds
    # mov %ax, %es
    # mov %ax, %fs
    # mov %ax, %gs
    # mov %ax, %ss

    # 清除BSS段
    # call clear_bss

    # 调用C语言内核入口点
    mov %r15, %rdi        # boot_info作为第一个参数
    call kernel_start
    
    # 如果kernel_start返回，进入无限循环
.kernel_halt:
    cli
    hlt
    jmp .kernel_halt

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