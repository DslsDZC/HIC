# HIC 内核 Makefile（架构无关版本）

CC = gcc
LD = ld

# ==================== Sparse语义检查支持 ====================
# 使用方式：
# make C=1          # 对修改的文件运行Sparse检查
# make C=2          # 对所有文件运行Sparse检查
# CF=              # 指定Sparse参数（默认：-Wbitwise -Wcontext -Wdecl）
ifeq ("$(origin C)", "command line")
  ifeq ("$(C)", "1")
    CHECK := sparse
    CHECK_OPTS := -Wbitwise -Wcontext -Wdecl -Wcast-truncate -Wone-bit-signed-bitfield -Wtransparent-union
  else
    ifeq ("$(C)", "2")
      CHECK := sparse
      CHECK_OPTS := -Wbitwise -Wcontext -Wdecl -Wcast-truncate -Wone-bit-signed-bitfield -Wtransparent-union
    else
      CHECK := 
    endif
  endif
else
  CHECK := 
endif

# 如果指定了CF参数，覆盖默认选项
ifdef CF
  CHECK_OPTS := $(CF)
endif

# Sparse检查命令
CHECK_CMD = $(CHECK) $(CHECK_OPTS)

# ==================== Cppcheck静态分析支持 ====================
# 使用方式：
# make CPPCHECK=1    # 运行Cppcheck静态分析
# make cppcheck      # 运行完整的Cppcheck检查
# CPPCHECK_RULES=   # 指定自定义规则文件（默认：../scripts/cppcheck_rules.xml）
ifeq ($(CPPCHECK),1)
  RUN_CPPCHECK := 1
else
  RUN_CPPCHECK := 
endif

# Cppcheck命令
CPPCHECK ?= cppcheck
CPPCHECK_OPTS := --enable=all --inconclusive --std=c23 --suppress=missingIncludeSystem \
                 --suppress=unusedFunction --suppress=uninitStructMember \
                 --template="{file}:{line}: {severity} ({id}): {message}"

# 自定义规则文件（可选）
CPPCHECK_RULES ?= ../scripts/cppcheck_rules.xml

# Cppcheck检查命令（带自定义规则）
CPPCHECK_CMD_RULES = $(CPPCHECK) $(CPPCHECK_OPTS) --rule-file=$(CPPCHECK_RULES)

# Cppcheck检查命令（不带自定义规则）
CPPCHECK_CMD = $(CPPCHECK) $(CPPCHECK_OPTS)

# ==================== Clang Static Analyzer支持 ====================
# Clang Static Analyzer使用方式：
# make clang-analyzer    # 运行Clang Static Analyzer
CLANG_ANALYZER ?= scan-build
CLANG_ANALYZER_OPTS ?= -analyze-headers --use-cc=clang --use-c++=clang++

# Clang Static Analyzer检查命令
CLANG_ANALYZER_CMD = $(CLANG_ANALYZER) $(CLANG_ANALYZER_OPTS) -o $(OBJ_DIR)/clang-analyzer

# 包含构建配置
-include ../build_config.mk

# 基础编译标志
CFLAGS = -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
         -m64 -mno-red-zone -mcmodel=large -mno-mmx -mno-sse -mno-sse2 \
         -fno-pic -fno-pie -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
         -Wconversion -Werror \
         -Wno-unused-function -Wno-unused-variable -O2

# 架构配置选项
ifeq ($(CONFIG_MMU),0)
    CFLAGS += -DCONFIG_MMU=0 -DNOMMU
else
    CFLAGS += -DCONFIG_MMU=1
endif

ifeq ($(CONFIG_MPU),1)
    CFLAGS += -DCONFIG_MPU=1
else
    CFLAGS += -DCONFIG_MPU=0
endif

ifeq ($(CONFIG_PHYSICAL_MAPPING),1)
    CFLAGS += -DCONFIG_PHYSICAL_MAPPING=1
else
    CFLAGS += -DCONFIG_PHYSICAL_MAPPING=0
endif

LDFLAGS = -nostdlib -m elf_x86_64

