/* HIC 内核链接脚本 */
/* 遵循形式化验证的内存约束和三层模型架构 */

/* 强制入口点为kernel_entry符号 */
ENTRY(kernel_entry)

/* 形式化验证约束：
 * 1. 内核大小限制在2MB以内
 * 2. BSS段限制在512KB以内
 * 3. 所有段必须4K对齐
 * 4. 内存布局必须可验证
 */

SECTIONS
{
    /* 内核起始地址 - Core-0层基址 */
    . = 0x100000;

    /* 代码段 - 只读可执行 */
    .text ALIGN(4K) : {
        _text_start = .;
        *(.text.kernel_entry)
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    /* 只读数据段 - 常量和字符串 */
    .rodata ALIGN(4K) : {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    /* 已初始化数据段 - 全局变量 */
    .data ALIGN(4K) : {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    /* 未初始化数据段 - 排除.lbss */
    .bss ALIGN(4K) : {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    /* Large BSS段 - 放在内核之后 */
    .lbss ALIGN(4K) : {
        _lbss_start = .;
        *(.lbss)
        *(.lbss.*)
        _lbss_end = .;
    }

    /* 内核结束标记 */
    _kernel_end = .;

    /* 形式化验证检查点 */
    _kernel_size = _kernel_end - 0x100000;
    _kernel_text_size = _text_end - _text_start;
    _kernel_data_size = _data_end - _data_start;
    _kernel_bss_size = _bss_end - _bss_start;
    _kernel_lbss_size = _lbss_end - _lbss_start;
}