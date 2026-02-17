#!/bin/bash
# HIC Bootloader测试脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BOOTLOADER_DIR="$PROJECT_ROOT/bootloader"
BUILD_DIR="$BOOTLOADER_DIR/bin"
TEST_DIR="$BOOTLOADER_DIR/test_disk"

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖..."
    
    local missing_deps=()
    
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        missing_deps+=("gcc-mingw-w64-x86-64")
    fi
    
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        missing_deps+=("qemu-system-x86")
    fi
    
    if [ ! -f /usr/share/ovmf/OVMF_CODE.fd ]; then
        missing_deps+=("ovmf")
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        log_error "缺少以下依赖:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "安装命令:"
        echo "  sudo apt install ${missing_deps[*]}"
        exit 1
    fi
    
    log_success "所有依赖已满足"
}

# 编译引导加载程序
build_bootloader() {
    log_info "编译引导加载程序..."
    
    cd "$BOOTLOADER_DIR"
    make clean
    make all
    
    if [ ! -f "$BUILD_DIR/bootx64.efi" ]; then
        log_error "编译失败: bootx64.efi 不存在"
        exit 1
    fi
    
    log_success "编译成功"
}

# 创建测试环境
setup_test_environment() {
    log_info "创建测试环境..."
    
    # 创建测试目录
    mkdir -p "$TEST_DIR/EFI/HIC"
    
    # 复制引导加载程序
    cp "$BUILD_DIR/bootx64.efi" "$TEST_DIR/EFI/HIC/"
    
    # 创建配置文件
    cat > "$TEST_DIR/EFI/HIC/boot.conf" << 'EOF'
# HIC Bootloader Configuration

# 内核路径
kernel=\EFI\HIC\kernel.hic

# 启动参数
cmdline=quiet debug=1

# 启动超时（秒）
timeout=5

# 调试输出
debug=1
EOF
    
    # 创建虚拟内核文件（用于测试）
    cat > "$TEST_DIR/EFI/HIC/kernel.hic" << 'EOF'
HIC_IMG\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00
EOF
    
    log_success "测试环境创建完成"
}

# 运行QEMU测试
run_qemu_test() {
    local mode="${1:-text}"
    
    log_info "运行QEMU测试 ($mode 模式)..."
    
    cd "$BOOTLOADER_DIR"
    
    if [ "$mode" = "gui" ]; then
        qemu-system-x86_64 \
            -bios /usr/share/ovmf/OVMF_CODE.fd \
            -drive if=virtio,file=fat:rw:$TEST_DIR:format=raw \
            -net none \
            -serial stdio \
            -m 512M \
            -smp 2
    else
        qemu-system-x86_64 \
            -bios /usr/share/ovmf/OVMF_CODE.fd \
            -drive if=virtio,file=fat:rw:$TEST_DIR:format=raw \
            -net none \
            -nographic \
            -monitor none \
            -m 512M \
            -smp 2
    fi
}

# 创建USB镜像
create_usb_image() {
    local image_name="${1:-hic-boot.img}"
    
    log_info "创建USB镜像: $image_name"
    
    cd "$BOOTLOADER_DIR"
    
    # 创建100MB镜像
    dd if=/dev/zero of="$image_name" bs=1M count=100
    
    # 格式化为FAT32
    mkfs.vfat -F 32 "$image_name"
    
    # 创建EFI目录
    mmd -i "$image_name" ::/EFI
    mmd -i "$image_name" ::/EFI/HIC
    
    # 复制文件
    mcopy -i "$image_name" "$BUILD_DIR/bootx64.efi" ::/EFI/HIC/
    mcopy -i "$image_name" "$TEST_DIR/EFI/HIC/boot.conf" ::/EFI/HIC/
    
    log_success "USB镜像创建完成: $image_name"
}

# 清理测试环境
cleanup() {
    log_info "清理测试环境..."
    
    cd "$BOOTLOADER_DIR"
    
    # 清理测试目录
    if [ -d "$TEST_DIR" ]; then
        rm -rf "$TEST_DIR"
    fi
    
    # 清理临时文件
    rm -f *.img
    
    log_success "清理完成"
}

# 显示帮助
show_help() {
    cat << EOF
HIC Bootloader 测试脚本

用法: $0 [选项] [命令]

选项:
  -h, --help     显示此帮助信息

命令:
  check          检查依赖
  build          编译引导加载程序
  setup          创建测试环境
  test           运行QEMU测试（文本模式）
  test-gui       运行QEMU测试（图形模式）
  usb [名称]     创建USB镜像
  clean          清理测试环境
  all            执行完整测试流程（检查、编译、设置、测试）

示例:
  $0 check              # 检查依赖
  $0 build              # 编译
  $0 test               # 运行测试
  $0 all                # 完整测试
  $0 usb hic-boot.img   # 创建USB镜像

EOF
}

# 主函数
main() {
    local command="${1:-help}"
    
    case "$command" in
        check)
            check_dependencies
            ;;
        build)
            build_bootloader
            ;;
        setup)
            build_bootloader
            setup_test_environment
            ;;
        test)
            build_bootloader
            setup_test_environment
            run_qemu_test text
            ;;
        test-gui)
            build_bootloader
            setup_test_environment
            run_qemu_test gui
            ;;
        usb)
            build_bootloader
            setup_test_environment
            create_usb_image "$2"
            ;;
        clean)
            cleanup
            ;;
        all)
            check_dependencies
            build_bootloader
            setup_test_environment
            log_info "准备运行QEMU测试..."
            log_info "按 Ctrl+C 退出"
            sleep 2
            run_qemu_test text
            ;;
        -h|--help|help)
            show_help
            ;;
        *)
            log_error "未知命令: $command"
            show_help
            exit 1
            ;;
    esac
}

# 运行主函数
main "$@"