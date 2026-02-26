# HIC Privileged-1 服务主Makefile

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdlib
CFLAGS += -I$(CURDIR)/../Core-0/include
CFLAGS += -I$(CURDIR)/include

# 核心服务（按启动优先级排序）
CORE_SERVICES := password_manager_service crypto_service module_manager_service config_service serial_service cli_service

# 所有服务
ALL_SERVICES := $(CORE_SERVICES)

# 目标文件
SERVICE_OBJS := $(addsuffix .o, $(CORE_SERVICES))

# 输出
TARGET := privileged_services.o
MOD_TARGETS := $(addsuffix .hicmod, $(ALL_SERVICES))

.PHONY: all clean services modules help

all: services
	@echo "All services built successfully!"

# 编译核心服务
services: $(SERVICE_OBJS)
	@echo "Linking core services..."
	$(CC) $(LDFLAGS) -r $^ -o $(TARGET)
	@echo "Core services built successfully"
	@echo "  - password_manager_service (priority 1)"
	@echo "  - crypto_service (priority 2)"
	@echo "  - module_manager_service (priority 3)"
	@echo "  - config_service (priority 4)"
	@echo "  - serial_service (priority 5)"
	@echo "  - cli_service (priority 6)"

# 编译单个服务
%.o: services/%/service.c
	@echo "Building $@..."
	$(MAKE) -C $(dir $<)
	$(CC) $(LDFLAGS) -r services/$*/service.o -o $@

# 生成模块
modules: $(MOD_TARGETS)

%.hicmod:
	@echo "Creating module $@..."
	$(MAKE) -C services/$* clean
	$(MAKE) -C services/$*
	# TODO: 调用create_hicmod.sh生成模块

# 清理
clean:
	@echo "Cleaning Privileged-1 services..."
	for service in $(ALL_SERVICES); do \
		if [ -d "services/$$service" ]; then \
			$(MAKE) -C services/$$service clean; \
		fi; \
	done
	rm -f $(SERVICE_OBJS) $(TARGET) $(MOD_TARGETS)
	@echo "Clean complete"

# 帮助信息
help:
	@echo "HIC Privileged-1 Services Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - 编译所有核心服务"
	@echo "  services  - 编译并链接核心服务"
	@echo "  modules   - 生成所有.hicmod模块"
	@echo "  clean     - 清理所有构建产物"
	@echo "  help      - 显示此帮助信息"
	@echo ""
	@echo "Core Services:"
	@echo "  1. password_manager_service - 密码管理服务"
	@echo "  2. crypto_service          - 密码学服务"
	@echo "  3. module_manager_service  - 模块管理服务"
	@echo "  4. config_service          - 配置管理服务"
	@echo "  5. serial_service          - 串口服务（读取内核输出）"
	@echo "  6. cli_service             - 命令行服务"
	@echo ""
	@echo "Service Directory Structure:"
	@echo "  services/"
	@echo "    └── <service_name>/"
	@echo "        ├── hicmod.txt"
	@echo "        ├── service.h"
	@echo "        ├── service.c"
	@echo "        └── Makefile"
