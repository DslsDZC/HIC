<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 开发环境

本文档介绍如何搭建HIC的开发环境。

## 系统要求

### 最低要求

- **操作系统**: Linux (Ubuntu 20.04+, Arch Linux, Fedora 33+)
- **CPU**: x86-64 架构，支持64位
- **内存**: 至少 4GB RAM (推荐 8GB+)
- **磁盘空间**: 至少 10GB 可用空间
- **网络**: 互联网连接（用于下载依赖）

### 推荐配置

- **操作系统**: Ubuntu 22.04 LTS 或 Arch Linux
- **CPU**: 多核处理器 (4核心+)
- **内存**: 8GB+ RAM
- **磁盘空间**: 20GB+ SSD
- **网络**: 稳定的互联网连接

## 必需工具

### 基础工具

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y \
    build-essential \
    make \
    python3 \
    python3-pip \
    git \
    nasm \
    qemu-system-x86 \
    ovmf

# Arch Linux
sudo pacman -S --needed \
    base-devel \
    make \
    python \
    python-pip \
    git \
    nasm \
    qemu-system-x86 \
    ovmf

# Fedora
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    python3 \
    python3-pip \
    git \
    nasm \
    qemu-system-x86 \
    edk2-ovmf
```

### 交叉编译工具链

#### UEFI交叉编译工具链

```bash
# Ubuntu/Debian
sudo apt install -y gcc-mingw-w64-x86-64 gnu-efi

# Arch Linux
sudo pacman -S --needed mingw-w64-gcc gcc-efi

# Fedora
sudo dnf install -y mingw64-gcc mingw64-headers
```

#### 内核交叉编译工具链

```bash
# Ubuntu/Debian
sudo apt install -y gcc-x86-64-elf-binutils gcc-x86-64-elf

# Arch Linux
# AUR: x86_64-elf-gcc
yay -S x86_64-elf-gcc

# Fedora
sudo dnf install -y x86_64-elf-gcc x86_64-elf-binutils
```

### 从源码构建交叉编译工具链（如果不可用）

```bash
# 克隆工具链仓库
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain

# 配置
./configure --prefix=/opt/riscv --enable-multilib

# 构建（需要较长时间）
make

# 添加到PATH
echo 'export PATH=/opt/riscv/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

## 可选工具

### 调试工具

```bash
# GDB
sudo apt install -y gdb

# QEMU调试支持
sudo apt install -y qemu-system-x86-debuginfo

# 调试器前端
sudo apt install -y cgdb ddd
```

### 代码分析工具

```bash
# 静态分析
sudo apt install -y cppcheck clang-tidy

# 代码格式化
sudo apt install -y clang-format

# 代码覆盖率
sudo apt install -y gcov lcov
```

### 文档工具

```bash
# Markdown渲染
sudo apt install -y pandoc

# 图表生成
sudo apt install -y graphviz

# LaTeX（用于数学证明）
sudo apt install -y texlive-full
```

## Python依赖

```bash
# 安装Python包
pip3 install --user \
    pyyaml \
    rich \
    inquirer \
    tqdm
```

或使用requirements.txt：

```bash
# 创建requirements.txt
cat > requirements.txt << EOF
pyyaml>=6.0
rich>=13.0
inquirer>=3.0
tqdm>=4.65
EOF

# 安装
pip3 install --user -r requirements.txt
```

## 环境变量配置

### 设置环境变量

```bash
# 添加到 ~/.bashrc 或 ~/.zshrc
cat >> ~/.bashrc << 'EOF'

# HIC开发环境变量
export HIC_ROOT=/path/to/HIC
export HIC_BUILD=$HIC_ROOT/build
export HIC_OUTPUT=$HIC_ROOT/output
export HIC_SRC=$HIC_ROOT/src

# 交叉编译工具链
export PREFIX=x86_64-elf
export CC=$PREFIX-gcc
export LD=$PREFIX-ld
export OBJCOPY=$PREFIX-objcopy
export OBJDUMP=$PREFIX-objdump

# UEFI工具链
export MINGW_PREFIX=x86_64-w64-mingw32
export MINGW_CC=$MINGW_PREFIX-gcc
export MINGW_LD=$MINGW_PREFIX-ld

# 调试选项
export DEBUG=1

# 优化选项
export OPTIMIZE=-O2

# 路径
export PATH=$HIC_ROOT/scripts:$PATH
EOF

# 重新加载配置
source ~/.bashrc
```

## IDE配置

### VS Code

安装扩展：

```bash
# C/C++扩展
code --install-extension ms-vscode.cpptools

# Python扩展
code --install-extension ms-python.python

# Makefile扩展
code --install-extension ms-vscode.makefile-tools

# Git扩展
code --install-extension eamodio.gitlens
```

配置 `.vscode/settings.json`：

