# HIC 内核 Makefile（规范版本）
# 支持自动扫描源文件、清晰的分层构建

# ==================== 工具链配置 ====================
CC = gcc
LD = ld
AR = ar
OBJCOPY = objcopy
STRIP = strip

# ==================== 目录结构 ====================
ROOT_DIR = $(shell pwd)/..
SRC_DIR = $(ROOT_DIR)/src
CORE0_DIR = $(SRC_DIR)/Core-0
PRIVILEGED1_DIR = $(SRC_DIR)/Privileged-1
BOOTLOADER_DIR = $(SRC_DIR)/bootloader

BUILD_DIR = $(ROOT_DIR)/build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
OUTPUT_DIR = $(ROOT_DIR)/output

# ==================== 构建配置 ====================
include $(ROOT_DIR)/build_config.mk

# 基础编译标志
CFLAGS = -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
         -m64 -mno-red-zone -mcmodel=large -mno-mmx -mno-sse -mno-sse2 \
         -fno-pic -fno-pie -Wall -Wextra -Wpedantic \
         -Wconversion -Werror \
         -Wno-unused-function -Wno-unused-variable -O2

# 架构配置
ifeq ($(CONFIG_MMU),0)
    CFLAGS += -DCONFIG_MMU=0 -DNOMMU
else
    CFLAGS += -DCONFIG_MMU=1
endif

# 调试/优化标志
ifeq ($(CONFIG_DEBUG),1)
    CFLAGS += -g -DDEBUG
else
    CFLAGS += -DNDEBUG
endif

# 链接标志
LDFLAGS = -nostdlib -m elf_x86_64 -T $(LD_SCRIPT)

# ==================== 源文件自动扫描 ====================

# Core-0 核心文件（必须包含）
CORE0_CORE_SRCS = $(wildcard $(CORE0_DIR)/kernel_start.c \
                               $(CORE0_DIR)/hal.c \
                               $(CORE0_DIR)/chal.c \
                               $(CORE0_DIR)/pmm.c \
                               $(CORE0_DIR)/globals.c \
                               $(CORE0_DIR)/boot_info.c \
                               $(CORE0_DIR)/exception.c \
                               $(CORE0_DIR)/irq.c \
                               $(CORE0_DIR)/pagetable.c)

