/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64 AP（应用处理器）启动代码（AMP架构）
 * AP不运行完整内核，只运行简单的任务处理器
 * 必须加载到1MB以下的物理地址
 * 
 * 修复：动态地址计算
 * - 不再硬编码 0x8000 地址
 * - 使用 BSP 设置的动态基址
 */

.intel_syntax noprefix

.section .ap_trampoline, "ax"
.global ap_trampoline
.type ap_trampoline, @function

/**
 * AP启动入口点（16位实模式）
 * 
 * 修复说明：
 * 原代码使用 lea eax, [gdt_ptr - ap_trampoline + 0x8000]
 * 假设代码被复制到固定地址 0x8000。
 * 
 * 新方案：
 * BSP 在复制代码后，会设置以下动态变量：
 * - trampoline_base: 实际加载地址
 * - gdt_ptr_offset: GDT指针相对于加载地址的偏移
 */
.Code16
ap_trampoline:
    cli
    cld

    /* ===== 动态地址计算（修复硬编码问题）===== */
    /* 方法：使用相对调用获取当前IP */
    call .L_get_ip
.L_get_ip:
    pop ax                      /* AX = 当前IP (.L_get_ip 的地址) */
    
    /* 计算代码段基址 */
    /* 当前IP = ap_trampoline中的偏移 + 实际加载地址 */
    /* trampoline_base 在运行时由BSP设置 */
    
    /* 加载预先计算的GDT指针地址 */
    /* BSP会在复制代码后，计算 gdt_ptr 的实际物理地址并写入 gdt_ptr_addr */
    mov eax, [.L_gdt_ptr_addr - ap_trampoline + .L_get_ip]
    lgdt [eax]

    /* 启用保护模式 */
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    /* 远跳转到32位 */
    jmp 0x08:ap_protected_mode

/* GDT指针地址占位符（由BSP填充） */
.L_gdt_ptr_addr:
    .quad 0

.Code32
ap_protected_mode:
    /* 设置段寄存器 */
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    /* 启用PAE */
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    /* 设置PML4地址 */
    mov eax, [pml4_addr]
    mov cr3, eax

    /* 启用长模式 */
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    /* 启用分页 */
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    /* 远跳转到64位 */
    jmp 0x08:ap_long_mode

.Code64
ap_long_mode:
    /* 设置段寄存器 */
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    /* 设置栈 */
    mov rsp, [stack_top]

    /* 调用AP主函数（简单的任务处理器） */
    call amp_ap_main

    /* 如果返回，进入halt循环 */
ap_halt:
    cli
    hlt
    jmp ap_halt

.size ap_trampoline, .-ap_trampoline

/* ==================== 数据段 ==================== */

.section .ap_trampoline

/* GDT描述符 */
.align 16
.global gdt_ptr
gdt_ptr:
    .word gdt_end - gdt - 1
    .quad gdt

/* GDT */
.align 16
.global gdt
gdt:
    .quad 0
    .quad 0x00AF9A000000FFFF  /* 代码段 */
    .quad 0x00CF92000000FFFF  /* 数据段 */
gdt_end:

/**
 * PML4地址 - 必须由BSP在启动AP前填充
 * 
 * 初始化流程：
 * 1. BSP分配AP专用页表
 * 2. BSP将页表物理地址写入pml4_addr
 * 3. BSP分配AP专用栈
 * 4. BSP将栈顶地址写入stack_top
 * 5. BSP计算gdt_ptr的实际物理地址并写入.L_gdt_ptr_addr
 * 6. BSP将代码复制到目标地址（如0x8000）
 * 7. BSP发送INIT-SIPI-SIPI序列启动AP
 * 
 * 注意：这些变量位于.ap_trampoline段，该段会被复制到
 * 低物理地址供AP在实模式下访问。
 */
.align 8
.global pml4_addr
pml4_addr:
    .quad 0

.align 8
.global stack_top
stack_top:
    .quad 0

/* AP启动代码段大小标记 */
.global ap_trampoline_end
ap_trampoline_end: