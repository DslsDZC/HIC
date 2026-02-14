#!/bin/bash
# HIK系统构建脚本
# 支持命令行、文本GUI、图形化GUI三种构建方式
# 遵循TD/滚动更新.md文档

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[0;37m'
RESET='\033[0m'

# 项目信息
PROJECT="HIK System"
VERSION="0.1.0"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${OUTPUT_DIR}"

# 显示横幅
show_banner() {
    echo "${MAGENTA}========================================${RESET}"
    echo "${MAGENTA}HIK系统构建系统 v${VERSION}${RESET}"
    echo "${MAGENTA}========================================${RESET}"
    echo ""
}

# 显示菜单
show_menu() {
    echo "${GREEN}请选择构建方式:${RESET}"
    echo "  ${CYAN}1${RESET} - 命令行模式构建"
    echo "  ${CYAN}2${RESET} - 文本GUI模式构建"
    echo "  ${CYAN}3${RESET} - 图形化GUI模式构建"
    echo "  ${CYAN}4${RESET} - 清理构建文件"
    echo "  ${CYAN}5${RESET} - 安装依赖 (Arch Linux)"
    echo "  ${CYAN}6${RESET} - 显示帮助"
    echo "  ${CYAN}0${RESET} - 退出"
    echo ""
}

# 检查依赖
check_dependencies() {
    echo "${CYAN}检查构建依赖...${RESET}"
    
    if ! command -v gcc &> /dev/null; then
        echo "${RED}错误: 未找到 gcc${RESET}"
        return 1
    fi
    
    if ! command -v make &> /dev/null; then
        echo "${RED}错误: 未找到 make${RESET}"
        return 1
    fi
    
    echo "${GREEN}依赖检查完成${RESET}"
    return 0
}

# 命令行模式构建
build_console() {
    echo "${CYAN}执行命令行模式构建...${RESET}"
    check_dependencies || exit 1
    
    cd "${ROOT_DIR}"
    make BUILD_TYPE=console clean
    make BUILD_TYPE=console
    
    echo "${GREEN}命令行模式构建完成!${RESET}"
    echo "${CYAN}输出目录: ${OUTPUT_DIR}${RESET}"
}

# 文本GUI模式构建
build_tui() {
    echo "${CYAN}执行文本GUI模式构建...${RESET}"
    
    if ! command -v ncurses6-config &> /dev/null; then
        echo "${RED}错误: 需要安装 ncurses${RESET}"
        echo "${YELLOW}运行: sudo pacman -S ncurses${RESET}"
        return 1
    fi
    
    check_dependencies || exit 1
    
    cd "${ROOT_DIR}"
    make BUILD_TYPE=tui clean
    make BUILD_TYPE=tui
    
    echo "${GREEN}文本GUI模式构建完成!${RESET}"
}

# 图形化GUI模式构建
build_gui() {
    echo "${CYAN}执行图形化GUI模式构建...${RESET}"
    
    if ! command -v gtk3 &> /dev/null; then
        echo "${RED}错误: 需要安装 gtk3${RESET}"
        echo "${YELLOW}运行: sudo pacman -S gtk3${RESET}"
        return 1
    fi
    
    check_dependencies || exit 1
    
    cd "${ROOT_DIR}"
    make BUILD_TYPE=gui clean
    make BUILD_TYPE=gui
    
    echo "${GREEN}图形化GUI模式构建完成!${RESET}"
}

# 清理构建
clean_build() {
    echo "${CYAN}清理构建文件...${RESET}"
    cd "${ROOT_DIR}"
    make clean
    echo "${GREEN}清理完成${RESET}"
}

# 安装依赖
install_dependencies() {
    echo "${CYAN}安装构建依赖 (Arch Linux)...${RESET}"
    
    if [[ ! -f /etc/arch-release ]]; then
        echo "${RED}错误: 此脚本仅适用于 Arch Linux${RESET}"
        return 1
    fi
    
    sudo pacman -S --needed base-devel git mingw-w64-gcc gnu-efi ncurses gtk3
    
    echo "${GREEN}依赖安装完成${RESET}"
    echo "${YELLOW}注意: 内核交叉编译工具链需要手动安装:${RESET}"
    echo "${YELLOW}  https://archlinux.org/packages/extra/x86_64/cross-x86_64-elf-gcc/${RESET}"
}

# 显示帮助
show_help() {
    echo "${GREEN}HIK系统构建系统帮助${RESET}"
    echo ""
    echo "${CYAN}用法: ./build.sh [选项]${RESET}"
    echo ""
    echo "${GREEN}选项:${RESET}"
    echo "  ${CYAN}--console${RESET}         - 命令行模式构建"
    echo "  ${CYAN}--tui${RESET}             - 文本GUI模式构建"
    echo "  ${CYAN}--gui${RESET}             - 图形化GUI模式构建"
    echo "  ${CYAN}--clean${RESET}           - 清理构建文件"
    echo "  ${CYAN}--deps${RESET}            - 安装依赖"
    echo "  ${CYAN}--help${RESET}            - 显示此帮助"
    echo ""
    echo "${GREEN}交互模式:${RESET}"
    echo "  直接运行 ./build.sh 进入交互式菜单"
    echo ""
    echo "${GREEN}配置文件:${RESET}"
    echo "  ${CYAN}Build.conf${RESET}        - 构建配置文件"
    echo ""
    echo "${GREEN}构建类型:${RESET}"
    echo "  ${CYAN}console${RESET}           - 命令行模式 (默认)"
    echo "  ${CYAN}tui${RESET}               - 文本GUI模式 (需要 ncurses)"
    echo "  ${CYAN}gui${RESET}               - 图形化GUI模式 (需要 gtk3)"
    echo ""
}

# 主函数
main() {
    show_banner
    
    # 检查命令行参数
    case "${1:-}" in
        --console)
            build_console
            ;;
        --tui)
            build_tui
            ;;
        --gui)
            build_gui
            ;;
        --clean)
            clean_build
            ;;
        --deps)
            install_dependencies
            ;;
        --help|-h)
            show_help
            ;;
        "")
            # 交互式菜单
            while true; do
                show_menu
                read -p "请输入选项 [0-6]: " choice
                
                case "$choice" in
                    1)
                        build_console
                        ;;
                    2)
                        build_tui
                        ;;
                    3)
                        build_gui
                        ;;
                    4)
                        clean_build
                        ;;
                    5)
                        install_dependencies
                        ;;
                    6)
                        show_help
                        ;;
                    0)
                        echo "${GREEN}再见!${RESET}"
                        exit 0
                        ;;
                    *)
                        echo "${RED}无效选项，请重试${RESET}"
                        ;;
                esac
                
                echo ""
                read -p "按Enter键继续..."
                clear
                show_banner
            done
            ;;
        *)
            echo "${RED}未知选项: ${1}${RESET}"
            echo "${YELLOW}运行 --help 查看帮助${RESET}"
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"