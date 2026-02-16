# HIK 根目录 Makefile
# 支持在根目录直接运行 make

.PHONY: all clean bootloader kernel install help debug test

# 默认目标
all: bootloader kernel

# 构建引导程序
bootloader:
	@echo "=== 构建引导程序 ==="
	@cd src/bootloader && $(MAKE) clean
	@cd src/bootloader && $(MAKE) all
	@echo "引导程序构建完成"

# 构建内核
kernel:
	@echo "=== 构建内核 ==="
	@cd build && $(MAKE) clean
	@cd build && $(MAKE) all
	@echo "内核构建完成"

# 清理
clean:
	@echo "=== 清理构建文件 ==="
	@cd src/bootloader && $(MAKE) clean || true
	@cd build && $(MAKE) clean || true
	@rm -rf output/*
	@echo "清理完成"

# 安装（将输出文件复制到output目录）
install:
	@echo "=== 安装构建产物 ==="
	@mkdir -p output
	@if [ -f src/bootloader/bin/bootx64.efi ]; then \
		cp src/bootloader/bin/bootx64.efi output/; \
		echo "已复制 bootx64.efi"; \
	fi
	@if [ -f src/bootloader/bin/bios.bin ]; then \
		cp src/bootloader/bin/bios.bin output/; \
		echo "已复制 bios.bin"; \
	fi
	@if [ -f build/bin/hik-kernel.bin ]; then \
		cp build/bin/hik-kernel.bin output/; \
		echo "已复制 hik-kernel.bin"; \
	fi
	@echo "安装完成"

# 运行构建系统脚本（GUI/TUI/CLI）
build:
	@echo "=== 运行构建系统 ==="
	@python3 scripts/build_system.py

# GDB调试（使用Makefile.debug）
debug:
	@$(MAKE) -f Makefile.debug debug

# 快速测试（使用Makefile.debug）
test:
	@$(MAKE) -f Makefile.debug test

# 构建系统帮助
help:
	@echo "HIK 构建系统"
	@echo ""
	@echo "用法:"
	@echo "  make              - 构建引导程序和内核"
	@echo "  make bootloader   - 仅构建引导程序"
	@echo "  make kernel       - 仅构建内核"
	@echo "  make clean        - 清理构建文件"
	@echo "  make install      - 安装构建产物到output目录"
	@echo "  make build        - 运行Python构建系统（GUI/TUI/CLI）"
	@echo "  make debug        - 使用GDB调试内核"
	@echo "  make test         - 快速测试内核启动"
	@echo "  make help         - 显示此帮助信息"
	@echo ""
	@echo "快速构建示例:"
	@echo "  make all && make install"
	@echo ""
	@echo "调试示例:"
	@echo "  make debug   # 完整GDB调试流程"
	@echo "  make test    # 快速启动测试"