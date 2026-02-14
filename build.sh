#!/bin/bash
# HIK 根目录构建脚本
# 支持在根目录直接运行 ./build.sh

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${CYAN}HIK 构建系统${RESET}"
echo "===================="
echo ""

# 检查参数
if [ "$1" = "help" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "用法: ./build.sh [选项]"
    echo ""
    echo "选项:"
    echo "  bootloader   - 仅构建引导程序"
    echo "  kernel       - 仅构建内核"
    echo "  clean        - 清理构建文件"
    echo "  install      - 安装构建产物"
    echo "  gui          - 运行GUI构建界面"
    echo "  tui          - 运行TUI构建界面"
    echo "  cli          - 运行CLI构建界面"
    echo "  help         - 显示此帮助信息"
    echo ""
    echo "无参数时：构建引导程序和内核"
    exit 0
fi

# 创建output目录
mkdir -p "${SCRIPT_DIR}/output"

# 根据参数执行不同操作
case "$1" in
    bootloader)
        echo -e "${YELLOW}构建引导程序...${RESET}"
        cd "${SCRIPT_DIR}/src/bootloader"
        make clean
        make all
        echo -e "${GREEN}引导程序构建完成${RESET}"
        ;;
    
    kernel)
        echo -e "${YELLOW}构建内核...${RESET}"
        cd "${SCRIPT_DIR}/build"
        make clean
        make all
        echo -e "${GREEN}内核构建完成${RESET}"
        ;;
    
    clean)
        echo -e "${YELLOW}清理构建文件...${RESET}"
        cd "${SCRIPT_DIR}/src/bootloader"
        make clean || true
        cd "${SCRIPT_DIR}/build"
        make clean || true
        rm -rf "${SCRIPT_DIR}/output/"*
        echo -e "${GREEN}清理完成${RESET}"
        ;;
    
    install)
        echo -e "${YELLOW}安装构建产物...${RESET}"
        mkdir -p "${SCRIPT_DIR}/output"
        if [ -f "${SCRIPT_DIR}/src/bootloader/bin/bootx64.efi" ]; then
            cp "${SCRIPT_DIR}/src/bootloader/bin/bootx64.efi" "${SCRIPT_DIR}/output/"
            echo "已复制 bootx64.efi"
        fi
        if [ -f "${SCRIPT_DIR}/src/bootloader/bin/bios.bin" ]; then
            cp "${SCRIPT_DIR}/src/bootloader/bin/bios.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 bios.bin"
        fi
        if [ -f "${SCRIPT_DIR}/build/bin/hik-kernel.bin" ]; then
            cp "${SCRIPT_DIR}/build/bin/hik-kernel.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 hik-kernel.bin"
        fi
        echo -e "${GREEN}安装完成${RESET}"
        ;;
    
    gui)
        echo -e "${YELLOW}运行GUI构建界面...${RESET}"
        cd "${SCRIPT_DIR}"
        python3 scripts/build_gui.py
        ;;
    
    tui)
        echo -e "${YELLOW}运行TUI构建界面...${RESET}"
        cd "${SCRIPT_DIR}"
        python3 scripts/build_tui.py
        ;;
    
    cli)
        echo -e "${YELLOW}运行CLI构建界面...${RESET}"
        cd "${SCRIPT_DIR}"
        python3 scripts/build_system.py
        ;;
    
    "")
        echo -e "${YELLOW}构建引导程序...${RESET}"
        cd "${SCRIPT_DIR}/src/bootloader"
        make clean
        make all
        echo -e "${GREEN}引导程序构建完成${RESET}"
        
        echo ""
        echo -e "${YELLOW}构建内核...${RESET}"
        cd "${SCRIPT_DIR}/build"
        make clean
        make all
        echo -e "${GREEN}内核构建完成${RESET}"
        
        echo ""
        echo -e "${YELLOW}安装构建产物...${RESET}"
        mkdir -p "${SCRIPT_DIR}/output"
        if [ -f "${SCRIPT_DIR}/src/bootloader/bin/bootx64.efi" ]; then
            cp "${SCRIPT_DIR}/src/bootloader/bin/bootx64.efi" "${SCRIPT_DIR}/output/"
            echo "已复制 bootx64.efi"
        fi
        if [ -f "${SCRIPT_DIR}/src/bootloader/bin/bios.bin" ]; then
            cp "${SCRIPT_DIR}/src/bootloader/bin/bios.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 bios.bin"
        fi
        if [ -f "${SCRIPT_DIR}/build/bin/hik-kernel.bin" ]; then
            cp "${SCRIPT_DIR}/build/bin/hik-kernel.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 hik-kernel.bin"
        fi
        echo -e "${GREEN}安装完成${RESET}"
        
        echo ""
        echo -e "${CYAN}====================${RESET}"
        echo -e "${GREEN}构建完成！${RESET}"
        echo "输出文件位于: ${SCRIPT_DIR}/output/"
        ;;
    
    *)
        echo -e "${RED}错误: 未知选项 '$1'${RESET}"
        echo "运行 './build.sh help' 查看帮助"
        exit 1
        ;;
esac