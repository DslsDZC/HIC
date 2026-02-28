# 模块管理服务Makefile

SERVICE_NAME := module_manager_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CFLAGS += -I$(ROOT_DIR)/src/Privileged-1/include
CFLAGS += -I.
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc
CFLAGS += -I$(ROOT_DIR)/src/Core-0/include
CFLAGS += -I$(ROOT_DIR)/src/Privileged-1/include
CFLAGS += -I.

OBJS := $(OBJ_DIR)/module_manager_service.service.o

TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
MOD_TARGET := $(MOD_DIR)/$(SERVICE_NAME).hicmod
TXT_FILE := hicmod.txt

.PHONY: all clean module

all: $(TARGET)

module: $(MOD_TARGET)

$(MOD_TARGET): $(TARGET) $(TXT_FILE)
	@echo "Creating module $(MOD_TARGET)..."
	@python3 $(ROOT_DIR)/scripts/create_hicmod.py $(TARGET) $(TXT_FILE) $(MOD_TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/module_manager_service.service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOD_TARGET)