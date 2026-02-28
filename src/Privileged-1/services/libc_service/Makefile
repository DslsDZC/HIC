# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# libc 服务 Makefile

SERVICE_NAME := libc_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc
CFLAGS += -I/home/DslsDZC/HIC/src/Privileged-1/include
CFLAGS += -I/home/DslsDZC/HIC/src/Core-0/include
CFLAGS += -I.

# 服务源文件
SERVICE_SRCS := service.c

# Lib源文件
LIB_SRCS := string.c ctype.c stdio.c stdlib.c

# 所有源文件
SRCS := $(SERVICE_SRCS) $(LIB_SRCS)

# 输出目录（通过变量传入）
OBJ_DIR ?= .
MOD_DIR ?= .

OBJS := $(addprefix $(OBJ_DIR)/,$(SRCS:.c=.o))

TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
MOD_TARGET := $(MOD_DIR)/$(SERVICE_NAME).hicmod
TXT_FILE := hicmod.txt

.PHONY: all clean module

all: $(TARGET)

module: $(MOD_TARGET)

$(MOD_TARGET): $(TARGET) $(TXT_FILE)
	@echo "Creating module $(MOD_TARGET)..."
	@mkdir -p $(MOD_DIR)
	@python3 /home/DslsDZC/HIC/scripts/create_hicmod.py $(TARGET) $(TXT_FILE) $(MOD_TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	@mkdir -p $(OBJ_DIR)
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOD_TARGET)