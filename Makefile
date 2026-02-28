# HIC 根目录 Makefile
# 整合构建系统、ISO镜像创建和调试功能

.PHONY: all clean bootloader kernel install help debug test build-qt build-gtk build-tui build-cli
.PHONY: iso iso-clean iso-list iso-test
.PHONY: gdb gdb-script qemu qemu-clean update-disk image
.PHONY: install-disk

# 项目配置
PROJECT = HIC
VERSION = 0.1.0
ROOT_DIR = $(shell pwd)
BUILD_DIR = $(ROOT_DIR)/build
BOOTLOADER_DIR = $(ROOT_DIR)/src/bootloader
OUTPUT_DIR = $(ROOT_DIR)/output
ISO_DIR = $(ROOT_DIR)/iso_output
ISO_FILE = $(OUTPUT_DIR)/$(PROJECT)-installer.iso

# QEMU配置
QEMU_MEM = 2G
QEMU_OVMF_CODE = /usr/share/OVMF/x64/OVMF_CODE.4m.fd
QEMU_OVMF_VARS = /usr/share/edk2-ovmf/x64/OVMF_VARS.4m.fd

# 主要构建目标

# 默认目标 - 启动增强的构建系统
all: build

# 快速构建向导（推荐新手）
start:
	@echo "启动快速构建向导"
	@python3 scripts/quick_start.py

# 构建引导程序
bootloader: image
	@echo "构建引导程序"
	cd $(BOOTLOADER_DIR) && $(MAKE) clean
	cd $(BOOTLOADER_DIR) && $(MAKE) all
	@echo "引导程序构建完成"

# 构建内核
kernel:
	@echo "构建内核"
	@cd $(BUILD_DIR) && $(MAKE) clean
	@cd $(BUILD_DIR) && $(MAKE) all
	@echo "内核构建完成"

# 构建特权服务
privileged-services:
	@echo "构建特权层服务"
	@cd $(ROOT_DIR)/src/Privileged-1 && $(MAKE) clean
	@cd $(ROOT_DIR)/src/Privileged-1 && $(MAKE) all
	@cd $(ROOT_DIR)/src/Privileged-1 && $(MAKE) install
	@echo "特权服务构建完成"

# 构建服务模块
privileged-modules:
	@echo "构建服务模块"
	@$(MAKE) -C src/Privileged-1 modules