OBJ_DIR = obj
BIN_DIR = bin
LD_SCRIPT = kernel.ld

.PHONY: all clean sparse-check cppcheck clang-analyzer static-analysis

all: $(BIN_DIR)/hic-kernel.elf

# Sparse检查目标
sparse-check:
	@echo "Running Sparse semantic check..."
	$(MAKE) C=2 clean all
	@echo "Sparse check completed."

# Cppcheck检查目标
cppcheck:
	@echo "Running Cppcheck static analysis..."
	@mkdir -p $(OBJ_DIR)/cppcheck
	@echo "Checking Core-0 files..."
	$(CPPCHECK_CMD) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 \
		../src/Core-0/*.c 2>&1 | tee $(OBJ_DIR)/cppcheck/core0.txt
	@echo "Checking Privileged-1 files..."
	$(CPPCHECK_CMD) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 \
		../src/Privileged-1/*.c 2>&1 | tee $(OBJ_DIR)/cppcheck/p1.txt
	@echo "Checking library files..."
	$(CPPCHECK_CMD) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 \
		../src/Core-0/lib/*.c 2>&1 | tee $(OBJ_DIR)/cppcheck/lib.txt
	@echo "Cppcheck check completed. Results saved to $(OBJ_DIR)/cppcheck/"

# Clang Static Analyzer检查目标
clang-analyzer:
	@echo "Running Clang Static Analyzer..."
	@mkdir -p $(OBJ_DIR)/clang-analyzer
	@$(MAKE) clean
	$(CLANG_ANALYZER_CMD) $(MAKE) all
	@echo "Clang Static Analyzer check completed. Results saved to $(OBJ_DIR)/clang-analyzer/"

# 静态分析（运行所有检查）
static-analysis: sparse-check cppcheck clang-analyzer
	@echo "All static analysis checks completed."

# 通用编译规则（支持Sparse检查）
$(OBJ_DIR)/%.o: ../src/Core-0/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@
	@if [ -n "$(CHECK)" ]; then \
		echo "Running Sparse on $<..."; \
		$(CHECK_CMD) -D__CHECKER__ $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 $<; \
	fi

$(OBJ_DIR)/%.o: ../src/Privileged-1/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@
	@if [ -n "$(CHECK)" ]; then \
		echo "Running Sparse on $<..."; \
		$(CHECK_CMD) -D__CHECKER__ $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 $<; \
	fi

$(OBJ_DIR)/%.o: ../src/Core-0/lib/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@
	@if [ -n "$(CHECK)" ]; then \
		echo "Running Sparse on $<..."; \
		$(CHECK_CMD) -D__CHECKER__ $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 $<; \
	fi

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# Core-0核心文件（架构无关）
CORE_OBJS = $(OBJ_DIR)/kernel_start.o \
            $(OBJ_DIR)/hal.o \
            $(OBJ_DIR)/hal_impl_simple.o \
            $(OBJ_DIR)/pmm.o \
            $(OBJ_DIR)/globals.o \
            $(OBJ_DIR)/boot_info.o \
            $(OBJ_DIR)/exception.o \
            $(OBJ_DIR)/irq.o \
            $(OBJ_DIR)/pagetable.o

# 其他模块
OTHER_OBJS = $(OBJ_DIR)/capability.o \
             $(OBJ_DIR)/domain.o \
             $(OBJ_DIR)/domain_switch.o \
             $(OBJ_DIR)/scheduler.o \
             $(OBJ_DIR)/thread.o \
             $(OBJ_DIR)/syscall.o \
             $(OBJ_DIR)/audit.o \
             $(OBJ_DIR)/formal_verification.o \
             $(OBJ_DIR)/performance.o \
             $(OBJ_DIR)/hardware_probe.o \
             $(OBJ_DIR)/module_loader.o \
             $(OBJ_DIR)/monitor.o \
             $(OBJ_DIR)/build_config.o \
             $(OBJ_DIR)/runtime_config.o \
             $(OBJ_DIR)/yaml.o \
             $(OBJ_DIR)/pkcs1.o \
             $(OBJ_DIR)/privileged_service.o \
             $(OBJ_DIR)/capability_manager_service.o \
             $(OBJ_DIR)/scheduler_service.o \
             $(OBJ_DIR)/memory_manager_service.o \
             $(OBJ_DIR)/irq_controller_service.o \
             $(OBJ_DIR)/syscall_dispatcher_service.o \
             $(OBJ_DIR)/module_manager_service.o \
             $(OBJ_DIR)/console.o \
             $(OBJ_DIR)/mem.o \
             $(OBJ_DIR)/string.o \
             $(OBJ_DIR)/nommu.o \
             $(OBJ_DIR)/minimal_uart.o \
             $(OBJ_DIR)/apm.o

# 架构特定汇编文件
ALL_OBJS = $(OBJ_DIR)/entry.o $(CORE_OBJS) $(OTHER_OBJS)

# 架构特定汇编文件
$(OBJ_DIR)/entry.o: ../src/Core-0/arch/x86_64/entry.S | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 无MMU架构支持
$(OBJ_DIR)/nommu.o: ../src/Core-0/nommu.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# 极简串口驱动
$(OBJ_DIR)/minimal_uart.o: ../src/Core-0/minimal_uart.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# APM 系统
$(OBJ_DIR)/apm.o: ../src/Core-0/apm.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# Core-0 C文件
$(OBJ_DIR)/kernel_start.o: ../src/Core-0/kernel_start.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/hal.o: ../src/Core-0/hal.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/pmm.o: ../src/Core-0/pmm.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/globals.o: ../src/Core-0/globals.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/boot_info.o: ../src/Core-0/boot_info.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/exception.o: ../src/Core-0/exception.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/irq.o: ../src/Core-0/irq.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/pagetable.o: ../src/Core-0/pagetable.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/hal_impl_simple.o: ../src/Core-0/arch/x86_64/hal_impl_simple.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# 其他C文件
$(OBJ_DIR)/capability.o: ../src/Core-0/capability.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/domain.o: ../src/Core-0/domain.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/domain_switch.o: ../src/Core-0/domain_switch.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/scheduler.o: ../src/Core-0/scheduler.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/thread.o: ../src/Core-0/thread.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@



$(OBJ_DIR)/pkcs1.o: ../src/Core-0/pkcs1.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# Privileged-1
$(OBJ_DIR)/privileged_service.o: ../src/Privileged-1/privileged_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/capability_manager_service.o: ../src/Privileged-1/capability_manager_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/scheduler_service.o: ../src/Privileged-1/scheduler_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/memory_manager_service.o: ../src/Privileged-1/memory_manager_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/irq_controller_service.o: ../src/Privileged-1/irq_controller_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/syscall_dispatcher_service.o: ../src/Privileged-1/syscall_dispatcher_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/module_manager_service.o: ../src/Privileged-1/module_manager_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# 库
$(OBJ_DIR)/console.o: ../src/Core-0/lib/console.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/mem.o: ../src/Core-0/lib/mem.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/string.o: ../src/Core-0/lib/string.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

# 链接
$(BIN_DIR)/hic-kernel.elf: $(ALL_OBJS) $(LD_SCRIPT) | $(BIN_DIR)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -e kernel_entry --allow-multiple-definition $(ALL_OBJS) -o $@
	@echo "内核链接完成"
	@echo "运行内存区域重叠检查..."
	@python3 ../scripts/check_memory_overlap.py ../iso_output/platform.yaml $(BIN_DIR)/hic-kernel.elf
	@if [ $$? -ne 0 ]; then \
		echo "错误: 内存区域检查失败，构建终止"; \
		exit 1; \
	fi

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# 显示Sparse使用帮助
help-sparse:
	@echo "Sparse语义检查使用指南:"
	@echo "  make C=1              # 对修改的文件运行Sparse检查"
	@echo "  make C=2              # 对所有文件运行Sparse检查"
	@echo "  make sparse-check     # 运行完整的Sparse检查（等同于C=2）"
	@echo "  make CF='-Wbitwise' C=1 # 自定义Sparse参数"
	@echo ""
	@echo "地址空间标记（在types.h中定义）:"
	@echo "  __kernel      - Core-0内核空间（address_space(0)）"
	@echo "  __capability - 能力系统对象空间（address_space(1)）"
	@echo "  __p1          - Privileged-1服务内存空间（address_space(2)）"
	@echo "  __p3          - Application-3用户空间（address_space(3)）"
	@echo "  __io          - I/O端口/MMIO空间（address_space(4)）"
	@echo ""
	@echo "示例用法（在代码中）:"
	@echo "  #ifdef __CHECKER__"
	@echo "      __capability cap_entry_t *cap;"
	@echo "  #else"
	@echo "      cap_entry_t *cap;"
	@echo "  #endif"
	@echo ""
	@echo "或直接使用标记（正常编译时自动消失）:"
	@echo "  __capability cap_entry_t *cap;"

# 显示Cppcheck使用帮助
help-cppcheck:
	@echo "Cppcheck静态分析使用指南:"
	@echo "  make cppcheck         # 运行Cppcheck静态分析"
	@echo "  make static-analysis  # 运行所有静态分析（Sparse + Cppcheck + Clang）"
	@echo "  CPPCHECK_RULES=path/to/rules.xml make cppcheck # 使用自定义规则"
	@echo ""
	@echo "自定义规则文件: $(CPPCHECK_RULES)"
	@echo "检查结果保存在: $(OBJ_DIR)/cppcheck/"
	@echo ""
	@echo "能力系统检查规则:"
	@echo "  - 未验证能力就访问资源"
	@echo "  - 能力撤销后继续使用"
	@echo "  - 跨域指针误用"
	@echo "  - 内存泄漏检查"

# 显示Clang Static Analyzer使用帮助
help-clang:
	@echo "Clang Static Analyzer使用指南:"
	@echo "  make clang-analyzer  # 运行Clang Static Analyzer"
	@echo "  make static-analysis # 运行所有静态分析（Sparse + Cppcheck + Clang）"
	@echo ""
	@echo "检查结果保存在: $(OBJ_DIR)/clang-analyzer/"
	@echo "查看结果: 用浏览器打开 $(OBJ_DIR)/clang-analyzer/index.html"
	@echo ""
	@echo "检测能力:"
	@echo "  - 空指针解引用"
	@echo "  - 内存泄漏"
	@echo "  - 未初始化变量使用"
	@echo "  - 死代码检测"
	@echo "  - 缓冲区溢出"

# 显示所有帮助
help: help-sparse help-cppcheck help-clang
	@echo ""
	@echo "快速开始:"
	@echo "  make static-analysis  # 运行完整的静态分析"
	@echo "  make C=2              # 对所有文件运行Sparse检查"
	@echo "  make sparse-check     # 运行完整的Sparse检查（等同于C=2）"
	@echo "  make CF='-Wbitwise' C=1 # 自定义Sparse参数"
	@echo "  make cppcheck         # 运行Cppcheck静态分析"
	@echo "  make clang-analyzer   # 运行Clang Static Analyzer"
	@echo ""
	@echo "地址空间标记（在types.h中定义）:"
	@echo "  __kernel      - Core-0内核空间（address_space(0)）"
	@echo "  __capability - 能力系统对象空间（address_space(1)）"
	@echo "  __p1          - Privileged-1服务内存空间（address_space(2)）"
	@echo "  __p3          - Application-3用户空间（address_space(3)）"
	@echo "  __io          - I/O端口/MMIO空间（address_space(4)）"
	@echo ""
	@echo "示例用法（在代码中）:"
	@echo "  #ifdef __CHECKER__"
	@echo "      __capability cap_entry_t *cap;"
	@echo "  #else"
	@echo "      cap_entry_t *cap;"
	@echo "  #endif"
	@echo ""
	@echo "或直接使用标记（正常编译时自动消失）:"
	@echo "  __capability cap_entry_t *cap;"
	@echo ""
	@echo "未安装的工具（可选）:"
	@echo "  - STANSE: http://www.fi.muni.cz/~xsvenda/stanse.html"
	@echo "  - IKOS: https://github.com/NASA-SW-VnV/ikos"
