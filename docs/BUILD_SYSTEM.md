<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC系统构建系统说明

## 概述

HIC系统构建系统提供了一套完整的构建工具，支持：

1. **自动签名** - 构建时自动生成签名密钥并签名所有组件
2. **多目标构建** - 支持UEFI、BIOS引导程序和内核的独立构建
3. **构建报告** - 自动生成详细的构建报告和哈希值
4. **双版本支持** - 提供Python和Shell两种实现

## 快速开始

### 方法1：使用主Makefile（推荐）

```bash
# 构建所有组件（自动签名）
make build-system

# 仅构建UEFI引导程序
make build-uefi

# 仅构建BIOS引导程序
make build-bios

# 仅构建内核
make build-kernel

# 清理构建文件
make clean-system
```

### 方法2：直接使用Python构建系统

```bash
# 构建所有组件
./build_system.py

# 构建特定目标
./build_system.py --target uefi
./build_system.py --target bios
./build_system.py --target kernel

# 构建多个目标
./build_system.py --target uefi bios

# 清理构建
./build_system.py --clean

# 查看帮助
./build_system.py --help
```

### 方法3：直接使用Shell构建系统

```bash
# 构建所有组件
./build_system.sh

# 构建特定目标
./build_system.sh --target uefi
./build_system.sh --target bios
./build_system.sh --target kernel

# 清理构建
./build_system.sh --clean

# 查看帮助
./build_system.sh --help
```

## 输出结构

构建完成后，所有输出文件位于 `output/` 目录：

```
output/
├── EFI/
│   └── BOOT/
│       └── bootx64.efi          # UEFI引导程序
├── bios.bin                     # BIOS引导程序
├── kernel.elf                   # 内核映像
├── kernel.sig.json              # 内核签名信息
├── build_report.json            # 构建报告
└── build.log                    # 构建日志
```

## 签名系统

### 自动密钥生成

构建系统会自动在 `build/` 目录下生成签名密钥对：

- `signing_key.pem` - RSA-4096私钥
- `signing_cert.pem` - 自签名证书（有效期10年）

### 签名过程

构建系统会自动对以下组件进行签名：

1. **内核** (`kernel.elf`)
   - 使用SHA-384计算哈希
   - 使用RSA-4096进行签名
   - 生成签名信息文件 `kernel.sig.json`

### 签名信息格式

```json
{
  "version": "0.1.0",
  "timestamp": "2026-02-14T12:00:00+08:00",
  "algorithm": "RSA-4096",
  "hash": "SHA-384",
  "file_hash": "abc123...",
  "signature_size": 512,
  "signature": "base64_encoded_signature..."
}
```

## 构建报告

构建报告 (`build_report.json`) 包含以下信息：

```json
{
  "project": "HIC System",
  "version": "0.1.0",
  "build_time": "2026-02-14T12:00:00+08:00",
  "build_type": "full",
  "components": {
    "uefi_bootloader": {
      "path": "EFI/BOOT/bootx64.efi",
      "size": 25600,
      "hash": "sha384_hash..."
    },
    "bios_bootloader": {
      "path": "bios.bin",
      "size": 51200,
      "hash": "sha384_hash..."
    },
    "kernel": {
      "path": "kernel.elf",
      "size": 1048576,
      "hash": "sha384_hash..."
    }
  }
}
```

## 依赖要求

### 必需依赖

- `gcc` - GNU C编译器
- `make` - 构建工具
- `objcopy` - 二进制文件操作
- `objdump` - 反汇编工具

### 可选依赖

- `x86_64-w64-mingw32-gcc` - UEFI引导程序编译
- `x86_64-elf-gcc` - 内核交叉编译
- `openssl` - 签名功能
- `python3` - Python构建系统
- `bash` - Shell构建系统

### 安装依赖（Arch Linux）

```bash
# 安装必需依赖
sudo pacman -S --needed base-devel git

# 安装UEFI工具
sudo pacman -S mingw-w64-gcc gnu-efi

# 安装内核交叉编译工具
sudo pacman -S cross-x86_64-elf-gcc

# 安装签名工具
sudo pacman -S openssl

# 安装Python
sudo pacman -S python3
```

## 高级用法

### 自定义构建目录

可以通过修改 `Build.conf` 文件来自定义构建目录：

```conf
BUILD_DIR = /path/to/custom/build
OUTPUT_DIR = /path/to/custom/output
```

### 仅重新签名已有构建

如果只想对已构建的文件进行签名，可以：

```bash
# 使用Python
python3 -c "
from build_system import BuildSystem
bs = BuildSystem()
bs.sign_file(Path('output/kernel.elf'), Path('output/kernel.sig.json'))
"

# 或使用Shell
openssl dgst -sha384 -sign build/signing_key.pem -out kernel.sig output/kernel.elf
```

### 验证签名

```bash
# 验证内核签名
openssl dgst -sha384 -verify build/signing_cert.pem \
    -signature kernel.sig output/kernel.elf
```

## 故障排除

### 问题1：缺少签名工具

**错误信息**：`警告: 未找到 openssl，无法进行签名操作`

**解决方案**：
```bash
sudo pacman -S openssl  # Arch Linux
sudo apt install openssl  # Ubuntu/Debian
```

### 问题2：缺少交叉编译工具

**错误信息**：`警告: 未找到 x86_64-elf-gcc，无法构建内核`

**解决方案**：
```bash
# Arch Linux
sudo pacman -S cross-x86_64-elf-gcc

# Ubuntu/Debian
sudo apt install gcc-x86-64-linux-gnu
```

### 问题3：签名密钥已存在

**现象**：提示"密钥文件已存在，跳过生成"

**解决方案**：这是正常行为，构建系统会复用已有密钥。如需重新生成，请：
```bash
rm -f build/signing_key.pem build/signing_cert.pem
make clean-system
make build-system
```

## 与旧构建系统的对比

| 特性 | 新构建系统 | 旧构建系统 |
|------|-----------|-----------|
| 自动签名 | ✅ 支持 | ❌ 不支持 |
| 构建报告 | ✅ 自动生成 | ❌ 无 |
| 密钥管理 | ✅ 自动生成 | ❌ 无 |
| Python支持 | ✅ | ❌ |
| Shell支持 | ✅ | ✅ |
| 兼容性 | ✅ 新系统 | ✅ 旧系统 |

## 安全建议

1. **保护私钥** - `build/signing_key.pem` 包含私钥，应妥善保管
2. **验证签名** - 部署前验证所有组件的签名
3. **使用可信源** - 确保构建工具链来自可信源
4. **定期更新** - 定期更新密钥和证书

## 技术细节

### 签名算法

- **算法**：RSA-4096
- **哈希**：SHA-384
- **填充**：PKCS#1 v1.5（OpenSSL默认）

### 构建流程

1. 检查依赖
2. 生成/加载签名密钥
3. 构建目标组件
4. 计算组件哈希
5. 签名组件
6. 生成签名信息
7. 创建输出目录
8. 复制文件到输出目录
9. 生成构建报告

### 哈希计算

所有组件使用SHA-384算法计算哈希值，确保完整性验证。

## 联系方式

如有问题或建议，请提交Issue或Pull Request。