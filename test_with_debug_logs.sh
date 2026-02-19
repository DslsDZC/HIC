#!/bin/bash
# HIC内核调试启动脚本（带详细日志）
# 启用QEMU/OVMF详细调试以排查UEFI FAT32驱动问题

cd /home/DslsDZC/HIC

# 清理进程
pkill -9 qemu-system-x86_64 2>/dev/null || true
sleep 1

# 清理旧的日志文件
rm -f /tmp/ovmf_debug.log /tmp/qemu_debug.log

# 创建可写的VARS
cp /usr/share/OVMF/x64/OVMF_VARS.4m.fd /tmp/OVMF_VARS.4m.fd 2>/dev/null || true

# 确保磁盘镜像存在
if [ ! -f /tmp/hic_disk.img ]; then
    echo "创建磁盘镜像..."
    dd if=/dev/zero of=/tmp/hic_disk.img bs=1M count=64 2>/dev/null
    mformat -i /tmp/hic_disk.img -F ::
    mmd -i /tmp/hic_disk.img ::/EFI
    mmd -i /tmp/hic_disk.img ::/EFI/BOOT
    mcopy -i /tmp/hic_disk.img output/bootx64.efi ::/EFI/BOOT/bootx64.efi
    mcopy -i /tmp/hic_disk.img build/bin/hic-kernel.elf ::/hic-kernel.elf
fi

echo "=================================="
echo "启动QEMU（带详细调试日志）"
echo "=================================="
echo "OVMF调试日志: /tmp/ovmf_debug.log"
echo "QEMU调试日志: /tmp/qemu_debug.log"
echo "=================================="
echo ""
echo "使用方式："
echo "1. 观察启动过程"
echo "2. 查看OVMF日志: cat /tmp/ovmf_debug.log | strings"
echo "3. 查看QEMU日志: cat /tmp/qemu_debug.log | strings"
echo "4. 按Ctrl+C退出"
echo ""

qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
  -drive format=raw,file=/tmp/hic_disk.img \
  -m 512M \
  -nographic \
  -serial stdio \
  -debugcon file:/tmp/ovmf_debug.log \
  -global isa-debugcon.iobase=0x402 \
  -D /tmp/qemu_debug.log \
  -d guest_errors,int,unimp,cpu_reset \
  -s

echo ""
echo "=================================="
echo "QEMU已退出"
echo "=================================="