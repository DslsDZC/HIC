/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64 IDT实现
 */

#include "idt.h"
#include "gdt.h"
#include "lib/mem.h"
#include "lib/console.h"

#define IDT_ENTRIES 256

/* IDT表 */
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

/* 外部汇编函数声明 */
extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_16(void);
extern void isr_17(void);
extern void isr_18(void);
extern void isr_19(void);
extern void isr_20(void);
extern void isr_30(void);
extern void isr_128(void);

/* IRQ 入口点（在 fast_path.S 中实现，每个 IRQ 有独立的入口） */
extern void irq_32(void);
extern void irq_33(void);
extern void irq_34(void);
extern void irq_35(void);
extern void irq_36(void);
extern void irq_37(void);
extern void irq_38(void);
extern void irq_39(void);
extern void irq_40(void);
extern void irq_41(void);
extern void irq_42(void);
extern void irq_43(void);
extern void irq_44(void);
extern void irq_45(void);
extern void irq_46(void);
extern void irq_47(void);

/* 外部函数声明 */
extern void irq_dispatch(u32 irq_vector);

/* 设置IDT门 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector,
                 uint8_t type, uint8_t dpl)
{
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = selector;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type | (dpl << 5) | 0x80;  /* Present */
    idt[vector].offset_middle = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

