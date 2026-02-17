<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 安全启动

## 概述

HIC 安全启动机制确保系统从硬件到内核的完整信任链，防止恶意代码在启动过程中被注入。该机制结合 TPM、加密签名和验证流程，提供端到端的安全保证。

## 信任链

```
┌─────────────────────────────────────┐
│   1. 硬件信任根 (RTM)              │
│   - CPU/TPM 固件                   │
└─────────────────────────────────────┘
              ↓ 验证
┌─────────────────────────────────────┐
│   2. Bootloader                    │
│   - UEFI/BIOS 引导程序             │
│   - RSA-3072 签名验证              │
└─────────────────────────────────────┘
              ↓ 验证
┌─────────────────────────────────────┐
│   3. HIC 内核镜像                  │
│   - SHA384 哈希验证                │
│   - RSA-3072 签名验证              │
└─────────────────────────────────────┘
              ↓ 验证
┌─────────────────────────────────────┐
│   4. 内核模块 (.hicmod)            │
│   - SHA384 哈希验证                │
│   - Ed25519 签名验证               │
└─────────────────────────────────────┘
```

## TPM 集成

### TPM 测量

```c
// TPM 测量扩展
void tpm_extend_measurement(u8 *data, u32 len, u32 pcr) {
    // 计算 SHA384 哈希
    u8 hash[48];
    sha384(data, len, hash);
    
    // 扩展到 PCR
    tpm2_pcr_extend(pcr, hash, 48);
}

// 测量 Bootloader
void measure_bootloader(u8 *bootloader, u32 size) {
    tpm_extend_measurement(bootloader, size, TPM_PCR_BOOTLOADER);
}

// 测量内核
void measure_kernel(u8 *kernel, u32 size) {
    tpm_extend_measurement(kernel, size, TPM_PCR_KERNEL);
}
```

### PCR 值

```
PCR 0: BIOS/UEFI 代码
PCR 1: BIOS/UEFI 配置
PCR 2: Bootloader 代码
PCR 3: Bootloader 配置
PCR 4: 内核代码
PCR 5: 内核配置
PCR 6: 初始内存状态
PCR 7: 系统启动参数
```

## 签名验证

### Bootloader 签名验证

```c
// 验证 Bootloader 签名
bool verify_bootloader_signature(u8 *bootloader, u32 size, 
                                  u8 *signature, u32 sig_size) {
    // 获取公钥（从 TPM 或安全存储）
    rsa_public_key_t *pubkey = get_bootloader_public_key();
    
    // 计算 SHA384 哈希
    u8 hash[48];
    sha384(bootloader, size, hash);
    
    // 验证 RSA-3072 签名
    return rsa_3072_verify_pss(pubkey, hash, 48, signature, sig_size);
}
```

### 内核签名验证

```c
// 验证内核签名
bool verify_kernel_signature(u8 *kernel, u32 size, 
                              u8 *signature, u32 sig_size) {
    // 获取内核公钥
    rsa_public_key_t *pubkey = get_kernel_public_key();
    
    // 计算 SHA384 哈希
    u8 hash[48];
    sha384(kernel, size, hash);
    
    // 验证 RSA-3072 签名
    return rsa_3072_verify_pss(pubkey, hash, 48, signature, sig_size);
}
```

### 模块签名验证

```c
// 验证模块签名
bool verify_module_signature(u8 *module, u32 size, 
                              u8 *signature, u32 sig_size) {
    // 获取模块公钥
    ed25519_public_key_t *pubkey = get_module_public_key();
    
    // 计算 SHA384 哈希
    u8 hash[48];
    sha384(module, size, hash);
    
    // 验证 Ed25519 签名
    return ed25519_verify(pubkey, hash, 48, signature, sig_size);
}
```

## 启动流程

### UEFI 启动

```c
// UEFI 启动入口
void UefiMain(void) {
    // 1. 初始化 TPM
    tpm2_init();
    
    // 2. 测量 Bootloader
    measure_bootloader(bootloader_code, bootloader_size);
    
    // 3. 验证 Bootloader 签名
    if (!verify_bootloader_signature(bootloader_code, bootloader_size,
                                     bootloader_sig, bootloader_sig_size)) {
        console_puts("Bootloader signature verification failed!\n");
        halt();
    }
    
    // 4. 加载内核
    load_kernel_image();
    
    // 5. 验证内核签名
    if (!verify_kernel_signature(kernel_image, kernel_size,
                                  kernel_sig, kernel_sig_size)) {
        console_puts("Kernel signature verification failed!\n");
        halt();
    }
    
    // 6. 测量内核
    measure_kernel(kernel_image, kernel_size);
    
    // 7. 跳转到内核
    jump_to_kernel();
}
```

