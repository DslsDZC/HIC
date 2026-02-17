<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 构建系统

HIK提供多种构建方式，满足不同场景的需求。

## 构建方式概览

| 方式 | 适用场景 | 优点 | 缺点 |
|------|----------|------|------|
| 根目录Makefile | 快速构建 | 简单直接 | 功能有限 |
| 根目录构建脚本 | 自动化构建 | 功能丰富 | 需要bash |
| Python构建系统 | 高级构建 | GUI/TUI/CLI | 需要Python |
| 直接Makefile | 细粒度控制 | 最大灵活性 | 需要手动管理 |

## 方式1：根目录Makefile

### 基本用法

```bash
# 构建所有组件
make

# 或
make all

# 仅构建引导程序
make bootloader

# 仅构建内核
make kernel

# 清理构建文件
make clean

# 安装构建产物
make install

# 查看帮助
make help
```

### 快速构建

```bash
# 一键构建并安装
make all && make install
```

### Makefile目标说明

| 目标 | 说明 |
|------|------|
| `all` | 构建所有组件（默认） |
| `bootloader` | 仅构建引导程序 |
| `kernel` | 仅构建内核 |
| `clean` | 清理构建文件 |
| `install` | 安装构建产物到output目录 |
| `build` | 运行Python构建系统 |
| `help` | 显示帮助信息 |

## 方式2：根目录构建脚本

### 基本用法

```bash
# 构建所有组件
./build.sh

# 仅构建引导程序
./build.sh bootloader

# 仅构建内核
./build.sh kernel

# 清理构建文件
./build.sh clean

# 安装构建产物
./build.sh install

# 运行GUI构建界面
./build.sh gui

# 运行TUI构建界面
./build.sh tui

# 运行CLI构建界面
./build.sh cli

# 查看帮助
./build.sh help
```

### 脚本参数说明

| 参数 | 说明 |
|------|------|
| 无参数 | 构建所有组件并安装 |
| `bootloader` | 仅构建引导程序 |
| `kernel` | 仅构建内核 |
| `clean` | 清理构建文件 |
| `install` | 安装构建产物 |
| `gui` | 运行GUI构建界面 |
| `tui` | 运行TUI构建界面 |
| `cli` | 运行CLI构建界面 |
| `help` | 显示帮助信息 |

## 方式3：Python构建系统

### 基本用法

```bash
# 运行构建系统（自动选择GUI/TUI/CLI）
python3 scripts/build_system.py

# 运行GUI构建界面
python3 scripts/build_gui.py

# 运行TUI构建界面
python3 scripts/build_tui.py
```

### Python构建系统特性

- **自动检测**: 自动检测构建环境和依赖
- **交互式界面**: 提供GUI、TUI、CLI三种界面
- **并行构建**: 支持多核并行构建
- **增量构建**: 只重新构建修改的文件
- **详细日志**: 提供详细的构建日志

## 方式4：直接Makefile

### 构建引导程序

```bash
cd src/bootloader

# 清理旧的构建文件
make clean

# 构建UEFI引导程序
make efi

# 构建BIOS引导程序
make bios

# 构建所有
make all
```

### 构建内核

```bash
cd build

# 清理旧的构建文件
make clean

# 构建内核
make all

# 查看详细输出
make all V=1
```

## 构建配置

### 环境变量

```bash
# 设置交叉编译器前缀
export PREFIX=x86_64-elf

# 设置优化级别
export OPTIMIZE=-O2

# 设置调试选项
export DEBUG=1

# 设置输出目录
export OUTPUT_DIR=output
```

### 配置文件

#### build/Build.conf

```makefile
# 项目信息
PROJECT = hik-kernel
VERSION = 0.1.0

# 编译器配置
PREFIX ?= x86_64-elf
CC      = $(PREFIX)-gcc
LD      = $(PREFIX)-ld

# 优化选项
CFLAGS = -O2 -Wall -Wextra

# 调试选项
ifdef DEBUG
CFLAGS += -g -DDEBUG
endif
```

