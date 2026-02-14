# HIK内核Makefile
# 遵循三层模型文档：Core-0、Privileged-1、Application-3分层构建

# 编译器和工具
PREFIX ?= x86_64-elf
CC      = $(PREFIX)-gcc
LD      = $(PREFIX)-ld
OBJCOPY = $(PREFIX)-objcopy
OBJDUMP = $(PREFIX)-objdump
NM      = $(PREFIX)-nm

# 项目信息
PROJECT = hik-kernel
VERSION = 0.1.0

# 目录
SRC_DIR         = ../src
CORE0_DIR       = $(SRC_DIR)/Core-0
PRIVILEGED1_DIR = $(SRC_DIR)/Privileged-1
APPLICATION3_DIR = $(SRC_DIR)/Application-3
ARCH_DIR        = $(CORE0_DIR)/arch/x86_64
LIB_DIR         = $(CORE0_DIR)/lib
BUILD_DIR       = build
BIN_DIR         = bin

# 源文件
CORE0_SRCS       = $(wildcard $(CORE0_DIR)/*.c)
PRIVILEGED1_SRCS = $(wildcard $(PRIVILEGED1_DIR)/*.c)
ARCH_SRCS        = $(wildcard $(ARCH_DIR)/*.c)
LIB_SRCS         = $(wildcard $(LIB_DIR)/*.c)
ARCH_ASM         = $(wildcard $(ARCH_DIR)/*.S)

ALL_SRCS = $(CORE0_SRCS) $(PRIVILEGED1_SRCS) $(ARCH_SRCS) $(LIB_SRCS)

# 目标文件
CORE0_OBJS       = $(patsubst $(CORE0_DIR)/%.c,$(BUILD_DIR)/Core-0/%.o,$(CORE0_SRCS))
PRIVILEGED1_OBJS = $(patsubst $(PRIVILEGED1_DIR)/%.c,$(BUILD_DIR)/Privileged-1/%.o,$(PRIVILEGED1_SRCS))
ARCH_OBJS        = $(patsubst $(ARCH_DIR)/%.c,$(BUILD_DIR)/arch/%.o,$(ARCH_SRCS))
LIB_OBJS         = $(patsubst $(LIB_DIR)/%.c,$(BUILD_DIR)/lib/%.o,$(LIB_SRCS))
ASM_OBJS         = $(patsubst $(ARCH_DIR)/%.S,$(BUILD_DIR)/arch/%.o,$(ARCH_ASM))

OBJS = $(CORE0_OBJS) $(PRIVILEGED1_OBJS) $(ARCH_OBJS) $(LIB_OBJS) $(ASM_OBJS)

# 编译选项
CFLAGS = -Wall -Wextra -Werror -nostdlib -ffreestanding \
         -fno-stack-protector -fno-omit-frame-pointer \
         -mno-red-zone -mcmodel=kernel -m64 \
         -I$(CORE0_DIR) -I$(PRIVILEGED1_DIR) -I$(CORE0_DIR)/include -I$(CORE0_DIR)/lib

# 链接选项
LDFLAGS = -nostdlib -T linker.ld -z max-page-size=0x1000

# 默认目标
all: $(BIN_DIR)/$(PROJECT).elf $(BIN_DIR)/$(PROJECT).bin

# 创建目录
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@ $(BUILD_DIR)/Core-0 $(BUILD_DIR)/Privileged-1 \
	             $(BUILD_DIR)/arch $(BUILD_DIR)/lib

# Core-0编译规则
$(BUILD_DIR)/Core-0/%.o: $(CORE0_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Privileged-1编译规则
$(BUILD_DIR)/Privileged-1/%.o: $(PRIVILEGED1_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 架构编译规则
$(BUILD_DIR)/arch/%.o: $(ARCH_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 库编译规则
$(BUILD_DIR)/lib/%.o: $(LIB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 汇编规则
$(BUILD_DIR)/arch/%.o: $(ARCH_DIR)/%.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接
$(BIN_DIR)/$(PROJECT).elf: $(OBJS) linker.ld | $(BIN_DIR)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# 提取二进制
$(BIN_DIR)/$(PROJECT).bin: $(BIN_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

# 反汇编
disasm: $(BIN_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d $< > $(BIN_DIR)/$(PROJECT).asm

# 查看符号
symbols: $(BIN_DIR)/$(PROJECT).elf
	$(NM) $< > $(BIN_DIR)/$(PROJECT).sym

# 清理
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all disasm symbols clean