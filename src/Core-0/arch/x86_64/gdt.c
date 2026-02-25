/*
 * SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * x86-64 GDT实现
 */

#include "gdt.h"
#include "lib/mem.h"
#include "lib/console.h"

#define GDT_ENTRIES 7

/* GDT表 */
static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;

/* TSS */
static tss_entry_t tss;

/* 创建GDT描述符 */
static void gdt_set_entry(int num, uint64_t base, uint64_t limit, 
                          uint8_t access, uint8_t granularity)
{
    gdt[num].limit_low  = limit & 0xFFFF;
    gdt[num].base_low   = (base >> 0) & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access     = access;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= granularity & 0xF0;
    gdt[num].base_high  = (base >> 24) & 0xFF;
}

/* 初始化GDT */
void gdt_init(void)
{
    console_puts("[GDT] Initializing GDT...\n");
    
    /* 清零GDT */
    memzero(gdt, sizeof(gdt));
    memzero(&tss, sizeof(tss));
    
    /* 设置GDT指针 */
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    /* 0号描述符：空描述符 */
    gdt_set_entry(0, 0, 0, 0, 0);
    
    /* 1号描述符：内核代码段 (64位) */
    gdt_set_entry(GDT_KERNEL_CS, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODE_DATA |
                 GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READ_WRITE,
                 GDT_GRANULARITY_4K | GDT_SIZE_64BIT);
    
    /* 2号描述符：内核数据段 */
    gdt_set_entry(GDT_KERNEL_DS, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODE_DATA |
                 GDT_ACCESS_READ_WRITE,
                 GDT_GRANULARITY_4K | GDT_SIZE_32BIT);
    
    /* 3号描述符：用户代码段 (64位) */
    gdt_set_entry(GDT_USER_CS, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_DATA |
                 GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READ_WRITE,
                 GDT_GRANULARITY_4K | GDT_SIZE_64BIT);
    
    /* 4号描述符：用户数据段 */
    gdt_set_entry(GDT_USER_DS, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_DATA |
                 GDT_ACCESS_READ_WRITE,
                 GDT_GRANULARITY_4K | GDT_SIZE_32BIT);
    
    /* 5号描述符：TSS */
    /* 完整实现：设置TSS描述符 */
    /* TSS用于特权级切换和中断处理 */
    uint64_t tss_base = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss) - 1;
    
    /* TSS下限 */
    gdt_set_entry(GDT_TSS, tss_base, tss_limit,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_TSS |
                 GDT_ACCESS_EXECUTABLE,
                 GDT_GRANULARITY_BYTE | GDT_SIZE_64BIT);
    
    /* TSS上限 */
    gdt_set_entry(GDT_TSS + 1, (tss_base >> 32) & 0xFFFFFFFF, 0, 0, 0);
    
    /* 加载GDT */
    gdt_load(&gdt_ptr);
    
    /* 加载TSS */
    tss_load(GDT_TSS << 3);
    
    /* 重新加载段寄存器 */
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov $0x08, %%ax\n"
        "mov %%ax, %%ss\n"
        : : : "ax"
    );
    
    console_puts("[GDT] GDT initialized\n");
}

/* 设置TSS栈 */
void tss_set_stack(uint64_t rsp)
{
    /* 完整实现：TSS栈设置 */
    /* 设置特权级0（内核）的栈指针 */
    tss.rsp0 = rsp;
    
    /* 设置特权级1和2的栈指针（如果使用） */
    tss.rsp1 = 0;
    tss.rsp2 = 0;
    
    /* 设置IST栈（用于中断处理） */
    tss.ist[0] = rsp;  /* IST1: 用于双重错误等关键中断 */
    tss.ist[1] = 0;
    tss.ist[2] = 0;
    tss.ist[3] = 0;
    tss.ist[4] = 0;
    tss.ist[5] = 0;
    tss.ist[6] = 0;
    tss.ist[7] = 0;
    (void)rsp;
}