#### build/platform.yaml

```yaml
# 硬件配置
platform:
  architecture: x86_64
  cpu_count: 4
  memory_size: "512M"

# 服务配置
services:
  - name: core0
    type: kernel
    priority: 0
  - name: network
    type: driver
    priority: 1

# 安全配置
security:
  secure_boot: true
  signature_algorithm: RSA-3072
  hash_algorithm: SHA-384
```

## 构建产物

### 目录结构

```
output/
├── bootx64.efi         # UEFI引导程序
├── bios.bin            # BIOS引导程序
├── hik-kernel.bin      # 内核映像
└── build.log           # 构建日志
```

### 文件说明

| 文件 | 说明 | 格式 |
|------|------|------|
| `bootx64.efi` | UEFI引导程序 | PE32+ |
| `bios.bin` | BIOS引导程序 | 二进制 |
| `hik-kernel.bin` | 内核映像 | 二进制 |
| `build.log` | 构建日志 | 文本 |

## 并行构建

### 使用Make的并行构建

```bash
# 使用4个核心并行构建
make -j4

# 使用所有可用核心
make -j$(nproc)
```

### 使用Python构建系统的并行构建

Python构建系统自动支持并行构建。

## 增量构建

### Make的增量构建

```bash
# 只重新构建修改的文件
make

# 强制重新构建
make clean
make
```

### Python构建系统的增量构建

Python构建系统自动支持增量构建。

## 清理构建

### 清理所有构建文件

```bash
make clean
# 或
./build.sh clean
```

### 清理特定目录

```bash
# 清理引导程序构建文件
cd src/bootloader && make clean

# 清理内核构建文件
cd build && make clean
```

### 清理输出文件

```bash
rm -rf output/*
```

## 构建调试

### 查看详细输出

```bash
# Make详细输出
make V=1

# Python构建系统详细输出
python3 scripts/build_system.py --verbose
```

### 查看构建日志

```bash
# 查看构建日志
cat output/build.log

# 实时查看构建日志
make all 2>&1 | tee output/build.log
```

### 调试构建错误

```bash
# 停止在第一个错误
make -k

# 只显示错误
make 2>&1 | grep -i error
```

## 构建优化

### 使用ccache加速

```bash
# 安装ccache
sudo apt install ccache

# 配置ccache
export CC="ccache gcc"
export CXX="ccache g++"

# 然后正常构建
make
```

### 使用编译缓存

```bash
# 设置编译缓存目录
export CCACHE_DIR=/path/to/cache

# 限制缓存大小
export CCACHE_MAXSIZE=5G
```

## CI/CD集成

### GitHub Actions示例

```yaml
name: Build HIK

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y gcc-mingw-w64-x86-64 gnu-efi
      - name: Build
        run: |
          make all
          make install
      - name: Upload artifacts
        uses: actions/upload-artifact@v2
        with:
          name: hik-build
          path: output/
```

### GitLab CI示例

```yaml
build:
  image: ubuntu:latest
  script:
    - apt update
    - apt install -y gcc-mingw-w64-x86-64 gnu-efi
    - make all
    - make install
  artifacts:
    paths:
      - output/
```

## 常见问题

### Q: 构建失败，提示找不到编译器？

A: 安装交叉编译工具链：`sudo apt install gcc-mingw-w64-x86-64 gnu-efi`

### Q: 如何加速构建？

A: 使用并行构建：`make -j$(nproc)` 或使用ccache

### Q: 如何自定义编译选项？

A: 修改 `build/Build.conf` 或设置环境变量

### Q: 构建产物在哪里？

A: 所有构建产物位于 `output/` 目录

### Q: 如何清理构建文件？

A: 运行 `make clean` 或 `./build.sh clean`

## 参考资源

- [Makefile文档](https://www.gnu.org/software/make/manual/)
- [GNU Make最佳实践](https://clarkgrubb.com/makefile-style-guide)
- [Python构建工具](https://docs.python.org/3/library/subprocess.html)

---

*最后更新: 2026-02-14*