# HIC Privileged-1 服务主Makefile

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdlib
CFLAGS += -fno-stack-protector -fno-builtin -fno-exceptions
CFLAGS += -I$(CURDIR)/../Core-0/include
CFLAGS += -I$(CURDIR)/include
CFLAGS += -I$(CURDIR)  # 添加当前目录，以便包含 common.h

# 自动扫描所有服务（包含 service.c 和 hicmod.txt 的目录）
CORE_SERVICES := $(shell find services -mindepth 1 -maxdepth 1 -type d -exec sh -c 'test -f "$$1/service.c" && test -f "$$1/hicmod.txt" && echo "$$1"' _ {} \; | xargs -n1 basename)

# 所有服务
ALL_SERVICES := $(CORE_SERVICES)

# 构建输出目录
BUILD_DIR := $(CURDIR)/../../build/privileged-1
OBJ_DIR := $(BUILD_DIR)/obj
MOD_DIR := $(BUILD_DIR)/modules

# 目标文件（放在build目录）
SERVICE_OBJS := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(CORE_SERVICES)))

# 输出
TARGET := $(BUILD_DIR)/privileged_services.o
MOD_TARGETS := $(addprefix $(MOD_DIR)/, $(addsuffix .hicmod, $(ALL_SERVICES)))

# 模块输出目录
MODULE_OUTPUT_DIR := $(MOD_DIR)
STATIC_MODULES_DIR := $(BUILD_DIR)/static_mod

.PHONY: all clean services modules static_modules help install

all: services
ifeq ($(CONFIG_MODULE_STATIC),1)
	@echo "Static module build: embedding into kernel"
	@$(MAKE) static_modules
else
	@echo "Dynamic module build: creating .hicmod files"
	@$(MAKE) modules
endif
	@echo "All services built successfully!"

# 编译核心服务
services: $(SERVICE_OBJS)
	@echo "Linking core services..."
	@mkdir -p $(BUILD_DIR)
	$(CC) $(LDFLAGS) -r $^ -o $(TARGET)
	@echo "Core services built:"
	@for service in $(CORE_SERVICES); do \
		echo "  - $$service"; \
	done

# 编译单个服务
$(OBJ_DIR)/%.o: services/%/service.c
	@echo "Building $@..."
	@mkdir -p $(OBJ_DIR)
	$(MAKE) -C services/$* OBJ_DIR=$(OBJ_DIR) MOD_DIR=$(MOD_DIR) ROOT_DIR=$(CURDIR)/../..

# 生成动态模块
modules:
	@echo "Creating dynamic service modules..."
	@mkdir -p $(MOD_DIR)
	for service in $(CORE_SERVICES); do \
		if [ -d "services/$$service" ]; then \
			$(MAKE) -C services/$$service module OBJ_DIR=$(OBJ_DIR) MOD_DIR=$(MOD_DIR) ROOT_DIR=$(CURDIR)/../..; \
			if [ -f "services/$$service/$$service.hicmod" ]; then \
				cp "services/$$service/$$service.hicmod" $(MOD_DIR)/; \
				echo "  Created: $$service.hicmod"; \
			fi; \
		fi; \
	done
	@echo "All dynamic modules created in $(MOD_DIR)!"

# 生成静态模块（嵌入内核）
static_modules: $(MOD_TARGETS)
	@echo "Creating static modules for kernel embedding..."
	@mkdir -p $(STATIC_MODULES_DIR)
	for service in $(CORE_SERVICES); do \
		if [ -d "services/$$service" ]; then \
			$(MAKE) -C services/$$service module OBJ_DIR=$(OBJ_DIR) MOD_DIR=$(MOD_DIR) ROOT_DIR=$(CURDIR)/../..; \
			if [ -f "services/$$service/$$service.hicmod" ]; then \
				cp "services/$$service/$$service.hicmod" $(STATIC_MODULES_DIR)/; \
				echo "  Embedded: $$service.hicmod"; \
			fi; \
		fi; \
	done
	@echo "All static modules created in $(STATIC_MODULES_DIR)!"

# 安装模块到输出目录
install:
	@echo "Installing modules..."
	@echo "Modules are available in: $(MODULE_OUTPUT_DIR)"

# 清理
clean:
	@echo "Cleaning Privileged-1 services..."
	for service in $(ALL_SERVICES); do \
		if [ -d "services/$$service" ]; then \
			$(MAKE) -C services/$$service clean; \
		fi; \
	done
	rm -rf $(BUILD_DIR)
	rm -f *.o
	@echo "Clean complete"

# 帮助信息
help:
	@echo "HIC Privileged-1 Services Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all           - 编译所有核心服务（根据配置选择静态或动态）"
	@echo "  services      - 编译并链接核心服务"
	@echo "  modules       - 生成动态.hicmod模块"
	@echo "  static_modules- 生成静态模块（嵌入内核）"
	@echo "  install       - 安装模块到输出目录"
	@echo "  clean         - 清理所有构建产物"
	@echo "  help          - 显示此帮助信息"
	@echo ""
	@echo "Configuration Options (set in build_config.mk):"
	@echo "  CONFIG_MODULE_STATIC=0     - 动态模块模式（默认）"
	@echo "  CONFIG_MODULE_STATIC=1     - 静态模块模式（嵌入内核）"
	@echo "  CONFIG_DISABLE_SERIAL_SERVICE=0  - 启用串口服务（默认）"
	@echo "  CONFIG_DISABLE_VGA_SERVICE=0     - 启用VGA服务（默认）"
	@echo "  CONFIG_DISABLE_CLI_SERVICE=0     - 启用CLI服务（默认）"
	@echo ""
	@echo "Examples:"
	@echo "  make                                    # 动态模式构建"
	@echo "  make CONFIG_MODULE_STATIC=1             # 静态模式构建"
	@echo "  make CONFIG_DISABLE_CLI_SERVICE=1       # 禁用CLI服务"
	@echo ""
	@echo "Core Services:"
	@echo "  1. password_manager_service - 密码管理服务（必需）"
	@echo "  2. crypto_service          - 密码学服务（必需）"
	@echo "  3. module_manager_service  - 模块管理服务（必需）"
	@echo "  4. config_service          - 配置管理服务（必需）"
	@echo "  5. serial_service          - 串口服务（可选）"
	@echo "  6. vga_service             - VGA显示服务（可选）"
	@echo "  7. cli_service             - 命令行服务（可选）"
