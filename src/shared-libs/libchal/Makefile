# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# CHAL 共享库 Makefile

LIB_NAME    := libchal
LIB_VERSION := 1.0.0
LIB_UUID    := c1a10000-0000-0000-8000-000000000001

# 源文件
SRCS := chal.c

# 架构特定源文件
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    SRCS += arch/x86_64/chal_arch.c
    ARCH_FLAG := -DHICLIB_ARCH_X86_64
else ifeq ($(ARCH),aarch64)
    SRCS += arch/arm64/chal_arch.c
    ARCH_FLAG := -DHICLIB_ARCH_ARM64
else ifeq ($(ARCH),riscv64)
    SRCS += arch/riscv/chal_arch.c
    ARCH_FLAG := -DHICLIB_ARCH_RISCV
endif

# 编译器设置
CC      := gcc
CFLAGS  := -ffreestanding -nostdlib -nostdinc \
           -fno-builtin -fno-stack-protector \
           -fPIC -fvisibility=hidden \
           -O2 -Wall -Wextra \
           $(ARCH_FLAG)

# 包含路径
INCLUDES := -I../include -I../../Privileged-1/include

# 输出目录
OUT_DIR  ?= ../../../output/libs

# 目标文件
OBJS := $(SRCS:.c=.o)
LIB_HICLIB := $(OUT_DIR)/$(LIB_NAME).hiclib
LIB_ELF    := $(OUT_DIR)/$(LIB_NAME).elf

# 默认目标
all: $(LIB_HICLIB)

# 编译目标文件
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 链接 ELF
$(LIB_ELF): $(OBJS)
	@mkdir -p $(OUT_DIR)
	ld -shared -nostdlib -o $@ $^

# 转换为 hiclib 格式
$(LIB_HICLIB): $(LIB_ELF)
	@echo "Creating $(LIB_NAME).hiclib..."
	python3 ../../../scripts/create_hiclib.py \
		--input $< \
		--output $@ \
		--name $(LIB_NAME) \
		--version $(LIB_VERSION) \
		--uuid $(LIB_UUID) \
		--arch $(ARCH)

# 清理
clean:
	rm -f $(OBJS) $(LIB_ELF) $(LIB_HICLIB)

.PHONY: all clean