# 安装特权模块到构建目录
privileged-install:
	@echo "安装特权模块"
	@mkdir -p $(BUILD_DIR)/modules
	@cp -r $(BUILD_DIR)/modules/*.hicmod $(BUILD_DIR)/modules/ 2>/dev/null || true
	@ls -lh $(BUILD_DIR)/modules/

# 生成HIC镜像（HIK格式）
image: kernel privileged-services
	@echo "生成HIC镜像（HIK格式）"
	@objcopy -O binary $(BUILD_DIR)/bin/hic-kernel.elf $(BUILD_DIR)/bin/hic-kernel.bin
	@python3 scripts/create_hik_image.py $(BUILD_DIR)/bin/hic-kernel.elf $(BUILD_DIR)/bin/hic-kernel.hic
	@echo "HIC镜像生成完成: $(BUILD_DIR)/bin/hic-kernel.hic"

# 创建磁盘镜像（IMG）- 自动嵌入所有模块
img: bootloader kernel privileged-services privileged-modules
	@echo "创建磁盘镜像（IMG）并嵌入模块"
	@mkdir -p $(OUTPUT_DIR)
	@python3 scripts/create_efi_disk_no_root.py \
		--bootloader $(BOOTLOADER_DIR)/bin/bootx64.efi \
		--kernel $(BUILD_DIR)/bin/hic-kernel.elf \
		--output $(OUTPUT_DIR)/hic-uefi-disk.img
	@echo "嵌入所有模块到磁盘镜像..."
	@echo "创建modules目录..."
	@mdir -i $(OUTPUT_DIR)/hic-uefi-disk.img ::modules 2>/dev/null || mmd -i $(OUTPUT_DIR)/hic-uefi-disk.img ::modules
	@for mod in $(BUILD_DIR)/privileged-1/modules/*.hicmod; do \
		if [ -f "$$mod" ]; then \
			echo "  嵌入: $$(basename $$mod)"; \
			mcopy -i $(OUTPUT_DIR)/hic-uefi-disk.img "$$mod" ::modules/; \
		fi; \
	done
	@echo "已嵌入的模块:"
	@mdir -i $(OUTPUT_DIR)/hic-uefi-disk.img ::modules
	@echo "磁盘镜像创建完成: $(OUTPUT_DIR)/hic-uefi-disk.img"

# 更新磁盘镜像
update-disk: img
	@echo "更新磁盘镜像"
	@sudo umount /tmp/hic_mnt 2>/dev/null || true
	@sudo losetup -f output/hic-uefi-disk.img /dev/loop0 2>/dev/null || true
	@sudo partprobe /dev/loop0 2>/dev/null || true
	@sudo mount /dev/loop0p1 /tmp/hic_mnt 2>/dev/null || true
	@sudo mkdir -p /tmp/hic_mnt/EFI/BOOT
	@sudo cp $(BOOTLOADER_DIR)/bin/bootx64.efi /tmp/hic_mnt/EFI/BOOT/
	@sudo cp $(BUILD_DIR)/bin/hic-kernel.hic /tmp/hic_mnt/hic-kernel.bin
	@sudo umount /tmp/hic_mnt 2>/dev/null || true
	@sudo losetup -d /dev/loop0 2>/dev/null || true
	@echo "磁盘镜像更新完成"

# 清理
clean: clean-debug clean-iso
	@echo "清理构建文件"
	@cd $(BOOTLOADER_DIR) && $(MAKE) clean || true
	@cd $(BUILD_DIR) && $(MAKE) clean || true
	@rm -rf $(OUTPUT_DIR)/*
	@echo "清理完成"

# 安装（将输出文件复制到output目录）
install:
	@echo "安装构建产物"
	@mkdir -p $(OUTPUT_DIR)
	@if [ -f $(BOOTLOADER_DIR)/bin/bootx64.efi ]; then \
		cp $(BOOTLOADER_DIR)/bin/bootx64.efi $(OUTPUT_DIR)/; \
		echo "已复制 bootx64.efi"; \
	fi
	@if [ -f $(BOOTLOADER_DIR)/bin/bios.bin ]; then \
		cp $(BOOTLOADER_DIR)/bin/bios.bin $(OUTPUT_DIR)/; \
		echo "已复制 bios.bin"; \
	fi
	@if [ -f $(BUILD_DIR)/bin/hic-kernel.bin ]; then \
		cp $(BUILD_DIR)/bin/hic-kernel.bin $(OUTPUT_DIR)/; \
		echo "已复制 hic-kernel.bin"; \
	fi
	@if [ -f $(BUILD_DIR)/bin/hic-kernel.elf ]; then \
		cp $(BUILD_DIR)/bin/hic-kernel.elf $(OUTPUT_DIR)/; \
		echo "已复制 hic-kernel.elf"; \
	fi
	@echo "安装完成"

# 安装到磁盘（需要sudo权限）
install-disk: $(ISO_FILE)
	@echo " 安装到磁盘 "
	@echo "警告: 此操作需要root权限"
	@echo "请手动使用以下命令："

	@echo "替换 /dev/sdX 为您的目标设备"

# 构建系统界面

# 运行构建系统（自动选择界面）
build:
	@echo " 启动HIC构建系统 "
	@bash scripts/build_launcher.sh

# 指定界面类型运行构建系统
build-qt:
	@echo " 运行构建系统 Qt GUI "
	@python3 scripts/build_system.py --interface qt

build-gtk:
	@echo " 运行构建系统 GTK GUI "
	@python3 scripts/build_system.py --interface gtk

build-tui:
	@echo " 运行构建系统 (TUI) "
	@python3 scripts/build_system.py --interface tui

build-cli:
	@echo " 运行构建系统 (CLI) "
	@python3 scripts/build_system.py --interface cli

build-web:
	@echo " 运行构建系统 (Web GUI) "
	@scripts/venv/bin/python scripts/gui_unified.py --backend web

# 使用预设配置构建
build-balanced:
	@python3 scripts/build_system.py --preset balanced

build-release:
	@python3 scripts/build_system.py --preset release

build-debug:
	@python3 scripts/build_system.py --preset debug

build-minimal:
	@python3 scripts/build_system.py --preset minimal

build-performance:
	@python3 scripts/build_system.py --preset performance

# ISO镜像创建

# 创建ISO镜像
iso: $(ISO_FILE)

$(ISO_FILE): $(ISO_DIR)
	@echo " 创建ISO镜像 "
	xorriso -as mkisofs \
		-r -J -joliet-long \
		-V "$(PROJECT)_Installer_v$(VERSION)" \
		-e EFI/BOOT/BOOTX64.EFI -no-emul-boot \
		-o $(ISO_FILE) \
		$(ISO_DIR)
	@echo "ISO镜像创建完成: $(ISO_FILE)"

# 准备ISO目录结构
$(ISO_DIR):
	@echo " 准备ISO目录 "
	@mkdir -p $(ISO_DIR)/EFI/BOOT
	@mkdir -p $(ISO_DIR)/kernel
	@if [ -f $(BOOTLOADER_DIR)/bin/bootx64.efi ]; then \
		cp $(BOOTLOADER_DIR)/bin/bootx64.efi $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI; \
		echo "已复制 bootx64.efi"; \
	fi
	@if [ -f $(BUILD_DIR)/bin/hic-kernel.hic ]; then \
		cp $(BUILD_DIR)/bin/hic-kernel.hic $(ISO_DIR)/kernel/; \
		echo "已复制 hic-kernel.hic"; \
	elif [ -f $(BUILD_DIR)/bin/hic-kernel.bin ]; then \
		cp $(BUILD_DIR)/bin/hic-kernel.bin $(ISO_DIR)/kernel/hic-kernel.hic; \
		echo "已复制 hic-kernel.bin (重命名为hic-kernel.hic)"; \
	fi
	@cp $(BOOTLOADER_DIR)/platform.yaml $(ISO_DIR)/
	@echo " HIC ISO安装包 " > $(ISO_DIR)/README.txt
	@echo "1. 将此ISO刻录到USB或CD" >> $(ISO_DIR)/README.txt
	@echo "2. 从USB/CD启动(UEFI模式)" >> $(ISO_DIR)/README.txt
	@echo "3. 系统将自动安装HIC内核" >> $(ISO_DIR)/README.txt
	@echo "" >> $(ISO_DIR)/README.txt
	@echo " 版本信息 " >> $(ISO_DIR)/README.txt
	@git log -1 --oneline >> $(ISO_DIR)/README.txt 2>/dev/null || echo "Version: $(VERSION)" >> $(ISO_DIR)/README.txt
	@echo "构建时间: $$(date)" >> $(ISO_DIR)/README.txt

# 清理ISO文件
clean-iso:
	@echo " 清理ISO文件 "
	@rm -rf $(ISO_DIR) $(ISO_FILE)
	@echo "ISO清理完成"

# 查看ISO内容
iso-list: $(ISO_FILE)
	@echo " ISO内容 "
	@xorriso -indev $(ISO_FILE) -ls /

# 测试ISO
iso-test: $(ISO_FILE)
	@echo " 测试ISO镜像 "
	@timeout 30 qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(QEMU_OVMF_CODE) \
		-drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
		-drive format=raw,file=$(ISO_FILE),media=cdrom \
		-m $(QEMU_MEM) -nographic \
		2>&1 | tail -100

# 调试相关

# 创建GDB脚本
gdb-script:
	@echo " 创建GDB调试脚本 "
	@echo " HIC内核GDB调试脚本 " > /tmp/hic_debug.gdb
	@echo "target remote :1234" >> /tmp/hic_debug.gdb
	@echo "break kernel_start" >> /tmp/hic_debug.gdb
	@echo "break scheduler_init" >> /tmp/hic_debug.gdb
	@echo "continue" >> /tmp/hic_debug.gdb
	@echo "info threads" >> /tmp/hic_debug.gdb
	@echo "info registers" >> /tmp/hic_debug.gdb
	@echo "bt" >> /tmp/hic_debug.gdb
	@echo "quit" >> /tmp/hic_debug.gdb
	@echo "GDB脚本创建完成"

# 快速运行 QEMU
run:
	@pkill qemu-system-x86_64 2>/dev/null || true
	@echo " 启动 QEMU 测试内核 (GUI模式 + 串口输出) "
	@echo "提示: 串口输出将显示在终端中，GUI 窗口显示图形界面"
	@qemu-system-x86_64 \
		-drive if=virtio,file=output/hic-uefi-disk.img,format=raw,cache=none \
		-m $(QEMU_MEM) \
		-drive if=pflash,readonly=on,file=$(QEMU_OVMF_CODE) \
		-display gtk \
		-serial file:/tmp/qemu-serial.log \
		-monitor none &

# 快速运行 QEMU (串口模式)
run-serial:
	@pkill qemu-system-x86_64 2>/dev/null || true
	@echo " 启动 QEMU 测试内核 (串口模式) "
	@timeout 15 qemu-system-x86_64 \
		-nographic \
		-serial mon:stdio \
		-drive if=virtio,file=output/hic-uefi-disk.img,format=raw \
		-m $(QEMU_MEM) \
		-drive if=pflash,readonly=on,file=$(QEMU_OVMF_CODE) \
		2>&1 | tail -100 || true

# 启动QEMU（后台）
qemu: update-disk
	@echo " 启动QEMU（GDB模式） "
	@cp $(QEMU_OVMF_VARS) /tmp/OVMF_VARS.4m.fd 2>/dev/null || \
		touch /tmp/OVMF_VARS.4m.fd
	@qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(QEMU_OVMF_CODE) \
		-drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
		-drive format=raw,file=output/hic-uefi-disk.img \
		-m $(QEMU_MEM) \
		-nographic \
		-s -S \
		> /tmp/qemu_output.log 2>&1 &
	@echo $$ > /tmp/qemu.pid
	@echo "QEMU PID: $$(cat /tmp/qemu.pid)"
	@sleep 2

# 启动GDB调试
gdb: gdb-script
	@echo " 启动GDB调试 "
	@echo ""
	@gdb -batch -x /tmp/hic_debug.gdb $(BUILD_DIR)/bin/hic-kernel.elf
	@echo ""

# 完整调试流程
debug: qemu gdb
	@echo " 清理QEMU进程 "
	@if [ -f /tmp/qemu.pid ]; then \
		kill $$(cat /tmp/qemu.pid) 2>/dev/null || true; \
		rm /tmp/qemu.pid; \
	fi
	@echo ""
	@echo ""
	@cat /tmp/qemu_output.log | tail -50
	@echo ""

# 清理调试文件
clean-debug:
	@echo " 清理调试文件 "
	@sudo umount /tmp/hic_mnt 2>/dev/null || true
	@sudo losetup -d /dev/loop0 2>/dev/null || true
	@rm -f /tmp/qemu.pid /tmp/qemu_output.log /tmp/hic_debug.gdb /tmp/OVMF_VARS.4m.fd
	@echo "调试文件清理完成"

# 快速测试（不带GDB）
test: update-disk
	@echo " 快速测试启动 "
	@cp $(QEMU_OVMF_VARS) /tmp/OVMF_VARS.4m.fd 2>/dev/null || \
		touch /tmp/OVMF_VARS.4m.fd
	@timeout 20 qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=$(QEMU_OVMF_CODE) \
		-drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
		-drive format=raw,file=output/hic-uefi-disk.img \
		-m $(QEMU_MEM) \
		-nographic \
		2>&1 | tail -100

# 依赖管理

# 自动安装所有构建依赖
deps:
	@echo " 自动安装构建依赖 "
	@python3 scripts/dependency_manager.py

# 检查依赖状态
deps-check:
	@echo " 检查依赖状态 "
	@python3 scripts/dependency_manager.py --check

# 安装指定依赖
deps-install:
	@echo " 安装指定依赖 "
	@if [ -z "$(TOOLS)" ]; then \
		echo "用法: make deps-install TOOLS=gcc,make,python3"; \
		exit 1; \
	fi
	@python3 scripts/dependency_manager.py --install $(subst $(comma), ,$(TOOLS))

# 强制重新安装依赖
deps-force:
	@echo " 强制重新安装依赖 "
	@python3 scripts/dependency_manager.py --force

# Arch Linux 专用依赖安装（向后兼容）
deps-arch:
	@echo " Arch Linux 依赖安装 "
	@sudo pacman -S --needed base-devel git mingw-w64-gcc gnu-efi ncurses gtk3

# 帮助信息

help:
	@echo "$(PROJECT) 构建系统 v$(VERSION)"
	@echo ""

	@echo "  make start             - 启动快速构建向导（推荐新手）"
	@echo "  make build             - 自动选择界面（推荐）"
	@echo "  make deps              - 自动安装所有构建依赖（跨平台）"
	@echo ""

	@echo "  make all                - 构建所有组件"
	@echo "  make bootloader         - 仅构建引导程序"
	@echo "  make kernel             - 仅构建内核"
	@echo "  make image              - 生成HIC镜像"
	@echo "  make img                - 创建磁盘镜像（IMG）"
	@echo "  make update-disk        - 更新磁盘镜像"
	@echo "  make clean              - 清理所有构建文件"
	@echo "  make install            - 安装构建产物到output目录"
	@echo ""

	@echo "  make build              - 自动选择界面（推荐）"
	@echo "  make build-qt           - 强制使用Qt GUI界面"
	@echo "  make build-gtk          - 强制使用GTK GUI界面"
	@echo "  make build-tui          - 强制使用TUI界面"
	@echo "  make build-cli          - 强制使用CLI交互式Shell"
	@echo "  make build-web          - 强制使用Web GUI界面（浏览器）"
	@echo ""

	@echo "  make build-balanced     - 使用平衡配置"
	@echo "  make build-release      - 使用发布配置"
	@echo "  make build-debug        - 使用调试配置"
	@echo "  make build-minimal      - 使用最小配置"
	@echo "  make build-performance  - 使用性能配置"
	@echo ""

	@echo "  make iso                - 创建ISO安装镜像"
	@echo "  make clean-iso          - 清理ISO文件"
	@echo "  make iso-list           - 查看ISO内容"
	@echo "  make iso-test           - 测试ISO镜像"
	@echo ""

	@echo "  make run                - 快速运行 QEMU 测试内核 (GUI模式)"
	@echo "  make run-serial         - 快速运行 QEMU 测试内核 (串口模式)"
	@echo "  make debug              - 完整GDB调试流程"
	@echo "  make gdb                - 仅启动GDB调试"
	@echo "  make gdb-script         - 创建GDB脚本"
	@echo "  make qemu               - 启动QEMU（后台）"
	@echo "  make test               - 快速测试启动（无GDB）"
	@echo "  make clean-debug        - 清理调试文件"
	@echo ""

	@echo "  make start                                  # 快速开始（推荐）"
	@echo "  make deps && make build-balanced            # 安装依赖并构建（跨平台）"
	@echo "  make build-balanced && make install         # 平衡配置构建并安装"
	@echo "  make kernel && make image && make iso       # 构建完整ISO"
	@echo "  make build-debug && make debug              # 调试内核启动"
	@echo "  make iso && make install-disk               # 创建并安装ISO"
	@echo ""

	@echo "  make deps              - 自动安装所有构建依赖"
	@echo "  make deps-check        - 检查依赖状态"

	@echo "  make deps-force        - 强制重新安装依赖"
	@echo ""

	@echo "  docs/Wiki/03-QuickStart.md                   - 快速开始指南"
	@echo "  docs/Wiki/04-BuildSystem.md                  - 构建系统详细说明"
	@echo "  docs/Wiki/08-Core0.md                        - Core-0层详解"
	@echo "  docs/BUILD_SYSTEM_GUIDE.md                   - 构建系统完整指南"