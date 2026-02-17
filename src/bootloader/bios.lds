/**
 * HIC BIOS Bootloader链接脚本
 * 
 * 内存布局：
 * 0x00000000 - 0x000FFFFF: BIOS区域
 * 0x0007C00 - 0x0007DFF: Bootloader (512字节)
 * 0x0008000 - 0x0009FFF: Bootloader扩展 (8KB)
 * 0x000A000 - 0x000BFFF: 视频 RAM
 * 0x000C000 - 0x000FFFFF: BIOS ROM
 * 0x00100000 - 0x00FFFFFF: 内核区域
 */

ENTRY(_start)

SECTIONS
{
    /* Bootloader代码段 (16位) */
    . = 0x7C00;
    
    .boot16 :
    {
        *(.boot16)
        *(.text16)
        . = 0x7DFE;
        SHORT(0xAA55);  /* BIOS启动签名 */
    }
    
    /* Bootloader代码段 (32位) */
    . = 0x8000;
    
    .boot32 :
    {
        *(.text32)
        *(.text.boot32)
    }
    
    /* Bootloader代码段 (64位) */
    . = ALIGN(4096);
    
    .text :
    {
        *(.text)
        *(.text*)
        . = ALIGN(4096);
    }
    
    /* 只读数据 */
    .rodata :
    {
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4096);
    }
    
    /* 数据段 */
    .data :
    {
        *(.data)
        *(.data*)
        . = ALIGN(4096);
    }
    
    /* BSS段 */
    .bss :
    {
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
    }
    
    /* 栈 */
    .stack :
    {
        . = ALIGN(16);
        . = . + 0x1000;  /* 4KB栈 */
    }
    
    /* 内核区域 */
    . = 0x100000;
    
    .kernel :
    {
        *(.kernel)
    }
    
    /* 符号表 */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}