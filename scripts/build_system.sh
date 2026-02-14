#!/bin/bash
# HIK系统构建系统 (Shell版本)
# 功能：
# 1. 自动获取签名
# 2. 构建引导程序和内核
# 3. 签名验证
# 4. 输出最终镜像

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
RESET='\033[0m'

# 配置
PROJECT="HIK System"
VERSION="0.1.0"
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
OUTPUT_DIR="${ROOT_DIR}/output"
SIGN_KEY_FILE="${BUILD_DIR}/signing_key.pem"
SIGN_CERT_FILE="${BUILD_DIR}/signing_cert.pem"

# 日志函数
log() {
    local level="$1"
    shift
    local message="$@"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo -e "[${timestamp}] [${level}] ${message}"
}

log_info() { log "INFO" "$@"; }
log_warn() { log "WARN" "$@"; }
log_error() { log "ERROR" "$@"; }
log_success() { log -e "${GREEN}SUCCESS${RESET}" "$@"; }

# 检查命令是否存在
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."

    local missing_tools=()
    local required_tools=("gcc" "make" "objcopy" "objdump")

    for tool in "${required_tools[@]}"; do
        if ! command_exists "$tool"; then
            missing_tools+=("$tool")
        fi
    done

    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_error "缺少依赖: ${missing_tools[*]}"
        return 1
    fi

    # 检查UEFI工具
    if ! command_exists "x86_64-w64-mingw32-gcc"; then
        log_warn "未找到 x86_64-w64-mingw32-gcc，无法构建UEFI引导程序"
    fi

    # 检查BIOS工具
    if ! command_exists "x86_64-elf-gcc"; then
        log_warn "未找到 x86_64-elf-gcc，无法构建内核"
    fi

    # 检查签名工具
    if ! command_exists "openssl"; then
        log_warn "未找到 openssl，无法进行签名操作"
    fi

    log_info "依赖检查完成"
    return 0
}

# 生成签名密钥对
generate_signing_keys() {
    log_info "生成签名密钥对..."

    mkdir -p "${BUILD_DIR}"

    if [ -f "${SIGN_KEY_FILE}" ] && [ -f "${SIGN_CERT_FILE}" ]; then
        log_info "密钥文件已存在，跳过生成"
        return 0
    fi

    # 生成私钥
    openssl genrsa -out "${SIGN_KEY_FILE}" 4096 2>/dev/null
    if [ $? -ne 0 ]; then
        log_error "私钥生成失败"
        return 1
    fi

    log_info "私钥生成成功: ${SIGN_KEY_FILE}"

    # 生成自签名证书
    openssl req -new -x509 \
        -key "${SIGN_KEY_FILE}" \
        -out "${SIGN_CERT_FILE}" \
        -days 3650 \
        -subj "/C=CN/ST=Beijing/O=HIK/CN=HIK-${VERSION}" \
        2>/dev/null

    if [ $? -ne 0 ]; then
        log_error "证书生成失败"
        return 1
    fi

    log_info "证书生成成功: ${SIGN_CERT_FILE}"
    return 0
}

# 计算文件哈希
calculate_hash() {
    local file="$1"
    sha384sum "$file" | awk '{print $1}'
}

# 签名文件
sign_file() {
    local input_file="$1"
    local output_file="$2"

    log_info "签名文件: ${input_file}"

    if [ ! -f "${SIGN_KEY_FILE}" ]; then
        log_warn "签名密钥不存在，跳过签名"
        return 1
    fi

    # 计算文件哈希
    local file_hash=$(calculate_hash "${input_file}")

    # 使用OpenSSL签名
    local sig_file="${input_file}.sig"
    openssl dgst -sha384 -sign "${SIGN_KEY_FILE}" -out "${sig_file}" "${input_file}" 2>/dev/null

    if [ $? -ne 0 ]; then
        log_error "签名失败"
        return 1
    fi

    # 读取签名
    local signature=$(base64 -w 0 "${sig_file}")
    local signature_size=$(stat -f%z "${sig_file}" 2>/dev/null || stat -c%s "${sig_file}")

    # 创建签名信息
    cat > "${output_file}" <<EOF
{
  "version": "${VERSION}",
  "timestamp": "$(date -Iseconds)",
  "algorithm": "RSA-4096",
  "hash": "SHA-384",
  "file_hash": "${file_hash}",
  "signature_size": ${signature_size},
  "signature": "${signature}"
}
EOF

    log_info "签名成功: ${output_file}"
    log_info "签名大小: ${signature_size} 字节"

    # 清理临时签名文件
    rm -f "${sig_file}"

    return 0
}

