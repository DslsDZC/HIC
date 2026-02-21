#!/bin/bash
# HIC构建系统启动器
# 智能选择最佳可用界面，自动处理依赖

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "HIC 构建系统 v0.1.0"
echo "===================="
echo ""

# 检查依赖
echo "检查构建依赖..."
MISSING_DEPS=()

# 检查基本工具
command -v gcc >/dev/null 2>&1 || MISSING_DEPS+=("gcc")
command -v make >/dev/null 2>&1 || MISSING_DEPS+=("make")
command -v python3 >/dev/null 2>&1 || MISSING_DEPS+=("python3")

# 检查UEFI工具
command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1 || MISSING_DEPS+=("mingw-w64-gcc")

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "缺少依赖: ${MISSING_DEPS[*]}"
    echo "请先安装缺少的依赖"
    echo ""
    echo "Arch Linux: sudo pacman -S ${MISSING_DEPS[*]}"
    echo "Ubuntu/Debian: sudo apt-get install ${MISSING_DEPS[*]}"
    exit 1
fi

echo "依赖检查完成 ✓"
echo ""

# 检测显示环境
HAS_DISPLAY_ENV=0
if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "wayland" ] || [ "$XDG_SESSION_TYPE" = "x11" ]; then
    HAS_DISPLAY_ENV=1
    echo "检测到桌面显示环境 ✓"
else
    echo "未检测到桌面显示环境（将使用CLI/TUI界面）"
fi
echo ""

# 检测界面可用性
echo "检测可用界面..."
HAS_QT=0
HAS_GTK=0
HAS_TUI=0

# 检测显示环境
HAS_DISPLAY=0
if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ] || [ "$XDG_SESSION_TYPE" = "wayland" ] || [ "$XDG_SESSION_TYPE" = "x11" ]; then
    HAS_DISPLAY=1
fi

# 检测Qt（仅在有显示环境时）
if [ $HAS_DISPLAY -eq 1 ]; then
    if /usr/bin/python3 -c "import PyQt6" 2>/dev/null || python3 -c "import PyQt6" 2>/dev/null; then
        HAS_QT=1
        echo "  ✓ Qt GUI 可用"
    else
        echo "  ✗ Qt GUI 不可用"
    fi
else
    echo "  ✗ Qt GUI 不可用（无显示环境）"
fi

# 检测GTK（仅在有显示环境时）
if [ $HAS_DISPLAY -eq 1 ]; then
    if /usr/bin/python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2>/dev/null || python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2>/dev/null; then
        HAS_GTK=1
        echo "  ✓ GTK GUI 可用"
    else
        echo "  ✗ GTK GUI 不可用"
    fi
else
    echo "  ✗ GTK GUI 不可用（无显示环境）"
fi

# 检测TUI
if python3 -c "import curses; curses.setupterm()" 2>/dev/null; then
    HAS_TUI=1
    echo "  ✓ TUI 界面可用"
else
    echo "  ✗ TUI 界面不可用"
fi

echo "  ✓ 交互式 CLI 界面可用 (保底)"
echo ""

# 智能处理GUI依赖
if [ $HAS_DISPLAY_ENV -eq 1 ]; then
    # 有显示环境，询问是否安装GUI依赖
    if [ $HAS_QT -eq 0 ] && [ $HAS_GTK -eq 0 ]; then
        echo "检测到桌面环境，但缺少GUI依赖"
        echo ""
        
        # 检测系统类型并给出安装建议
        if command -v pacman >/dev/null 2>&1; then
            PKG_MANAGER="Arch Linux (pacman)"
            INSTALL_CMD="sudo pacman -S python-pyqt6"
        elif command -v apt-get >/dev/null 2>&1; then
            PKG_MANAGER="Ubuntu/Debian (apt)"
            INSTALL_CMD="sudo apt-get install python3-pyqt6"
        elif command -v dnf >/dev/null 2>&1; then
            PKG_MANAGER="Fedora (dnf)"
            INSTALL_CMD="sudo dnf install python3-pyqt6"
        else
            PKG_MANAGER="pip"
            INSTALL_CMD="pip3 install PyQt6"
        fi
        
        echo "是否安装 GUI 依赖以获得更好的构建体验？"
        echo "  [Y] 是 - 安装 PyQt6 (推荐)"
        echo "  [n] 否 - 使用命令行界面"
        echo ""
        echo "系统类型: $PKG_MANAGER"
        echo "安装命令: $INSTALL_CMD"
        echo ""
        read -p "请选择 [Y/n]: " install_gui
        
        if [[ "$install_gui" =~ ^[Yy]$|^$ ]]; then
            echo ""
            echo "正在安装 PyQt6..."
            if command -v pacman >/dev/null 2>&1; then
                sudo pacman -S python-pyqt6 --noconfirm
            elif command -v apt-get >/dev/null 2>&1; then
                sudo apt-get install -y python3-pyqt6
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y python3-pyqt6
            else
                pip3 install PyQt6
            fi
            
            if [ $? -eq 0 ]; then
                echo "PyQt6 安装成功！"
                HAS_QT=1
            else
                echo "PyQt6 安装失败，将使用命令行界面"
            fi
        else
            echo "将使用命令行界面"
        fi
        echo ""
    fi
fi

# 启动构建系统
echo "启动构建系统（自动选择最佳界面）..."
python3 scripts/build_system.py --interface auto