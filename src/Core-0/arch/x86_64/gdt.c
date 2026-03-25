/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
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

/* 创建64位TSS描述符（需要两个GDT条目）
 * TSS描述符在64位模式下需要16字节（两个8字节GDT条目）
 * 第一个条目： 包含低32位基址、限长等
 * 第二个条目： 包含高32位基址
 * 
 * x86-64 TSS描述符布局（16字节）：
 * 字节 0-1:   limit[15:0]
 * 字节 2-3:   base[15:0]
 * 字节 4:     base[23:16]
 * 字节 5:     access byte (0x89 = 64-bit TSS, Available)
 * 字节 6:     limit[19:16] + granularity flags
 * 字节 7:     base[31:24]
 * 字节 8-11:  base[63:32]  <-- 高32位基址
 * 字节 12-15: reserved (must be 0)
 */
static void gdt_set_tss_entry(int num, uint64_t base, uint32_t limit)
{
    /* 第一个条目： 低32位基址和限长 */
    gdt[num].limit_low  = limit & 0xFFFF;
    gdt[num].base_low   = (base >> 0) & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access     = GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | 0x89;  /* 64-bit TSS, Available */
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= GDT_GRANULARITY_BYTE;  /* Byte granularity */
    gdt[num].base_high  = (base >> 24) & 0xFF;
    
    /* 第二个条目： 高32位基址（修复版本）
     * 
     * gdt_entry_t 结构在第二个描述符中的解释：
     * - limit_low  (字节 8-9):  base[47:32]
     * - base_low   (字节 10-11): base[63:48]
     * - base_middle (字节 12):  保留 (必须为0)
     * - access     (字节 13):   保留 (必须为0)
     * - granularity (字节 14):  保留 (必须为0)
     * - base_high  (字节 15):   保留 (必须为0)
     * 
     * 问题修复：原代码将基址[47:32]放入limit_low，基址[63:48]放入base_low
     * 这是正确的，因为这两个字段连续存储在内存中（字节8-11）
     */
    gdt[num + 1].limit_low  = (uint16_t)((base >> 32) & 0xFFFF);   /* 基址[47:32] -> 字节 8-9 */
    gdt[num + 1].base_low   = (uint16_t)((base >> 48) & 0xFFFF);   /* 基址[63:48] -> 字节 10-11 */
    gdt[num + 1].base_middle = 0;  /* 保留 (字节 12) */
    gdt[num + 1].access     = 0;   /* 保留 (字节 13) */
    gdt[num + 1].granularity = 0;  /* 保留 (字节 14) */
    gdt[num + 1].base_high  = 0;   /* 保留 (字节 15) */
    
    /* 调试输出：验证TSS描述符设置 */
    console_puts("[GDT] TSS descriptor: base=0x");
    console_puthex64(base);
    console_puts(" limit=0x");
    console_puthex32(limit);
    console_puts("\n");
    console_puts("[GDT]   Entry[");
    console_putu32((u32)num);
    console_puts("]: 0x");
    console_puthex64(*((uint64_t*)&gdt[num]));
    console_puts("\n");
    console_puts("[GDT]   Entry[");
    console_putu32((u32)num + 1);
    console_puts("]: 0x");
    console_puthex64(*((uint64_t*)&gdt[num + 1]));
    console_puts("\n");
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
    
    /* 5号描述符: TSS (64-bit)
     * 完整实现: 设置TSS描述符
     * TSS用于特权级切换和中断处理
     * 注意: 64位TSS需要两个GDT条目（共16字节)
     */
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    
    /* 使用专门的函数设置64位TSS描述符 */
    gdt_set_tss_entry(GDT_TSS, tss_base, tss_limit);
    
    /* 加载GDT */
    gdt_load(&gdt_ptr);
    
    /* 加载TSS */
    tss_load(TSS_SELECTOR);
    
    /* 重新加载段寄存器 */
    __asm__ volatile (
        "mov %0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %0, %%ax\n"   /* SS 应使用数据段选择子 */
        "mov %%ax, %%ss\n"
        : : "i"(KERNEL_DS_SELECTOR) : "ax"
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
}