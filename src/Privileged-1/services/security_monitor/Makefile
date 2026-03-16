# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# Security Monitor Service Makefile

SERVICE_NAME := security_monitor

# 编译器设置
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc -Wno-error
CFLAGS += -I$(ROOT_DIR)/src/Privileged-1/include
CFLAGS += -I$(ROOT_DIR)/src/Core-0/include
CFLAGS += -I$(ROOT_DIR)/src/Core-0
CFLAGS += -I.

OBJS := $(OBJ_DIR)/security_monitor.service.o
TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/security_monitor.service.o: service.c service.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
