# Memory Service Makefile

SERVICE_NAME := memory_service

CC := gcc
CFLAGS := -Wall -Wextra -Wno-error -O2 -fPIC -ffreestanding -nostdinc
CFLAGS += -I$(ROOT_DIR)/src/Privileged-1/include
CFLAGS += -I$(ROOT_DIR)/src/Core-0/include
CFLAGS += -I$(ROOT_DIR)/src/Core-0
CFLAGS += -I$(ROOT_DIR)/src/Core-0/lib
CFLAGS += -I.

# 兼容传入的目录
OBJ_DIR ?= .
MOD_DIR ?= .

TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
SERVICE_OBJ := $(OBJ_DIR)/$(SERVICE_NAME).service.o

.PHONY: all clean module install

all: $(TARGET)

$(TARGET): $(SERVICE_OBJ)
	@echo "Building $(SERVICE_NAME)..."
	$(LD) -r $^ -o $@

$(SERVICE_OBJ): service.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 生成模块（兼容父 Makefile 调用）
module: $(TARGET)
	@mkdir -p $(MOD_DIR)
	@echo "Module created: $(SERVICE_NAME).o"

# 安装目标
install:
	@echo "Installing $(SERVICE_NAME)..."

clean:
	rm -f $(TARGET) $(SERVICE_OBJ) *.o
