#!/bin/bash
# HIC 根目录构建脚本
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

echo -e "${CYAN}HIC 构建系统${RESET}"
echo "=="
echo ""

# 检查参数
if [ "$1"  "--help" ]; then
    echo "用法: ./build.sh [选项]"
    echo ""
    echo "快速启动:"
    echo "  start              - 启动快速构建向导（推荐新手）"
    echo ""
    echo "构建选项:"
    echo "  bootloader   - 仅构建引导程序"
    echo "  kernel       - 仅构建内核"
    echo "  clean        - 清理构建文件"
    echo "  install      - 安装构建产物"
    echo ""
    echo "依赖管理（跨平台）:"
    echo "  deps         - 自动安装所有构建依赖"
    echo "  deps-check   - 检查依赖状态"
    echo "  deps-install - 安装指定依赖"
    echo ""
    echo "界面选项:"
    echo "  gui          - 运行GUI构建界面"
    echo "  tui          - 运行TUI构建界面"
    echo "  cli          - 运行CLI交互式Shell"
    echo "  web          - 运行Web界面"
    echo ""
    echo "配置选项:"
    echo "  cli --config        - 显示当前编译配置"
    echo "  cli --config-runtime - 显示运行时配置说明"
    echo ""
    echo "环境选项:"
    echo "  setup-venv   - 设置Python虚拟环境"
    echo "  clean-venv   - 清理Python虚拟环境"
    echo ""
    echo "其他:"
    echo "  help         - 显示此帮助信息"
    echo ""
    echo "无参数时：构建引导程序和内核"
    exit 0
fi

# 创建output目录
mkdir -p "${SCRIPT_DIR}/output"

# 检查和设置Python虚拟环境
setup_venv() {
    local venv_dir="${SCRIPT_DIR}/scripts/venv"
    local requirements_file="${SCRIPT_DIR}/scripts/requirements.txt"
    
    if [ ! -d "$venv_dir" ]; then
        echo -e "${YELLOW}创建Python虚拟环境...${RESET}"
        /usr/bin/python3 -m venv "$venv_dir"
        
        if [ -f "$requirements_file" ]; then
            echo -e "${YELLOW}安装Python依赖...${RESET}"
            "$venv_dir/bin/pip" install -r "$requirements_file" --quiet
        fi
    fi
}

# 根据参数执行不同操作
case "$1" in
    deps)
        echo -e "${YELLOW}自动安装构建依赖...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/dependency_manager.py"
        ;;

    deps-check)
        echo -e "${YELLOW}检查依赖状态...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/dependency_manager.py" --check
        ;;

    deps-install)
        if [ -z "$2" ]; then
            echo -e "${RED}错误: 请指定要安装的工具${RESET}"
            echo "用法: ./build.sh deps-install <工具1> <工具2> ..."
            exit 1
        fi
        echo -e "${YELLOW}安装指定依赖...${RESET}"
        setup_venv
        shift
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/dependency_manager.py" --install "$@"
        ;;

    start)
        echo -e "${YELLOW}启动快速构建向导...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/quick_start.py"
        ;;
    
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
        if [ -f "${SCRIPT_DIR}/build/bin/hic-kernel.bin" ]; then
            cp "${SCRIPT_DIR}/build/bin/hic-kernel.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 hic-kernel.bin"
        fi
        echo -e "${GREEN}安装完成${RESET}"
        ;;
    
    gui)
        echo -e "${YELLOW}运行GUI构建界面...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/gui_unified.py"
        ;;
    
    tui)
        echo -e "${YELLOW}运行TUI构建界面...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/build_tui.py"
        ;;
    
    cli)
        echo -e "${YELLOW}运行HIC交互式构建系统Shell...${RESET}"
        setup_venv
        "${SCRIPT_DIR}/scripts/venv/bin/python" "${SCRIPT_DIR}/scripts/hic_shell.py"
        ;;
    
    setup-venv)
        echo -e "${YELLOW}设置Python虚拟环境...${RESET}"
        setup_venv
        echo -e "${GREEN}虚拟环境设置完成${RESET}"
        echo "虚拟环境位置: ${SCRIPT_DIR}/scripts/venv"
        ;;
    
    clean-venv)
        echo -e "${YELLOW}清理Python虚拟环境...${RESET}"
        if [ -d "${SCRIPT_DIR}/scripts/venv" ]; then
            rm -rf "${SCRIPT_DIR}/scripts/venv"
            echo -e "${GREEN}虚拟环境已清理${RESET}"
        else
            echo -e "${YELLOW}虚拟环境不存在${RESET}"
        fi
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
        if [ -f "${SCRIPT_DIR}/build/bin/hic-kernel.bin" ]; then
            cp "${SCRIPT_DIR}/build/bin/hic-kernel.bin" "${SCRIPT_DIR}/output/"
            echo "已复制 hic-kernel.bin"
        fi
        echo -e "${GREEN}安装完成${RESET}"
        
        echo ""
        echo -e "${CYAN}==${RESET}"
        echo -e "${GREEN}构建完成！${RESET}"
        echo "输出文件位于: ${SCRIPT_DIR}/output/"
        ;;
    
    *)
        echo -e "${RED}错误: 未知选项 '$1'${RESET}"
        echo "运行 './build.sh help' 查看帮助"
        exit 1
        ;;
esac