# 构建引导程序
build_bootloader() {
    local target="$1"
    log_info "构建引导程序 (目标: ${target})..."

    local bootloader_dir="${ROOT_DIR}/../src/bootloader"

    # 清理旧的构建
    make -C "${bootloader_dir}" clean >/dev/null 2>&1

    # 构建目标
    make -C "${bootloader_dir}" "${target}" >/dev/null 2>&1

    if [ $? -ne 0 ]; then
        log_error "引导程序构建失败"
        return 1
    fi

    local output
    if [ "${target}" = "uefi" ]; then
        output="${bootloader_dir}/bin/bootx64.efi"
    else
        output="${bootloader_dir}/bin/bios.bin"
    fi

    if [ ! -f "${output}" ]; then
        log_error "引导程序构建失败: ${output} 不存在"
        return 1
    fi

    log_info "引导程序构建成功: ${output}"
    return 0
}

# 构建内核
build_kernel() {
    log_info "构建内核..."

    local kernel_dir="${ROOT_DIR}"

    # 清理旧的构建
    make -C "${kernel_dir}" clean >/dev/null 2>&1

    # 构建内核
    make -C "${kernel_dir}" all >/dev/null 2>&1

    if [ $? -ne 0 ]; then
        log_error "内核构建失败"
        return 1
    fi

    local output="${kernel_dir}/bin/hik-kernel.elf"

    if [ ! -f "${output}" ]; then
        log_error "内核构建失败: ${output} 不存在"
        return 1
    fi

    log_info "内核构建成功: ${output}"
    return 0
}

# 创建启动镜像
create_output_structure() {
    log_info "创建输出目录结构..."
    
    mkdir -p "${OUTPUT_DIR}/EFI/BOOT"
    
    # 复制UEFI引导程序
    local bootloader_uefi="${ROOT_DIR}/../src/bootloader/bin/bootx64.efi"
    local bootloader_bios="${ROOT_DIR}/../src/bootloader/bin/bios.bin"
    local kernel="${ROOT_DIR}/bin/hik-kernel.elf"

    # UEFI镜像
    if [ -f "${bootloader_uefi}" ]; then
        mkdir -p "${OUTPUT_DIR}/EFI/BOOT"
        cp "${bootloader_uefi}" "${OUTPUT_DIR}/EFI/BOOT/bootx64.efi"
        log_info "UEFI镜像创建完成: ${OUTPUT_DIR}/EFI/BOOT/bootx64.efi"
    fi

    # BIOS镜像
    if [ -f "${bootloader_bios}" ]; then
        cp "${bootloader_bios}" "${OUTPUT_DIR}/bios.bin"
        log_info "BIOS镜像创建完成: ${OUTPUT_DIR}/bios.bin"
    fi

    # 内核
    if [ -f "${kernel}" ]; then
        cp "${kernel}" "${OUTPUT_DIR}/kernel.elf"
        log_info "内核复制完成: ${OUTPUT_DIR}/kernel.elf"

        # 签名内核
        sign_file "${kernel}" "${OUTPUT_DIR}/kernel.sig.json"
    fi

    # 生成构建报告
    generate_build_report

    return 0
}

# 生成构建报告
generate_build_report() {
    local report_file="${OUTPUT_DIR}/build_report.json"
    local report="{"
    report+='"project": "'"${PROJECT}"'",'
    report+='"version": "'"${VERSION}"'",'
    report+='"build_time": "'"$(date -Iseconds)"'",'
    report+='"build_type": "full",'
    report+='"components": {'

    local first=true

    # 检查UEFI引导程序
    local bootloader_uefi="${ROOT_DIR}/../src/bootloader/bin/bootx64.efi"
    if [ -f "${bootloader_uefi}" ]; then
        if [ "$first" = true ]; then
            first=false
        else
            report+=","
        fi
        local size=$(stat -f%z "${bootloader_uefi}" 2>/dev/null || stat -c%s "${bootloader_uefi}")
        local hash=$(calculate_hash "${bootloader_uefi}")
        report+='"uefi_bootloader": {'
        report+='"path": "EFI/BOOT/bootx64.efi",'
        report+='"size": '${size}','
        report+='"hash": "'"${hash}"'"'
        report+="}"
    fi

    # 检查BIOS引导程序
    local bootloader_bios="${ROOT_DIR}/../src/bootloader/bin/bios.bin"
    if [ -f "${bootloader_bios}" ]; then
        if [ "$first" = true ]; then
            first=false
        else
            report+=","
        fi
        local size=$(stat -f%z "${bootloader_bios}" 2>/dev/null || stat -c%s "${bootloader_bios}")
        local hash=$(calculate_hash "${bootloader_bios}")
        report+='"bios_bootloader": {'
        report+='"path": "bios.bin",'
        report+='"size": '${size}','
        report+='"hash": "'"${hash}"'"'
        report+="}"
    fi

    # 检查内核
    local kernel="${ROOT_DIR}/bin/hik-kernel.elf"
    if [ -f "${kernel}" ]; then
        if [ "$first" = true ]; then
            first=false
        else
            report+=","
        fi
        local size=$(stat -f%z "${kernel}" 2>/dev/null || stat -c%s "${kernel}")
        local hash=$(calculate_hash "${kernel}")
        report+='"kernel": {'
        report+='"path": "kernel.elf",'
        report+='"size": '${size}','
        report+='"hash": "'"${hash}"'"'
        report+="}"
    fi

    report+="}"
    report+="}"

    echo "${report}" | python3 -m json.tool 2>/dev/null || echo "${report}" > "${report_file}"

    log_info "构建报告生成完成: ${report_file}"
}

