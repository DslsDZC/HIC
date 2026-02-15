/**
 * HIK内核主入口
 * Bootloader跳转到的第一个C函数
 */

#include "boot_info.h"
#include "types.h"
#include "lib/console.h"

/**
 * 内核入口点（汇编调用）
 * 
 * 这个函数由bootloader的jump_to_kernel函数调用
 * 接收boot_info作为参数（在RDI寄存器中）
 */
void kernel_start(hik_boot_info_t* boot_info) {
    console_puts("HIK: Bootloader passed control\n");
    console_puts("Boot info at: 0x");
    console_puthex64((u64)boot_info);
    console_puts("\n");
    
    // 记录引导日志（如果有）
    if (boot_info && boot_info->version >= 1) {
        // boot_info可能包含引导日志指针
        console_puts("Boot log entries: ");
        console_putu64(boot_info->boot_log.log_entry_count);
        console_puts("\n");
    }
    
    // 转发到实际的内核入口点
    kernel_entry(boot_info);
}