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
    /* 设置UEFI子系统类型 */
    . = 0;
    .pe_header : {
        LONG(0x0000000a);  /* Subsystem: EFI Application */
    }
}
