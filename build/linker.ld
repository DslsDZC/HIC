/**
 * HIK内核链接脚本
 * x86-64架构
 */

ENTRY(kernel_main)

SECTIONS
{
    /* 内核从1MB开始加载 */
    . = 0x100000;
    
    /* 内核代码段 */
    .text : ALIGN(4K) {
        *(.text)
        *(.text.*)
    }
    
    /* 只读数据段 */
    .rodata : ALIGN(4K) {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* 初始化数据段 */
    .data : ALIGN(4K) {
        *(.data)
        *(.data.*)
    }
    
    /* 未初始化数据段 */
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* 栈 */
    .stack : ALIGN(4K) {
        . = . + 0x10000;  /* 64KB栈 */
        _kernel_stack_top = .;
    }
    
    /* 内核结束标记 */
    _kernel_end = .;
    
    /* 丢弃的段 */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}