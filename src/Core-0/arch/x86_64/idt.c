/**
 * x86-64 IDT实现
 */

#include "idt.h"
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

/* 中断处理函数 */
static void (*interrupt_handlers[256])(void) = {NULL};

/* 设置IDT门 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, 
                 uint8_t type, uint8_t dpl)
{
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = selector;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type | (dpl << 5) | 0x80;  /* Present */
    idt[vector].offset_middle = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
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
    idt_set_gate(IDT_VECTOR_DIVIDE_ERROR, (uint64_t)isr_0, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DEBUG, (uint64_t)isr_1, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_NMI, (uint64_t)isr_2, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_BREAKPOINT, (uint64_t)isr_3, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_OVERFLOW, (uint64_t)isr_4, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_BOUND_RANGE, (uint64_t)isr_5, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_INVALID_OPCODE, (uint64_t)isr_6, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DEVICE_NOT_AVAIL, (uint64_t)isr_7, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_DOUBLE_FAULT, (uint64_t)isr_8, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_INVALID_TSS, (uint64_t)isr_10, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SEGMENT_NOT_PRESENT, (uint64_t)isr_11, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_STACK_SEGMENT, (uint64_t)isr_12, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_GENERAL_PROTECTION, (uint64_t)isr_13, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_PAGE_FAULT, (uint64_t)isr_14, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_X87_FPU_ERROR, (uint64_t)isr_16, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_ALIGNMENT_CHECK, (uint64_t)isr_17, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_MACHINE_CHECK, (uint64_t)isr_18, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SIMD_FP, (uint64_t)isr_19, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_VIRTUALIZATION, (uint64_t)isr_20, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    idt_set_gate(IDT_VECTOR_SECURITY, (uint64_t)isr_30, KERNEL_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL0);
    
    /* 设置系统调用 */
    idt_set_gate(IDT_VECTOR_SYSCALL, (uint64_t)isr_128, USER_CS, 
                 IDT_TYPE_INTERRUPT_GATE, IDT_DPL3);
    
    /* 加载IDT */
    idt_load(&idt_ptr);
    
    console_puts("[IDT] IDT initialized\n");
}

/* 中断处理函数 */
void interrupt_handler(void)
{
    /* 从栈中获取中断向量 */
    u64 irq_vector;
    __asm__ volatile ("mov 8(%%rsp), %0" : "=r"(irq_vector));
    
    /* 调用中断控制器分发 */
    extern void irq_dispatch(u32 irq_vector);
    irq_dispatch((u32)irq_vector);
}