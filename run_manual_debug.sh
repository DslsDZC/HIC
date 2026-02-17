#!/bin/bash
# 手动GDB调试脚本

echo "启动QEMU（等待GDB连接）..."
qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2-ovmf/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
  -drive format=raw,file=output/hic-uefi-disk.img \
  -m 512M \
  -nographic \
  -s -S \
  > /tmp/qemu_debug.log 2>&1 &

QEMU_PID=$!
echo "QEMU PID: $QEMU_PID"
echo $QEMU_PID > /tmp/qemu.pid

echo "等待5秒让QEMU启动..."
sleep 5

echo "启动GDB调试..."
gdb -batch -x scripts/debug.gdb build/bin/hic-kernel.elf 2>&1 | tee /tmp/gdb_debug.log

echo ""
echo "清理QEMU进程..."
kill $QEMU_PID 2>/dev/null || true
rm /tmp/qemu.pid

echo ""
echo "========== QEMU输出 =========="
cat /tmp/qemu_debug.log

echo ""
echo "========== 调试完成 =========="