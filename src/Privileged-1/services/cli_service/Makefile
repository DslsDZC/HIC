# CLI 服务 Makefile

SERVICE_NAME := cli_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc
CFLAGS += -I/home/DslsDZC/HIC/src/Privileged-1/include
CFLAGS += -I/home/DslsDZC/HIC/src/Core-0/include
CFLAGS += -I.

OBJS := $(OBJ_DIR)/service.o

TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o
MOD_TARGET := $(MOD_DIR)/$(SERVICE_NAME).hicmod
TXT_FILE := hicmod.txt

.PHONY: all clean module

all: $(TARGET)

module: $(MOD_TARGET)

$(MOD_TARGET): $(TARGET) $(TXT_FILE)
	@echo "Creating module $(MOD_TARGET)..."
	@python3 /home/DslsDZC/HIC/scripts/create_hicmod.py $(TARGET) $(TXT_FILE) $(MOD_TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOD_TARGET)
	rm -f string.o ctype.o stdio.o stdlib.o