```json
{
    "files.associations": {
        "*.h": "c",
        "*.c": "c",
        "*.S": "asm",
        "*.asm": "asm"
    },
    "C_Cpp.default.configurationProvider": "ms-vscode.makefile-tools",
    "C_Cpp.default.intelliSenseMode": "gcc-x64",
    "C_Cpp.default.compilerPath": "/usr/bin/gcc",
    "makefile.extensionOutputFolder": "./.vscode/makefile"
}
```

配置 `.vscode/tasks.json`：

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build All",
            "type": "shell",
            "command": "make",
            "args": ["all"],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Build Bootloader",
            "type": "shell",
            "command": "make",
            "args": ["bootloader"],
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Build Kernel",
            "type": "shell",
            "command": "make",
            "args": ["kernel"],
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Clean",
            "type": "shell",
            "command": "make",
            "args": ["clean"],
            "problemMatcher": []
        },
        {
            "label": "Run in QEMU (UEFI)",
            "type": "shell",
            "command": "qemu-system-x86_64",
            "args": [
                "-bios", "/usr/share/OVMF/OVMF_CODE.fd",
                "-drive", "format=raw,file=${workspaceFolder}/output/bootx64.efi",
                "-drive", "format=raw,file=${workspaceFolder}/output/hic-kernel.bin",
                "-m", "512M",
                "-serial", "stdio"
            ],
            "group": "test",
            "problemMatcher": []
        }
    ]
}
```

### Vim/Neovim

配置 `.vimrc` 或 `init.vim`：

```vim
" C语言配置
autocmd FileType c setlocal tabstop=4 shiftwidth=4 expandtab
autocmd FileType asm setlocal tabstop=8 shiftwidth=8 noexpandtab

" 语法高亮
syntax on

" 行号
set number

" 自动缩进
set autoindent
set smartindent

" 搜索高亮
set hlsearch
set incsearch

" 显示匹配的括号
set showmatch

" 编译命令
nnoremap <F5> :make<CR>
nnoremap <F6> :make clean<CR>
nnoremap <F7> :make install<CR>
```

### Emacs

配置 `.emacs` 或 `init.el`：

```elisp
;; C语言配置
(add-hook 'c-mode-hook
          (lambda ()
            (setq c-default-style "linux")
            (setq indent-tabs-mode nil)
            (setq tab-width 4)
            (setq c-basic-offset 4)))

;; 汇编语言配置
(add-hook 'asm-mode-hook
          (lambda ()
            (setq indent-tabs-mode t)
            (setq tab-width 8)))

;; 编译命令
(global-set-key (kbd "<f5>") 'compile)
```

## 验证安装

### 验证基础工具

```bash
# 检查GCC
gcc --version

# 检查Make
make --version

# 检查Python
python3 --version

# 检查NASM
nasm --version

# 检查QEMU
qemu-system-x86_64 --version
```

### 验证交叉编译工具链

```bash
# 检查x86_64-elf-gcc
x86_64-elf-gcc --version

# 检查mingw-w64-gcc
x86_64-w64-mingw32-gcc --version

# 检查gnu-efi
pkg-config --modversion gnu-efi
```

### 验证Python包

```bash
python3 -c "import yaml; print(yaml.__version__)"
python3 -c "import rich; print(rich.__version__)"
```

### 测试构建

```bash
# 克隆项目（如果还没有）
git clone https://github.com/*/HIC.git
cd hic

# 测试构建
make clean
make all

# 如果成功，应该看到输出文件
ls -lh output/
```

## 常见问题

### Q: 找不到交叉编译工具链？

A: 根据你的发行版安装相应的包。如果不可用，可以从源码构建。

### Q: QEMU找不到OVMF？

A: 安装OVMF包，然后查找正确的路径：
```bash
find /usr -name "OVMF_CODE.fd" 2>/dev/null
```

### Q: Python包安装失败？

A: 尝试使用虚拟环境：
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Q: 权限不足？

A: 确保有正确的权限：
```bash
chmod +x build.sh
chmod +x scripts/*.sh
```

## 性能优化

### 使用ccache加速编译

```bash
# 安装ccache
sudo apt install ccache

# 配置环境变量
export CC="ccache gcc"
export CXX="ccache g++"
export PATH="/usr/lib/ccache:$PATH"

# 配置ccache
ccache -M 10G  # 最大缓存10GB
ccache -s      # 查看统计信息
```

### 并行编译

```bash
# 使用所有CPU核心
make -j$(nproc)

# 指定核心数
make -j4
```

## 下一步

环境配置完成后，你可以：

1. 阅读 [快速开始](./03-QuickStart.md) - 开始构建和运行HIC
2. 阅读 [代码规范](./06-CodingStandards.md) - 了解代码风格
3. 阅读 [测试指南](./07-Testing.md) - 学习如何测试
4. 开始开发你的第一个功能或修复

---

*最后更新: 2026-02-14*