/* 初始化IDT */
void idt_init(void)
{
    console_puts("[IDT] Initializing IDT...\n");
    
    /* 清零IDT */
    memzero(idt, sizeof(idt));
    
    /* 设置IDT指针 */
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    /* 设置异常处理程序 */
    idt_set_gate(IDT_VECTOR_DIVIDE_ERROR, (uint64_t)isr_0, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DEBUG, (uint64_t)isr_1, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_NMI, (uint64_t)isr_2, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_BREAKPOINT, (uint64_t)isr_3, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_OVERFLOW, (uint64_t)isr_4, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_BOUND_RANGE, (uint64_t)isr_5, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_INVALID_OPCODE, (uint64_t)isr_6, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DEVICE_NOT_AVAIL, (uint64_t)isr_7, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DOUBLE_FAULT, (uint64_t)isr_8, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_INVALID_TSS, (uint64_t)isr_10, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SEGMENT_NOT_PRESENT, (uint64_t)isr_11, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_STACK_SEGMENT, (uint64_t)isr_12, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_GENERAL_PROTECTION, (uint64_t)isr_13, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_PAGE_FAULT, (uint64_t)isr_14, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_X87_FPU_ERROR, (uint64_t)isr_16, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_ALIGNMENT_CHECK, (uint64_t)isr_17, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_MACHINE_CHECK, (uint64_t)isr_18, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SIMD_FP, (uint64_t)isr_19, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_VIRTUALIZATION, (uint64_t)isr_20, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SECURITY, (uint64_t)isr_30, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);

    /* 设置系统调用 */
    idt_set_gate(IDT_VECTOR_SYSCALL, (uint64_t)isr_128, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL3);
    
    /* 设置IRQ处理程序（向量 32-47）
     * 每个 IRQ 有独立的入口点，入口点负责压入向量号后调用 isr_fast_stub
     * 这是必要的，因为硬件中断不会自动压入向量号
     */
    idt_set_gate(32, (uint64_t)irq_32, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(33, (uint64_t)irq_33, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(34, (uint64_t)irq_34, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(35, (uint64_t)irq_35, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(36, (uint64_t)irq_36, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(37, (uint64_t)irq_37, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(38, (uint64_t)irq_38, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(39, (uint64_t)irq_39, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(40, (uint64_t)irq_40, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(41, (uint64_t)irq_41, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(42, (uint64_t)irq_42, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(43, (uint64_t)irq_43, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(44, (uint64_t)irq_44, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(45, (uint64_t)irq_45, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(46, (uint64_t)irq_46, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(47, (uint64_t)irq_47, GDT_KERNEL_CS << 3,
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    
    /* 加载IDT */
    idt_load(&idt_ptr);
    
    console_puts("[IDT] IDT initialized (fast path for IRQs)\n");
    
    /* 注意：
     * 1. 异常错误码处理：部分异常（如缺页、GPF等）会自动压入错误码。
     *    汇编存根必须正确处理错误码，确保栈平衡。
     * 2. 中断控制器初始化：IRQ映射到向量32-47，需要在加载IDT后初始化
     *    PIC（重新编程）或APIC，避免与异常向量冲突。
     * 3. 系统调用：此IDT支持 int 0x80（兼容模式），同时支持 syscall 指令
     *   （通过 syscall_fast_entry）。推荐使用 syscall 指令以获得更好性能。
     */
    
    /* 初始化 SYSCALL MSR（必须在 GDT 初始化后） */
    syscall_init();
}

/* ==================== MSR 辅助函数 ==================== */

/**
 * @brief 读取 MSR
 * @param msr MSR 地址
 * @return MSR 值
 */
static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t)high << 32) | low;
}

/**
 * @brief 写入 MSR
 * @param msr MSR 地址
 * @param value MSR 值
 */
static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t)(value & 0xFFFFFFFFULL);
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile (
        "wrmsr"
        :
        : "a"(low), "d"(high), "c"(msr)
    );
}

/* ==================== SYSCALL MSR 定义 ==================== */

#define MSR_IA32_EFER      0xC0000080  /* 扩展特性启用寄存器 */
#define MSR_IA32_STAR      0xC0000081  /* 段选择子 */
#define MSR_IA32_LSTAR     0xC0000082  /* SYSCALL 入口点 */
#define MSR_IA32_SFMASK    0xC0000084  /* RFLAGS 掩码 */

#define EFER_SCE           (1ULL << 0) /* SYSCALL/SYSRET 启用 */

/* 外部：syscall 快速入口点（在 fast_path.S 中定义） */
extern void syscall_fast_entry(void);

/* ==================== SYSCALL MSR 初始化 ==================== */

void syscall_init(void)
{
    console_puts("[SYSCALL] Initializing SYSCALL MSR...\n");
    
    /* 1. 设置 IA32_STAR: 段选择子
     * [63:48] = 用户态 CS (SYSRETQ 使用，需 +16 跳过空描述符)
     * [47:32] = 内核态 CS (SYSCALL 使用)
     * 
     * 内核 CS = GDT_KERNEL_CS << 3 = 0x08
     * 用户 CS = GDT_USER_CS << 3 = 0x18，但 SYSRETQ 使用 0x18+16 = 0x1B
     */
    uint64_t star = ((uint64_t)(GDT_USER_CS << 3 | 3) << 48) |  /* 用户态 CS (RPL=3) */
                    ((uint64_t)(GDT_KERNEL_CS << 3) << 32);      /* 内核态 CS */
    wrmsr(MSR_IA32_STAR, star);
    console_puts("[SYSCALL]   STAR=0x");
    console_puthex64(star);
    console_puts(" (UserCS=0x1B, KernCS=0x08)\n");
    
    /* 2. 设置 IA32_LSTAR: SYSCALL 入口点 */
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_fast_entry);
    console_puts("[SYSCALL]   LSTAR=0x");
    console_puthex64((uint64_t)syscall_fast_entry);
    console_puts(" (syscall_fast_entry)\n");
    
    /* 3. 设置 IA32_SFMASK: RFLAGS 清除掩码
     * 清除 IF (中断标志) 以在 syscall 入口禁用中断
     * 清除 TF (陷阱标志) 以禁用单步调试
     */
    uint64_t sfmask = (1ULL << 9) |   /* IF - 中断标志 */
                      (1ULL << 8);    /* TF - 陷阱标志 */
    wrmsr(MSR_IA32_SFMASK, sfmask);
    console_puts("[SYSCALL]   SFMASK=0x");
    console_puthex64(sfmask);
    console_puts(" (clear IF,TF)\n");
    
    /* 4. 启用 SYSCALL 指令 (EFER.SCE) */
    uint64_t efer = rdmsr(MSR_IA32_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_IA32_EFER, efer);
    console_puts("[SYSCALL]   EFER=0x");
    console_puthex64(efer);
    console_puts(" (SCE enabled)\n");
    
    console_puts("[SYSCALL] SYSCALL/SYSRETQ initialized successfully\n");
}