### BIOS 启动

```c
// BIOS 启动入口
void BiosMain(void) {
    // 1. 初始化 TPM
    tpm2_init();
    
    // 2. 测量 Bootloader
    measure_bootloader(bootloader_code, bootloader_size);
    
    // 3. 验证 Bootloader 签名
    if (!verify_bootloader_signature(bootloader_code, bootloader_size,
                                     bootloader_sig, bootloader_sig_size)) {
        console_puts("Bootloader signature verification failed!\n");
        halt();
    }
    
    // 4. 加载内核
    load_kernel_image();
    
    // 5. 验证内核签名
    if (!verify_kernel_signature(kernel_image, kernel_size,
                                  kernel_sig, kernel_sig_size)) {
        console_puts("Kernel signature verification failed!\n");
        halt();
    }
    
    // 6. 测量内核
    measure_kernel(kernel_image, kernel_size);
    
    // 7. 跳转到内核
    jump_to_kernel();
}
```

## 安全启动状态

### 启动状态检查

```c
// 检查安全启动状态
secure_boot_status_t check_secure_boot_status(void) {
    secure_boot_status_t status = {
        .tpm_enabled = false,
        .bootloader_verified = false,
        .kernel_verified = false,
        .modules_verified = false,
        .pcr_valid = false
    };
    
    // 检查 TPM
    if (tpm2_is_enabled()) {
        status.tpm_enabled = true;
    }
    
    // 检查 PCR 值
    u8 expected_pcr[TPM_PCR_COUNT][48];
    if (verify_pcr_values(expected_pcr)) {
        status.pcr_valid = true;
    }
    
    return status;
}
```

### 启动报告

```c
// 生成启动报告
void generate_boot_report(void) {
    secure_boot_status_t status = check_secure_boot_status();
    
    console_puts("=== Secure Boot Report ===\n");
    console_printf("TPM Enabled: %s\n", status.tpm_enabled ? "Yes" : "No");
    console_printf("Bootloader Verified: %s\n", 
                  status.bootloader_verified ? "Yes" : "No");
    console_printf("Kernel Verified: %s\n", 
                  status.kernel_verified ? "Yes" : "No");
    console_printf("Modules Verified: %s\n", 
                  status.modules_verified ? "Yes" : "No");
    console_printf("PCR Valid: %s\n", status.pcr_valid ? "Yes" : "No");
    
    // 输出 PCR 值
    for (u32 i = 0; i < TPM_PCR_COUNT; i++) {
        u8 pcr_value[48];
        tpm2_pcr_read(i, pcr_value);
        
        console_printf("PCR %u: ", i);
        for (u32 j = 0; j < 48; j++) {
            console_printf("%02x", pcr_value[j]);
        }
        console_puts("\n");
    }
}
```

## 密钥管理

### 密钥存储

```c
// 密钥存储结构
typedef struct key_store {
    rsa_public_key_t  bootloader_pubkey;
    rsa_public_key_t  kernel_pubkey;
    ed25519_public_key_t module_pubkey;
    u8                sealed_key[512];
} key_store_t;

// 从 TPM 加载密钥
bool load_keys_from_tpm(key_store_t *store) {
    // 使用 TPM 密封/解封机制
    return tpm2_unseal(store->sealed_key, sizeof(store->sealed_key), store);
}
```

### 密钥轮换

```c
// 密钥轮换
bool rotate_keys(void) {
    // 1. 生成新密钥对
    rsa_key_pair_t new_kernel_key;
    rsa_generate_key_pair(3072, &new_kernel_key);
    
    // 2. 更新密钥存储
    update_key_store(&new_kernel_key);
    
    // 3. 使用新密钥重新签名内核
    sign_kernel_with_new_key(&new_kernel_key);
    
    // 4. 密封到 TPM
    seal_keys_to_tpm();
    
    return true;
}
```

## 最佳实践

1. **启用 TPM**: 始终使用 TPM 增强安全性
2. **定期轮换密钥**: 定期更新签名密钥
3. **验证 PCR 值**: 定期检查 PCR 值完整性
4. **安全存储**: 安全存储私钥和敏感数据
5. **审计日志**: 记录所有启动相关事件

## 相关文档

- [安全架构](./13-SecurityArchitecture.md) - 安全架构
- [模块管理](./21-ModuleManager.md) - 模块签名
- [审计日志](./14-AuditLogging.md) - 审计机制

---

*最后更新: 2026-02-14*