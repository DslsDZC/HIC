OUTPUT_FORMAT("pe-x86-64")
ENTRY(UefiMain)
SECTIONS
{
    . = 0;
    .text : ALIGN(4096) {
        *(.text)
        *(.text*)
    }
    .rodata : ALIGN(4096) {
        *(.rodata)
        *(.rodata*)
    }
    .data : ALIGN(4096) {
        *(.data)
        *(.data*)
    }
    .bss : ALIGN(4096) {
        *(.bss)
        *(.bss*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.comment)
        *(.note.gnu.build-id)
        *(.eh_frame)
    }
}
