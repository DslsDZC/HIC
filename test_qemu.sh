#!/bin/bash
# QEMU测试脚本 - 查看审计日志

# 复制VARS文件
cp /usr/share/edk2/x64/OVMF_VARS.4m.fd /tmp/OVMF_VARS.4m.fd

# 运行QEMU并捕获输出
timeout 20 qemu-system-x86_64 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.4m.fd \
  -drive format=raw,file=output/bootx64.efi \
  -m 512M \
  -nographic 2>&1 | tee /tmp/qemu_output.log

echo "=== QEMU输出结束 ==="
echo "审计日志保存在 /tmp/qemu_output.log"