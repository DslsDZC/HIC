/**
 * HIK内核主入口
 * Bootloader跳转到的第一个C函数
 */

#include "boot_info.h"
#include "types.h"

/**
 * 内核入口点（汇编调用）
 * 
 * 这个函数由bootloader的jump_to_kernel函数调用
 * 接收boot_info作为参数（在RDI寄存器中）
 */
void kernel_start(hik_boot_info_t* boot_info) {
    // 转发到实际的内核入口点
    kernel_entry(boot_info);
}