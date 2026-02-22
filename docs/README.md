<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC内核实现

## 项目概述

HIC (Hierarchical Isolation Core) 是一个分级隔离内核，根据三层模型文档实现。

## 架构说明

### Core-0层 (内核核心)
- **职责**: 系统仲裁者、资源管理、能力系统
- **特权级**: x86 Ring 0
- **代码行数限制**: <10,000行C代码

### Privileged-1层 (特权服务沙箱)
- **职责**: 模块化服务提供者
- **特权级**: x86 Ring 0 (物理隔离)
- **内存映射**: 物理空间直接映射

### Application-3层 (应用层)
- **职责**: 不可信用户代码
- **特权级**: x86 Ring 3
- **内存**: 虚拟地址空间

## 目录结构

```
kernel/
├── include/          # 公共头文件
│   ├── types.h       # 基础类型
│   ├── capability.h  # 能力系统
│   ├── domain.h      # 域管理
│   ├── thread.h      # 线程管理
│   └── pmm.h         # 物理内存管理
├── core/             # Core-0实现
│   ├── main.c        # 内核入口
│   └── capability.c  # 能力系统实现
├── arch/x86_64/      # x86-64架构支持
├── lib/              # 内核库
└── Makefile          # 构建系统
```

## 核心特性

### 1. 能力系统
- 细粒度权限控制
- 动态传递与撤销
- 权限派生与限制

### 2. 物理内存管理
- 直接物理地址映射
- 基于页帧的分配
- 域级配额限制

### 3. 强隔离机制
- MMU硬件隔离
- 能力验证
- 故障隔离

### 4. 模块化服务
- 独立服务沙箱
- 热插拔支持
- 滚动更新

## 编译

### 方式1：使用根目录Makefile（推荐）

```bash
# 构建所有组件
make

# 仅构建引导程序
make bootloader

# 仅构建内核
make kernel

# 清理构建文件
make clean

# 安装构建产物到output目录
make install

# 快速构建
make all && make install
```

### 方式2：使用根目录构建脚本

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
```

### 方式3：使用Python构建系统

```bash
# 运行Python构建系统（自动选择GUI/TUI/CLI）
python3 scripts/build_system.py

# 运行GUI构建界面
python3 scripts/build_gui.py

# 运行TUI构建界面
python3 scripts/build_tui.py
```

### 方式4：直接进入各目录构建

```bash
# 构建引导程序
cd src/bootloader
make clean
make all

# 构建内核
cd build
make clean
make all
```

## 输出

构建产物位于 `output/` 目录：
- `bootx64.efi` - UEFI引导程序
- `bios.bin` - BIOS引导程序
- `hic-kernel.bin` - 二进制格式内核映像

## 文档

详细设计请参考 `TD/` 目录中的文档：
- `三层模型.md` - 核心架构设计
- `引导加载程序.md` - Bootloader接口
- `可移植性.md` - 跨平台支持

## 许可证

本项目采用 GPL-2.0 许可证。详见 [LICENSE](../LICENSE)。

## 联系方式

- 作者: *
- Email: *@gmail.com
- GitHub: https://github.com/*/HIC

## 版本

当前版本: 0.1.0 (开发中)