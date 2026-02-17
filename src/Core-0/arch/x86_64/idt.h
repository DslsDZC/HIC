/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64中断描述符表(IDT)
 */

#ifndef HIC_ARCH_X86_64_IDT_H
#define HIC_ARCH_X86_64_IDT_H

#include <stdint.h>

/* IDT描述符 */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

/* IDT指针 */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

/* 中断门类型 */
#define IDT_TYPE_INTERRUPT_GATE  0xE
#define IDT_TYPE_TRAP_GATE      0xF
#define IDT_TYPE_TASK_GATE      0x5

/* 特权级 */
#define IDT_DPL0   0
#define IDT_DPL3   3

/* 中断向量 */
#define IDT_VECTOR_DIVIDE_ERROR       0
#define IDT_VECTOR_DEBUG             1
#define IDT_VECTOR_NMI               2
#define IDT_VECTOR_BREAKPOINT        3
#define IDT_VECTOR_OVERFLOW          4
#define IDT_VECTOR_BOUND_RANGE       5
#define IDT_VECTOR_INVALID_OPCODE    6
#define IDT_VECTOR_DEVICE_NOT_AVAIL  7
#define IDT_VECTOR_DOUBLE_FAULT      8
#define IDT_VECTOR_INVALID_TSS       10
#define IDT_VECTOR_SEGMENT_NOT_PRESENT 11
#define IDT_VECTOR_STACK_SEGMENT     12
#define IDT_VECTOR_GENERAL_PROTECTION 13
#define IDT_VECTOR_PAGE_FAULT        14
#define IDT_VECTOR_X87_FPU_ERROR     16
#define IDT_VECTOR_ALIGNMENT_CHECK   17
#define IDT_VECTOR_MACHINE_CHECK     18
#define IDT_VECTOR_SIMD_FP           19
#define IDT_VECTOR_VIRTUALIZATION    20
#define IDT_VECTOR_SECURITY          30
#define IDT_VECTOR_IRQ_BASE          32
#define IDT_VECTOR_SYSCALL           128

/* 中断处理函数类型 */
typedef void (*interrupt_handler_t)(void);

/* 初始化IDT */
void idt_init(void);

/* 设置IDT门 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, 
                 uint8_t type, uint8_t dpl);

/* 加载IDT */
extern void idt_load(idt_ptr_t *idt_ptr);

/* 外部中断处理 */
extern void isr_common_stub(void);
extern void irq_common_stub(void);

#endif /* HIC_ARCH_X86_64_IDT_H */