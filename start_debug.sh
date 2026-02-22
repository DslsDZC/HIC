#!/bin/bash
# HIC内核GDB调试启动脚本
# 使用方法: ./start_debug.sh
# 然后在另一个终端运行: gdb -x scripts/debug_manual.gdb build/bin/hic-kernel.elf

cd /home/*/HIC

# 清理进程
pkill -9 qemu-system-x86_64 2>/dev/null || true
sleep 1

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

echo "启动QEMU（监听端口1234，按Ctrl+C退出）..."
qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
  -drive format=raw,file=/tmp/hic_disk.img \
  -m 512M \
  -nographic \
  -s -S