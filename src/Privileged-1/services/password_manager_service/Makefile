# 密码管理服务Makefile

SERVICE_NAME := password_manager_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# 编译器
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdlib
CFLAGS += -I$(CURDIR)/../../Core-0/include
CFLAGS += -I$(CURDIR)/../include

# 目标文件
OBJS := service.o

# 输出
TARGET := $(SERVICE_NAME).o
MOD_TARGET := $(SERVICE_NAME).hicmod

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOD_TARGET)