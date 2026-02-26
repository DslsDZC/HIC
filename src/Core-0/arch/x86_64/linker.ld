/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核链接脚本（x86_64）
 * 内存布局：0x100000 开始
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/* 内核基地址 */
KERNEL_BASE = 0x100000;

/* 内核虚拟地址（如果使用高半内核映射） */
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    /* 内核代码段（从物理地址 0x100000 开始） */
    . = KERNEL_BASE;

    .text ALIGN(4K) : AT(ADDR(.text)) {
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        *(.rodata)
        *(.rodata.*)
    }

    /* 初始化代码段（启动后可丢弃） */
    .init ALIGN(4K) : AT(ADDR(.init)) {
        __init_start = .;
        *(.init)
        *(.init.*)
        __init_end = .;
    }

    /* 数据段 */
    .data ALIGN(4K) : AT(ADDR(.data)) {
        *(.data)
        *(.data.*)
    }

    /* BSS段（零初始化数据） */
    .bss ALIGN(4K) : AT(ADDR(.bss)) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4K);
        __bss_end = .;
    }

    /* 栈段（预留16KB） */
    .stack ALIGN(4K) : AT(ADDR(.stack)) {
        __stack_start = .;
        . += 16K;
        __stack_end = .;
    }

    /* 内核结束 */
    _kernel_end = .;

    /* 丢弃调试信息 */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}