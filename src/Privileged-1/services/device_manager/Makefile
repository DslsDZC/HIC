# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# 设备管理服务 Makefile

# 服务名称
SERVICE_NAME := device_manager

# 编译器设置
CC := gcc
CFLAGS := -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
          -Wall -Wextra -Wno-error -std=c11 -O2 -g \
          -fno-builtin -fno-common -fno-exceptions

# 包含路径（兼容传入的 ROOT_DIR 或 HIC_ROOT）
HIC_ROOT ?= $(ROOT_DIR)
INCLUDES := -I$(HIC_ROOT)/src/Core-0 \
            -I$(HIC_ROOT)/src/Core-0/include \
            -I$(HIC_ROOT)/src/Core-0/lib \
            -I$(HIC_ROOT)/src/bootloader/include \
            -I$(HIC_ROOT)/build/include \
            -I.

# 输出目录（兼容传入的 OBJ_DIR）
OBJ_DIR ?= .
MOD_DIR ?= .

# 目标文件路径
TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
SERVICE_OBJ := $(OBJ_DIR)/$(SERVICE_NAME).service.o

.PHONY: all clean module install

# 默认目标
all: $(TARGET)

# 编译服务（输出到 OBJ_DIR）
$(TARGET): $(SERVICE_OBJ)
	@echo "Building $(SERVICE_NAME)..."
	$(LD) -r $^ -o $@

$(SERVICE_OBJ): service.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 生成模块（兼容父 Makefile 调用）
module: $(TARGET)
	@mkdir -p $(MOD_DIR)
	@echo "Module created: $(SERVICE_NAME).o"

# 安装目标（兼容父 Makefile 调用）
install:
	@echo "Installing $(SERVICE_NAME)..."

# 清理
clean:
	rm -f $(TARGET) $(SERVICE_OBJ) *.o