# 清理构建文件
clean_build() {
    log_info "清理构建文件..."

    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"

    # 清理子目录
    make -C "${ROOT_DIR}/../src/bootloader" clean >/dev/null 2>&1 || true
    make -C "${ROOT_DIR}" clean >/dev/null 2>&1 || true

    log_info "清理完成"
}

# 显示帮助
show_help() {
    cat <<EOF
${MAGENTA}========================================${RESET}
${MAGENTA}HIK系统构建系统 v${VERSION}${RESET}
${MAGENTA}========================================${RESET}

${GREEN}用法:${RESET}
  $0 [选项]

${GREEN}选项:${RESET}
  --target uefi      仅构建UEFI引导程序
  --target bios      仅构建BIOS引导程序
  --target kernel    仅构建内核
  --target all       构建所有组件 (默认)
  --clean            清理构建文件
  --help             显示此帮助

${GREEN}示例:${RESET}
  $0                  # 构建所有组件
  $0 --target uefi    # 仅构建UEFI引导程序
  $0 --target kernel  # 仅构建内核
  $0 --clean          # 清理构建文件

${GREEN}输出目录:${RESET}
  ${OUTPUT_DIR}

EOF
}

# 主函数
main() {
    local targets=()
    local clean_mode=false

    # 解析参数
    while [ $# -gt 0 ]; do
        case "$1" in
            --target)
                shift
                case "$1" in
                    uefi|bios|kernel|all)
                        if [ "$1" = "all" ]; then
                            targets=("uefi" "bios" "kernel")
                        else
                            targets+=("$1")
                        fi
                        ;;
                    *)
                        log_error "无效的目标: $1"
                        show_help
                        exit 1
                        ;;
                esac
                ;;
            --clean)
                clean_mode=true
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
        shift
    done

    # 默认构建所有组件
    if [ ${#targets[@]} -eq 0 ]; then
        targets=("uefi" "bios" "kernel")
    fi

    # 清理模式
    if [ "$clean_mode" = true ]; then
        clean_build
        exit 0
    fi

    # 开始构建
    echo -e "${MAGENTA}========================================${RESET}"
    echo -e "${MAGENTA}${PROJECT} 构建系统 v${VERSION}${RESET}"
    echo -e "${MAGENTA}========================================${RESET}"
    echo ""

    # 检查依赖
    if ! check_dependencies; then
        log_error "依赖检查失败，构建终止"
        exit 1
    fi

    # 创建构建目录
    mkdir -p "${BUILD_DIR}"

    # 生成签名密钥
    generate_signing_keys

    # 构建目标
    local success=true
    for target in "${targets[@]}"; do
        case "$target" in
            uefi|bios)
                if ! build_bootloader "$target"; then
                    success=false
                fi
                ;;
            kernel)
                if ! build_kernel; then
                    success=false
                fi
                ;;
        esac
    done

    if [ "$success" = false ]; then
        log_warn "部分构建失败"
    fi

    # 创建启动镜像
    if ! create_boot_image; then
        log_error "启动镜像创建失败"
        exit 1
    fi

    echo ""
    echo -e "${MAGENTA}========================================${RESET}"
    echo -e "${GREEN}构建完成!${RESET}"
    echo -e "${CYAN}输出目录: ${OUTPUT_DIR}${RESET}"
    echo -e "${MAGENTA}========================================${RESET}"

    exit 0
}

# 运行主函数
main "$@"
