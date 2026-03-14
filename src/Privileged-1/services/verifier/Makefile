# Verifier Service Makefile

SERVICE_NAME := verifier
SERVICE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -ffreestanding -nostdinc
CFLAGS += -I$(ROOT_DIR)/src/Privileged-1/include
CFLAGS += -I$(ROOT_DIR)/src/Core-0/include
CFLAGS += -I.

OBJS := $(OBJ_DIR)/verifier.service.o

TARGET := $(OBJ_DIR)/$(SERVICE_NAME).o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Building $(SERVICE_NAME)..."
	$(CC) $(LDFLAGS) -r $^ -o $@

$(OBJ_DIR)/verifier.service.o: service.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
