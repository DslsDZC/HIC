# 快速开始

本指南将帮助你快速构建和运行HIK系统。

## 前置要求

### 必需工具

```bash
# 检查是否已安装
make --version
python3 --version
gcc --version
```

### 可选工具

#### UEFI引导工具

```bash
# Ubuntu/Debian
sudo apt install gcc-mingw-w64-x86-64 gnu-efi

# Arch Linux
sudo pacman -S mingw-w64-gcc gcc-efi

# Fedora
sudo dnf install mingw64-gcc gnu-efi
```

#### 交叉编译工具链

```bash
# Ubuntu/Debian
sudo apt install gcc-x86-64-elf-binutils gcc-x86-64-elf

# 从源码构建（如果不可用）
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv --enable-multilib
make
```

## 快速构建

### 方式1：使用根目录Makefile（推荐）

```bash
# 在项目根目录（假设项目路径为 $HIK_ROOT）
cd $HIK_ROOT

# 构建所有组件
make all

# 安装构建产物
make install

# 或者一步完成
make all && make install
```

### 方式2：使用构建脚本

```bash
# 在项目根目录
cd $HIK_ROOT

# 构建所有组件
./build.sh
```

### 方式3：使用Python构建系统

```bash
# 在项目根目录
cd $HIK_ROOT

# 运行构建系统（自动选择GUI/TUI/CLI）
python3 scripts/build_system.py
```

## 构建选项

### 仅构建引导程序

```bash
make bootloader
# 或
./build.sh bootloader
```

### 仅构建内核

```bash
make kernel
# 或
./build.sh kernel
```

### 清理构建文件

```bash
make clean
# 或
./build.sh clean
```

### 查看帮助

```bash
make help
# 或
./build.sh help
```

## 输出文件

构建成功后，所有输出文件位于 `output/` 目录：

```
output/
├── bootx64.efi         # UEFI引导程序
├── bios.bin            # BIOS引导程序
└── hik-kernel.bin      # 内核映像
```

## 运行HIK

### 在QEMU中运行（UEFI）

```bash
# 安装QEMU和OVMF
sudo apt install qemu-system-x86 ovmf

# 创建虚拟磁盘
qemu-img create -f raw disk.img 1G

# 启动QEMU
qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_VARS.fd \
  -drive format=raw,file=disk.img \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hik-kernel.bin \
  -m 512M \
  -serial stdio
```

### 在QEMU中运行（BIOS）

```bash
# 启动QEMU（BIOS模式）
qemu-system-x86_64 \
  -drive format=raw,file=output/bios.bin \
  -drive format=raw,file=output/hik-kernel.bin \
  -m 512M \
  -serial stdio
```

### 在真实硬件上运行

#### UEFI系统

1. 将 `output/bootx64.efi` 复制到EFI分区
2. 将 `output/hik-kernel.bin` 复制到适当位置
3. 配置EFI引导加载程序

#### BIOS系统

1. 将 `output/bios.bin` 写入引导扇区
2. 将 `output/hik-kernel.bin` 复制到适当位置
3. 配置引导加载程序

## 验证构建

### 检查引导程序

```bash
# 检查UEFI引导程序
file output/bootx64.efi
# 应该显示: PE32+ executable (DLL) x86-64, for MS Windows

# 检查BIOS引导程序
file output/bios.bin
# 应该显示: x86 boot sector
```

### 检查内核映像

```bash
# 检查内核映像
file output/hik-kernel.bin
# 应该显示: data

# 查看内核大小
ls -lh output/hik-kernel.bin
```

### 检查签名（如果已签名）

```bash
# 检查SHA-384哈希
sha384sum output/bootx64.efi
sha384sum output/hik-kernel.bin

# 验证RSA签名（如果有签名工具）
# openssl dgst -sha384 -verify pubkey.pem -signature output/bootx64.efi.sig output/bootx64.efi
```

## 故障排查

### 构建失败

#### 错误：找不到编译器

```bash
# 解决方案：安装交叉编译工具链
sudo apt install gcc-mingw-w64-x86-64 gnu-efi
```

#### 错误：缺少依赖

```bash
# 解决方案：安装所有依赖
sudo apt install build-essential nasm python3 python3-yaml
```

#### 错误：权限不足

```bash
# 解决方案：确保有写入权限
chmod +x build.sh
```

### 运行失败

#### 错误：QEMU找不到OVMF

```bash
# 解决方案：查找OVMF路径
find /usr -name "OVMF_CODE.fd" 2>/dev/null

# 然后在QEMU命令中使用正确的路径
```

#### 错误：内核无法启动

```bash
# 解决方案：检查串口输出
qemu-system-x86_64 \
  -drive format=raw,file=output/bootx64.efi \
  -drive format=raw,file=output/hik-kernel.bin \
  -m 512M \
  -serial stdio \
  -d int,cpu_reset  # 显示详细调试信息
```

## 下一步

### 学习更多

- [项目概述](./01-Overview.md) - 了解HIK的设计哲学
- [架构设计](./02-Architecture.md) - 深入理解三层模型
- [开发环境](./05-DevelopmentEnvironment.md) - 搭建完整的开发环境

### 开始开发

- [构建系统](./04-BuildSystem.md) - 详细的构建说明
- [代码规范](./06-CodingStandards.md) - 了解代码风格和规范
- [测试指南](./07-Testing.md) - 如何运行测试

### 贡献代码

- [如何贡献](./39-Contributing.md) - 为项目做贡献
- [提交指南](./40-CommitGuidelines.md) - 提交消息规范

## 常见问题

### Q: 为什么需要交叉编译工具链？

A: HIK引导程序需要为UEFI环境编译，UEFI使用PE格式，需要特殊的交叉编译工具链。

### Q: 可以在没有UEFI的系统上运行吗？

A: 可以。HIK支持BIOS引导，使用传统的BIOS启动方式。

### Q: 构建需要多长时间？

A: 在现代CPU上，完整构建通常需要2-5分钟。

### Q: 支持哪些架构？

A: 目前主要支持x86-64。ARM64和RISC-V的支持正在开发中。

### Q: 可以在闭源项目中使用HIK吗？

A: 可以，但根据GPL-2.0许可证，如果您分发基于HIK的修改版本，必须同时提供源代码并使用相同的许可证。

### Q: 如何调试引导程序？

A: 可以使用QEMU的调试功能，或者添加串口输出进行调试。

## 获取帮助

- 查看 [常见问题](./36-FAQ.md)
- 查看 [故障排查](./38-Troubleshooting.md)
- 提交 [Issue](https://github.com/DslsDZC/HIK/issues)
- 加入 [讨论区](https://github.com/DslsDZC/HIK/discussions)

---

*最后更新: 2026-02-14*