# Core-0 其他模块（自动扫描）
CORE0_SRCS = $(wildcard $(CORE0_DIR)/*.c)
CORE0_LIB_SRCS = $(wildcard $(CORE0_DIR)/lib/*.c)
CORE0_ARCH_SRCS = $(wildcard $(CORE0_DIR)/arch/x86_64/*.c)
CORE0_ARCH_ASMS = $(wildcard $(CORE0_DIR)/arch/x86_64/*.S)

# Privileged-1 服务（自动扫描）
PRIVILEGED1_SRCS = $(wildcard $(PRIVILEGED1_DIR)/*.c)

# 转换为目标文件
CORE0_OBJS = $(patsubst $(CORE0_DIR)/%.c,$(OBJ_DIR)/core0/%.o,$(CORE0_SRCS))
CORE0_LIB_OBJS = $(patsubst $(CORE0_DIR)/lib/%.c,$(OBJ_DIR)/core0/lib/%.o,$(CORE0_LIB_SRCS))
CORE0_ARCH_OBJS = $(patsubst $(CORE0_DIR)/arch/x86_64/%.c,$(OBJ_DIR)/core0/arch/%.o,$(CORE0_ARCH_SRCS))
CORE0_ARCH_ASM_OBJS = $(patsubst $(CORE0_DIR)/arch/x86_64/%.S,$(OBJ_DIR)/core0/arch/%_asm.o,$(CORE0_ARCH_ASMS))

PRIVILEGED1_OBJS = $(patsubst $(PRIVILEGED1_DIR)/%.c,$(OBJ_DIR)/privileged1/%.o,$(PRIVILEGED1_SRCS))

# 所有目标文件
ALL_OBJS = $(CORE0_OBJS) $(CORE0_LIB_OBJS) $(CORE0_ARCH_OBJS) $(CORE0_ARCH_ASM_OBJS) $(PRIVILEGED1_OBJS)

# ==================== 头文件路径 ====================
INCLUDES = -I$(CORE0_DIR) \
           -I$(CORE0_DIR)/include \
           -I$(CORE0_DIR)/lib \
           -I$(CORE0_DIR)/arch/x86_64 \
           -I$(PRIVILEGED1_DIR) \
           -I$(PRIVILEGED1_DIR)/include

# ==================== 链接脚本 ====================
LD_SCRIPT = $(CORE0_DIR)/arch/x86_64/linker.ld

# ==================== 构建目标 ====================
.PHONY: all clean kernel bootloader install help
.PHONY: static-analysis sparse-check cppcheck clang-analyzer

# 默认目标
all: $(BIN_DIR)/hic-kernel.elf

# 内核目标
kernel: $(BIN_DIR)/hic-kernel.elf

# 引导程序
bootloader:
	@echo "构建引导程序..."
	@cd $(BOOTLOADER_DIR) && $(MAKE) all
	@echo "引导程序构建完成"

# 安装
install: kernel bootloader
	@echo "安装构建产物..."
	@mkdir -p $(OUTPUT_DIR)
	@cp $(BIN_DIR)/hic-kernel.elf $(OUTPUT_DIR)/
	@cp $(BOOTLOADER_DIR)/bin/bootx64.efi $(OUTPUT_DIR)/ 2>/dev/null || true
	@cp $(BOOTLOADER_DIR)/bin/bios.bin $(OUTPUT_DIR)/ 2>/dev/null || true
	@echo "安装完成"

# 清理
clean:
	@echo "清理构建文件..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "清理完成"

# ==================== 链接规则 ====================
$(BIN_DIR)/hic-kernel.elf: $(ALL_OBJS) $(LD_SCRIPT) | $(BIN_DIR)
	@echo "链接内核..."
	@$(LD) $(LDFLAGS) $(ALL_OBJS) -o $@
	@echo "内核链接完成: $@"

# ==================== 编译规则 ====================

# Core-0 C文件编译
$(OBJ_DIR)/core0/%.o: $(CORE0_DIR)/%.c | $(OBJ_DIR)/core0
	@echo "编译 Core-0: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Core-0 库文件编译
$(OBJ_DIR)/core0/lib/%.o: $(CORE0_DIR)/lib/%.c | $(OBJ_DIR)/core0/lib
	@echo "编译 Core-0 库: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Core-0 架构特定C文件编译
$(OBJ_DIR)/core0/arch/%.o: $(CORE0_DIR)/arch/x86_64/%.c | $(OBJ_DIR)/core0/arch
	@echo "编译 Core-0 架构: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Core-0 架构特定汇编文件编译
$(OBJ_DIR)/core0/arch/%_asm.o: $(CORE0_DIR)/arch/x86_64/%.S | $(OBJ_DIR)/core0/arch
	@echo "汇编 Core-0 架构: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Privileged-1 C文件编译
$(OBJ_DIR)/privileged1/%.o: $(PRIVILEGED1_DIR)/%.c | $(OBJ_DIR)/privileged1
	@echo "编译 Privileged-1: $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ==================== 目录创建 ====================
$(OBJ_DIR):
	@mkdir -p $@

$(OBJ_DIR)/core0:
	@mkdir -p $@

$(OBJ_DIR)/core0/lib:
	@mkdir -p $@

$(OBJ_DIR)/core0/arch:
	@mkdir -p $@

$(OBJ_DIR)/privileged1:
	@mkdir -p $@

$(BIN_DIR):
	@mkdir -p $@

# ==================== 静态分析 ====================
.PHONY: sparse-check cppcheck clang-analyzer

# Sparse 语义检查
sparse-check:
	@echo "运行 Sparse 语义检查..."
	@$(MAKE) clean
	@$(MAKE) CHECK=sparse C=2 all
	@echo "Sparse 检查完成"

# Cppcheck 静态分析
cppcheck:
	@echo "运行 Cppcheck 静态分析..."
	@mkdir -p $(OBJ_DIR)/cppcheck
	@cppcheck --enable=all --std=c23 --suppress=missingIncludeSystem \
	         -I$(CORE0_DIR) -I$(CORE0_DIR)/include -I$(CORE0_DIR)/lib \
	         $(CORE0_SRCS) $(CORE0_LIB_SRCS) 2>&1 | tee $(OBJ_DIR)/cppcheck/core0.txt
	@echo "Cppcheck 检查完成"

# Clang Static Analyzer
clang-analyzer:
	@echo "运行 Clang Static Analyzer..."
	@mkdir -p $(OBJ_DIR)/clang-analyzer
	@$(MAKE) clean
	@scan-build -o $(OBJ_DIR)/clang-analyzer $(MAKE) all
	@echo "Clang Static Analyzer 检查完成"

# 所有静态分析
static-analysis: sparse-check cppcheck clang-analyzer
	@echo "所有静态分析检查完成"

# ==================== 帮助信息 ====================
help:
	@echo "HIC 内核构建系统"
	@echo ""
	@echo "使用方法:"
	@echo "  make                  - 构建内核（默认）"
	@echo "  make kernel           - 仅构建内核"
	@echo "  make bootloader       - 仅构建引导程序"
	@echo "  make install          - 构建并安装"
	@echo "  make clean            - 清理构建文件"
	@echo ""
	@echo "静态分析:"
	@echo "  make sparse-check     - Sparse 语义检查"
	@echo "  make cppcheck         - Cppcheck 静态分析"
	@echo "  make clang-analyzer   - Clang Static Analyzer"
	@echo "  make static-analysis  - 运行所有静态分析"
	@echo ""
	@echo "配置选项（在 build_config.mk 中设置）:"
	@echo "  CONFIG_DEBUG=0/1      - 调试支持"
	@echo "  CONFIG_MMU=0/1        - MMU 支持"
	@echo "  CONFIG_PCI=0/1        - PCI 支持"
	@echo "  CONFIG_ACPI=0/1       - ACPI 支持"
	@echo ""
	@echo "示例:"
	@echo "  make clean && make kernel              # 完整构建"
	@echo "  make CONFIG_DEBUG=1 kernel              # 调试版本"
	@echo "  make clean && make static-analysis      # 静态分析"

# ==================== 依赖关系 ====================
-include $(ALL_OBJS:.o=.d)

# 自动生成依赖文件
$(OBJ_DIR)/core0/%.d: $(CORE0_DIR)/%.c | $(OBJ_DIR)/core0
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/core0/$*.o $< > $@

$(OBJ_DIR)/core0/lib/%.d: $(CORE0_DIR)/lib/%.c | $(OBJ_DIR)/core0/lib
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/core0/lib/$*.o $< > $@

$(OBJ_DIR)/core0/arch/%.d: $(CORE0_DIR)/arch/x86_64/%.c | $(OBJ_DIR)/core0/arch
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/core0/arch/$*.o $< > $@

$(OBJ_DIR)/privileged1/%.d: $(PRIVILEGED1_DIR)/%.c | $(OBJ_DIR)/privileged1
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/privileged1/$*.o $< > $@