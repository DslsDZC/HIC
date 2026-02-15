# HIK Bootloader Makefile
# 支持UEFI和BIOS两种引导方式

# 编译器和工具
UEFI_CC      = x86_64-w64-mingw32-gcc
UEFI_LD      = x86_64-w64-mingw32-ld
UEFI_OBJCOPY = x86_64-w64-mingw32-objcopy

BIOS_CC      = x86_64-elf-gcc
BIOS_LD      = x86_64-elf-ld
BIOS_OBJCOPY = x86_64-elf-objcopy
BIOS_AS      = x86_64-elf-as

# 项目信息
UEFI_PROJECT = bootx64
BIOS_PROJECT = bios
VERSION = 1.0.0

# 目录
SRC_DIR    = src
INCLUDE_DIR = include
LIB_DIR    = lib
OBJ_DIR    = obj
BIN_DIR    = bin

# 默认构建目标（可以设置为uefi或bios）
BUILD_TARGET ?= uefi

# 源文件
UEFI_CORE_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/bootlog.c $(SRC_DIR)/stdlib.c $(SRC_DIR)/string.c $(SRC_DIR)/console.c $(SRC_DIR)/efi_guids.c
# UEFI_CRYPTO_SRCS = $(wildcard $(SRC_DIR)/crypto/*.c)  # 暂时禁用
UEFI_LIB_SRCS  = $(wildcard $(LIB_DIR)/*.c)
UEFI_SRCS      = $(UEFI_CORE_SRCS) $(UEFI_LIB_SRCS)

BIOS_C_SRCS    = $(wildcard $(SRC_DIR)/bios.c) $(wildcard $(SRC_DIR)/console_bios.c)
BIOS_ASM_SRCS  = $(wildcard $(SRC_DIR)/bios_entry.S)
BIOS_SRCS      = $(BIOS_C_SRCS) $(BIOS_ASM_SRCS)

# 目标文件
UEFI_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/uefi_%.o,$(UEFI_SRCS))
BIOS_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/bios_%.o,$(BIOS_C_SRCS))
BIOS_OBJS += $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/bios_%.o,$(BIOS_ASM_SRCS))

# UEFI编译选项
UEFI_CFLAGS = -Wall -Wextra -Werror -ffreestanding \
             -fno-stack-protector -fno-stack-clash-protection \
             -fno-omit-frame-pointer -fno-pic -fno-pie \
             -fno-asynchronous-unwind-tables -O0 \
             -mno-red-zone -mcmodel=small -m64 \
             -fshort-wchar -mno-ms-bitfields \
             -I$(INCLUDE_DIR) -I$(SRC_DIR)

UEFI_LDFLAGS = -nostdlib -shared -e UefiMain \
               --subsystem=10 \
               -Map $(BIN_DIR)/$(UEFI_PROJECT).map

# BIOS编译选项
BIOS_CFLAGS = -Wall -Wextra -Werror -ffreestanding \
             -fno-stack-protector -fno-pic -fno-pie \
             -mno-red-zone -mcmodel=large -m64 \
             -I$(INCLUDE_DIR) -I$(SRC_DIR)

BIOS_ASFLAGS = -m64

BIOS_LDFLAGS = -nostdlib -T bios.lds -e _start \
               -Wl,-Map,$(BIN_DIR)/$(BIOS_PROJECT).map

# 默认目标
all:
	@echo "Building HIK Bootloader..."
	@echo "Target: $(BUILD_TARGET)"
	$(MAKE) $(BUILD_TARGET)

# UEFI目标
uefi: $(BIN_DIR)/$(UEFI_PROJECT).efi
	@echo "UEFI Bootloader built successfully"

# BIOS目标
bios: $(BIN_DIR)/$(BIOS_PROJECT).bin
	@echo "BIOS Bootloader built successfully"

# 双目标（同时构建）
both: uefi bios
	@echo "Both UEFI and BIOS bootloaders built"

# 创建目录
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# UEFI编译规则
$(OBJ_DIR)/uefi_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

$(OBJ_DIR)/uefi_%.o: $(LIB_DIR)/%.c | $(OBJ_DIR)
	$(UEFI_CC) $(UEFI_CFLAGS) -c $< -o $@

# BIOS编译规则
$(OBJ_DIR)/bios_%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(BIOS_CC) $(BIOS_CFLAGS) -c $< -o $@

$(OBJ_DIR)/bios_%.o: $(SRC_DIR)/%.S | $(OBJ_DIR)
	$(BIOS_AS) $(BIOS_ASFLAGS) $< -o $@

# UEFI链接
$(BIN_DIR)/$(UEFI_PROJECT).efi: $(UEFI_OBJS) efi.lds | $(BIN_DIR)
	$(UEFI_LD) $(UEFI_LDFLAGS) $(UEFI_OBJS) -o $@

# BIOS链接
$(BIN_DIR)/$(BIOS_PROJECT).elf: $(BIOS_OBJS) bios.lds | $(BIN_DIR)
	$(BIOS_LD) $(BIOS_LDFLAGS) $(BIOS_OBJS) -o $@

# 生成BIOS二进制
$(BIN_DIR)/$(BIOS_PROJECT).bin: $(BIN_DIR)/$(BIOS_PROJECT).elf
	$(BIOS_OBJCOPY) -O binary $< $@

# 清理
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaning..."

# 帮助
help:
	@echo "HIK Bootloader Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build default target ($(BUILD_TARGET))"
	@echo "  make uefi          - Build UEFI bootloader"
	@echo "  make bios          - Build BIOS bootloader"
	@echo "  make both          - Build both UEFI and BIOS"
	@echo "  make clean         - Clean build artifacts"
	@echo "  make help          - Show this help"
	@echo ""
	@echo "Build types:"
	@echo "  BUILD_TARGET=uefi  - Build UEFI bootloader (default)"
	@echo "  BUILD_TARGET=bios  - Build BIOS bootloader"

.PHONY: all uefi bios both clean help
