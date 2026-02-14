OUTPUT_FORMAT("pe-x86-64")
ENTRY(_start)
SECTIONS
{
    . = 0;
    .text : {
        *(.text)
        *(.text*)
    }
    .rodata : {
        *(.rodata)
        *(.rodata*)
    }
    .data : {
        *(.data)
        *(.data*)
    }
    .bss : {
        *(.bss)
        *(.bss*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.comment)
        *(.note.gnu.build-id)
    }
}
