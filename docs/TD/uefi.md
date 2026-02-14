# x86_64引导加载程序设计

基于HIK架构需求，以下是x86_64平台引导加载程序的详细设计方案：

## 一、总体架构

### 1.1 多阶段引导流程
```
UEFI/BIOS → HIK Bootloader (多阶段) → HIK Core-0
    │               │
    ├─ UEFI模式     ├─ BIOS模式
    │   (UEFI应用)   │   (MBR+引导扇区)
    └─ 安全启动      └─ 传统启动
```

## 二、UEFI引导模式（现代系统首选）

### 2.1 UEFI应用设计

**入口点签名：**
```c
EFI_STATUS EFIAPI
UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
```

**执行流程：**
1. **初始化UEFI服务**
   - 保存SystemTable和ImageHandle
   - 初始化控制台输出（ConOut）
   - 获取内存映射服务（BootServices）

2. **加载配置文件**
   - 从`\EFI\HIK\boot.conf`读取引导配置
   - 解析启动项、内核路径、启动参数

3. **安全启动验证**
   - 检查UEFI安全启动状态
   - 验证引导加载程序自身签名
   - 准备内核验证公钥

4. **加载内核映像**
   - 打开内核文件`\EFI\HIK\kernel.hik`
   - 读取文件头，验证魔数(`HIK_IMG`)
   - 验证数字签名（RSA-3072 + SHA-384）

5. **解析内核结构**
   - 读取段表：代码段、数据段、配置段
   - 检查架构兼容性（必须为x86_64）
   - 获取内核入口点地址

6. **准备启动环境**
   - 获取UEFI内存映射
   - 保留内核所需内存区域
   - 设置图形模式（可选）
   - 初始化串口调试（如果启用）

7. **传递启动信息**
   - 构建HIK Boot Information Structure (BIS)
   - 包含：内存映射、ACPI表指针、SMBIOS表、UEFI System Table

8. **退出启动服务**
   - 调用`ExitBootServices()`
   - 此后再无法使用UEFI服务

9. **跳转到内核**
   - 设置长模式环境
   - 禁用分页（内核自设页表）
   - 禁用中断
   - 跳转到内核入口点

### 2.2 UEFI安全启动集成

**实现要点：**
- 支持Microsoft UEFI签名数据库
- 可选集成TPM 2.0度量启动
- 支持自定义签名证书
- 实现内核回滚保护
