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
    
    /* 直接使用嵌入的 HIK 格式内核数据 */
    *kernel_data = (void *)bin_hic_kernel_hic;
    *kernel_size = bin_hic_kernel_hic_len;
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Embedded kernel loaded\n");
    
    /* 显示内核大小 */
    console_printf("[BOOTLOADER AUDIT] Stage 3: Kernel size: %d bytes (HIK format)\n", (int)*kernel_size);
    
    /* DEBUG: 检查内核魔数 */
    uint8_t *raw_kernel = (uint8_t *)*kernel_data;
    if (raw_kernel[0] == 0x48 && raw_kernel[1] == 0x49 && raw_kernel[2] == 0x43 && raw_kernel[3] == 0x5f) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: HIK magic verified\n");
    } else {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR: Invalid HIK magic\n");
        return EFI_INVALID_PARAMETER;
    }
    
    return EFI_SUCCESS;
}
