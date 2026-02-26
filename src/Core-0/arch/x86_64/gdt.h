/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64全局描述符表(GDT)
 */

#ifndef HIC_ARCH_X86_64_GDT_H
#define HIC_ARCH_X86_64_GDT_H

#include <stdint.h>

/* GDT描述符 */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/* TSS描述符 */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;

    /* TSS状态（x86-64） */
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

/* GDT指针 */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

/* 访问字节定义 */
#define GDT_ACCESS_PRESENT    (1 << 7)
#define GDT_ACCESS_RING0      (0 << 5)
#define GDT_ACCESS_RING3      (3 << 5)
#define GDT_ACCESS_SYSTEM     (0 << 4)
#define GDT_ACCESS_CODE_DATA  (1 << 4)
#define GDT_ACCESS_EXECUTABLE (1 << 3)
#define GDT_ACCESS_READ_WRITE (1 << 1)
#define GDT_ACCESS_ACCESSED   (1 << 0)
#define GDT_ACCESS_TSS        (0x9)  /* TSS类型：可用64位TSS */

/* 粒度定义 */
#define GDT_GRANULARITY_4K    (1 << 7)
#define GDT_GRANULARITY_BYTE  (0 << 7)
#define GDT_SIZE_32BIT        (0 << 6)
#define GDT_SIZE_64BIT        (1 << 6)

/* GDT选择子 */
#define GDT_NULL      0
#define GDT_KERNEL_CS  1
#define GDT_KERNEL_DS  2
#define GDT_USER_CS    3
#define GDT_USER_DS    4
#define GDT_TSS        5

/* 初始化GDT */
void gdt_init(void);

/* 加载GDT */
extern void gdt_load(gdt_ptr_t *gdt_ptr);

/* 加载TSS */
extern void tss_load(uint16_t tss_selector);

/* 设置TSS */
void tss_set_stack(uint64_t rsp);

#endif /* HIC_ARCH_X86_64_GDT_H */