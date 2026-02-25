<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 引导加载程序

## 概述

HIC 引导加载程序负责将 HIC 内核加载到内存并跳转到内核入口点。支持 UEFI 和 BIOS 两种引导方式，确保安全启动。

## Bootloader 架构

### 主要组件

```
┌─────────────────────────────────┐
│   Bootloader 主程序               │
├─────────────────────────────────┤
│   1. 硬件初始化                  │
│   2. 内存检测                    │
│  ┌─────────────────────────────┐  │
│  │   UEFI/BIOS 接口层         │  │
│  └─────────────────────────────┘  │
│   3. 内核加载                    │
│   4. 签名验证                    │
│   5. 跳转到内核                  │
└─────────────────────────────────┘
```

## 引导流程

### 1. 硬件初始化

```c
// 硬件初始化
void hardware_init(void) {
    // 初始化串口（用于调试）
    serial_init(COM1_PORT, 115200);
    
    // 检测CPU特性
    detect_cpu_features();
    
    // 检测内存
    detect_memory();
    
    // 初始化TPM
    tpm2_init();
}
```

### 2. 内存检测

```c
// 检测内存
void detect_memory(void) {
    // 通过BIOS/UEFI获取内存信息
    boot_info_t *boot_info = get_boot_info();
    
    // 遍历内存映射
    for (u32 i = 0; i < boot_info->mem_map_entry_count; i++) {
        mem_entry_t *entry = &boot_info->mem_map[i];
        
        if (entry->type == MEM_TYPE_USABLE) {
            // 添加到可用内存列表
            add_usable_memory(entry->base, entry->length);
        }
    }
}
```

### 3. 内核加载

```c
// 加载内核镜像
hic_status_t load_kernel_image(void) {
    // 查找内核镜像
    char *kernel_path = find_kernel_image();
    if (!kernel_path) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    // 读取内核文件
    u64 kernel_size;
    u8 *kernel_data = read_file(kernel_path, &kernel_size);
    
    // 验证内核签名
    if (!verify_kernel_signature(kernel_data, kernel_size)) {
        return HIC_ERROR_SIGNATURE;
    }
    
    // 计算内核加载地址
    u64 load_addr = calculate_kernel_load_addr(kernel_size);
    
    // 复制内核到内存
    memcpy((void*)load_addr, kernel_data, kernel_size);
    
    // 记录内核信息
    g_kernel_info.base = load_addr;
    g_kernel_info.size = kernel_size;
    
    return HIC_SUCCESS;
}
```

### 4. 签名验证

```c
// 验证内核签名
bool verify_kernel_signature(u8 *kernel_data, u64 kernel_size) {
    // 读取签名
    hicmod_signature_t *sig = (hicmod_signature_t *)
        (kernel_data + kernel_size - sizeof(hicmod_signature_t));
    
    // 计算内核哈希
    u64 signed_size = kernel_size - sizeof(hicmod_signature_t);
    u8 hash[48];
    sha384(kernel_data, signed_size, hash);
    
    // 验证 Ed25519 签名
    ed25519_public_key_t *pubkey = (ed25519_public_key_t *)sig->public_key;
    return ed25519_verify(pubkey, hash, 48, sig->signature, 64);
}
```

### 5. 跳转到内核

```c
// 跳转到内核
void jump_to_kernel(void) {
    // 设置内核启动参数
    hic_boot_info_t *boot_info = prepare_boot_info();
    
    // 设置页表（如果需要）
    setup_page_tables();
    
    // 跳转到内核入口
    void (*kernel_entry)(hic_boot_info_t*) = (void(*)(hic_boot_info_t*))
        g_kernel_info.base;
    
    console_puts("Jumping to kernel...\n");
    kernel_entry(boot_info);
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}
```

## UEFI 引导

### UEFI 入口点

```c
// UEFI 入口点
EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
) {
    // 初始化 UEFI 服务
    EFI_STATUS status = InitializeLib(ImageHandle, SystemTable);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 获取图形输出协议
    gGOP = InitializeGOP();
    
    // 初始化串口
    serial_init();
    
    // 执行引导流程
    do_boot();
    
    return EFI_SUCCESS;
}
```

## BIOS 引导

### BIOS 入口点

```c
// BIOS 入口点
void BiosMain(void) {
    // 初始化 VGA 显示
    vga_init();
    
    // 初始化串口
    serial_init(COM1_PORT, 115200);
    
    // 执行引导流程
    do_boot();
}
```

## 安全启动

### TPM 测量

```c
// TPM 测量内核
void measure_kernel_to_tpm(void) {
    // 测量内核代码
    u64 kernel_size = g_kernel_info.size;
    tpm2_pcr_extend(TPM_PCR_KERNEL, 
                     (u8*)g_kernel_info.base, 
                     kernel_size);
    
    // 测量启动参数
    u64 params_size = sizeof(g_boot_info);
    tpm2_pcr_extend(TPM_PCR_PARAMS,
                     (u8*)g_boot_info,
                     params_size);
}
```

## 最佳实践

1. **签名验证**: 始终验证内核签名
2. **错误处理**: 正确处理所有错误情况
3. **调试输出**: 提供详细的调试信息
4. **兼容性**: 支持多种硬件配置
5. **安全启动**: 启用 TPM 测量

## 相关文档

- [UEFI 引导](./29-UEFI.md) - UEFI 引导
- [BIOS 引导](./30-BIOS.md) - BIOS 引导
- [安全启动](./16-SecureBoot.md) - 安全启动

---

*最后更新: 2026-02-14*