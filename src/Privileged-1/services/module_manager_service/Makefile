# 模块管理服务Makefile

SERVICE_NAME := module_manager_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdlib
CFLAGS += -I$(CURDIR)/../../Core-0/include
CFLAGS += -I$(CURDIR)/../include

OBJS := service.o

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