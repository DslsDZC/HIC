# VGA 服务 Makefile

SERVICE_NAME := vga_service
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdlib
CFLAGS += -I$(CURDIR)/../../Core-0/include
CFLAGS += -I$(CURDIR)/../include

OBJS := service.o

TARGET := $(SERVICE_NAME).o
MOD_TARGET := $(SERVICE_NAME).hicmod
TXT_FILE := hicmod.txt

.PHONY: all clean module

all: $(TARGET)

module: $(MOD_TARGET)

$(MOD_TARGET): $(TARGET) $(TXT_FILE)
	@echo "Creating module $(MOD_TARGET)..."
	@python3 $(CURDIR)/../../../scripts/create_hicmod.py $(TARGET) $(TXT_FILE) $(MOD_TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOD_TARGET)