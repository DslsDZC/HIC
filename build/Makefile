# HIK 内核 Makefile（架构无关版本）

CC = gcc
LD = ld

CFLAGS = -ffreestanding -nostdlib -fno-stack-protector -fno-builtin \
         -m64 -mno-red-zone -mcmodel=large -mno-mmx -mno-sse -mno-sse2 \
         -fno-pic -fno-pie -Wall -Wextra \
         -Wno-unused-function -Wno-unused-variable -O2

LDFLAGS = -nostdlib -m elf_x86_64

OBJ_DIR = obj
BIN_DIR = bin
LD_SCRIPT = kernel.ld

.PHONY: all clean

all: $(BIN_DIR)/hik-kernel.elf

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
             $(OBJ_DIR)/console.o \
             $(OBJ_DIR)/mem.o \
             $(OBJ_DIR)/string.o

# 架构特定汇编文件
ALL_OBJS = $(OBJ_DIR)/entry.o $(CORE_OBJS) $(OTHER_OBJS)

# 架构特定汇编文件
$(OBJ_DIR)/entry.o: ../src/Core-0/arch/x86_64/entry.S | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

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

$(OBJ_DIR)/syscall.o: ../src/Core-0/syscall.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/audit.o: ../src/Core-0/audit.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/formal_verification.o: ../src/Core-0/formal_verification.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/performance.o: ../src/Core-0/performance.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/hardware_probe.o: ../src/Core-0/hardware_probe.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/module_loader.o: ../src/Core-0/module_loader.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/monitor.o: ../src/Core-0/monitor.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/build_config.o: ../src/Core-0/build_config.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/runtime_config.o: ../src/Core-0/runtime_config.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/yaml.o: ../src/Core-0/yaml.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/pkcs1.o: ../src/Core-0/pkcs1.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# Privileged-1
$(OBJ_DIR)/privileged_service.o: ../src/Privileged-1/privileged_service.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@

# 库
$(OBJ_DIR)/console.o: ../src/Core-0/lib/console.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/mem.o: ../src/Core-0/lib/mem.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

$(OBJ_DIR)/string.o: ../src/Core-0/lib/string.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Privileged-1 -c $< -o $@

# 链接
$(BIN_DIR)/hik-kernel.elf: $(ALL_OBJS) $(LD_SCRIPT) | $(BIN_DIR)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -e kernel_entry --allow-multiple-definition $(ALL_OBJS) -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
