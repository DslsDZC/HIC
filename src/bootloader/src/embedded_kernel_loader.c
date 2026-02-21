/* 嵌入的内核加载器 - 绕过UEFI FAT32驱动问题 */

#include <efi.h>
#include "console.h"
#include "embedded_kernel.h"

/**
 * 加载内核映像（从嵌入的数据）
 */
EFI_STATUS load_kernel_image_embedded(void **kernel_data, uint64_t *kernel_size)
{
    console_puts("[BOOTLOADER AUDIT] Stage 3: Loading Kernel Image (embedded)\n");
    
    /* 直接使用嵌入的内核数据 */
    *kernel_data = (void *)bin_hic_kernel_elf;
    *kernel_size = bin_hic_kernel_elf_len;
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Embedded kernel loaded\n");
    
    /* 显示内核大小 */
    console_printf("[BOOTLOADER AUDIT] Stage 3: Kernel size: %d bytes\n", (int)*kernel_size);
    
    return EFI_SUCCESS;
}
