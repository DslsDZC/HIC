#!/bin/bash
# 全自动测试脚本 - 运行到特权层和审计日志系统初始化

echo "========== HIK内核全自动测试 =========="
echo "目标: 验证内核能够运行到特权层和审计日志系统"
echo ""

# 清理
echo "[1/6] 清理旧进程和文件..."
killall qemu-system-x86_64 2>/dev/null || true
rm -f /tmp/qemu_test.log /tmp/serial_test.log /tmp/test_output.log
sleep 1
echo "✓ 清理完成"

# 编译
echo ""
echo "[2/6] 编译bootloader和内核..."
cd src/bootloader && make clean > /dev/null 2>&1 && make > /dev/null 2>&1
cd ../build && make clean > /dev/null 2>&1 && make > /dev/null 2>&1
cd ../..
echo "✓ 编译完成"

# 准备磁盘镜像
echo ""
echo "[3/6] 准备磁盘镜像..."
cp src/bootloader/bin/bootx64.efi output/ > /dev/null 2>&1
cd build && objcopy -O binary bin/hik-kernel.elf bin/hik-kernel.bin > /dev/null 2>&1 && cd ..
cp build/bin/hik-kernel.bin output/ > /dev/null 2>&1
rm -f output/hik-uefi-disk.img
dd if=/dev/zero of=output/hik-uefi-disk.img bs=1M count=64 2>/dev/null
mkfs.fat -F 32 output/hik-uefi-disk.img > /dev/null 2>&1
mmd -i output/hik-uefi-disk.img ::EFI > /dev/null 2>&1
mmd -i output/hik-uefi-disk.img ::EFI/BOOT > /dev/null 2>&1
mcopy -i output/hik-uefi-disk.img output/bootx64.efi ::EFI/BOOT/BOOTX64.EFI > /dev/null 2>&1
mcopy -i output/hik-uefi-disk.img output/hik-kernel.bin ::hik-kernel.bin > /dev/null 2>&1
echo "✓ 磁盘镜像准备完成"

# 运行测试
echo ""
echo "[4/6] 启动QEMU并测试..."
echo "（测试运行20秒，捕获所有输出）"

timeout 20 qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2-ovmf/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
  -drive format=raw,file=output/hik-uefi-disk.img \
  -m 512M \
  -nographic \
  -serial file:/tmp/serial_test.log \
  > /tmp/qemu_test.log 2>&1 || true

echo "✓ 测试运行完成"

# 分析结果
echo ""
echo "[5/6] 分析测试结果..."
echo ""

# 关键指标
BOOTLOADER_OK=$(grep -c "HIK UEFI Bootloader" /tmp/serial_test.log 2>/dev/null || echo 0)
KERNEL_LOADED=$(grep -c "Kernel loaded" /tmp/serial_test.log 2>/dev/null || echo 0)
KERNEL_JUMP=$(grep -c "jumping to kernel" /tmp/serial_test.log 2>/dev/null || echo 0)
HELLO_HIK=$(grep -c "Hello HIK" /tmp/serial_test.log 2>/dev/null || echo 0)
AUDIT_INIT=$(grep -c "audit" /tmp/serial_test.log 2>/dev/null || echo 0)
PRIVILEGED=$(grep -c "Privileged" /tmp/serial_test.log 2>/dev/null || echo 0)

echo "========== 测试结果 =========="
echo ""
echo "✓ Bootloader启动: $BOOTLOADER_OK"
echo "✓ 内核加载: $KERNEL_LOADED" 
echo "✓ 内核跳转: $KERNEL_JUMP"
echo "✓ Hello HIK输出: $HELLO_HIK"
echo "✓ 审计系统: $AUDIT_INIT (提到audit的次数)"
echo "✓ 特权层: $PRIVILEGED (提到Privileged的次数)"
echo ""

# 详细输出
echo "========== 详细日志 =========="
echo ""

echo "[Bootloader阶段]"
grep -E "HIK UEFI|Starting HIK|Auto-boot|Kernel loaded" /tmp/serial_test.log | tail -10
echo ""

echo "[内核跳转阶段]"
grep -E "jumping to kernel|Entry point|Before jump" /tmp/serial_test.log | tail -10
echo ""

echo "[审计系统阶段]"
grep -i "audit" /tmp/serial_test.log | tail -10
echo ""

echo "[特权层阶段]"
grep -i "privileged" /tmp/serial_test.log | tail -10
echo ""

# 最后检查
echo "[6/6] 最终检查..."

if [ $BOOTLOADER_OK -gt 0 ]; then
    echo "✅ Bootloader成功启动"
else
    echo "❌ Bootloader启动失败"
fi

if [ $KERNEL_LOADED -gt 0 ]; then
    echo "✅ 内核成功加载"
else
    echo "❌ 内核加载失败"
fi

if [ $KERNEL_JUMP -gt 0 ]; then
    echo "✅ 成功跳转到内核"
else
    echo "❌ 内核跳转失败"
fi

if [ $HELLO_HIK -gt 0 ]; then
    echo "✅ 内核成功输出 Hello HIK"
else
    echo "❌ 内核没有输出 Hello HIK"
fi

if [ $AUDIT_INIT -gt 0 ]; then
    echo "✅ 审计系统相关: $AUDIT_INIT 次提及"
else
    echo "❌ 没有审计系统相关输出"
fi

if [ $PRIVILEGED -gt 0 ]; then
    echo "✅ 特权层相关: $PRIVILEGED 次提及"
else
    echo "❌ 没有特权层相关输出"
fi

echo ""
echo "========== 测试完成 =========="
echo ""
echo "完整日志保存在: /tmp/serial_test.log"
echo "QEMU输出保存在: /tmp/qemu_test.log"
echo ""
