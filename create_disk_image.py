#!/usr/bin/env python3
import os
import struct

# 创建FAT32磁盘镜像
def create_fat32_image(filename, bootloader_path):
    with open(filename, 'wb') as img:
        # 写入64MB空镜像
        img.write(b'\x00' * (64 * 1024 * 1024))
    
    print(f"创建磁盘镜像: {filename}")
    print(f"需要手动挂载并复制文件到 EFI/BOOT/BOOTX64.EFI")

if __name__ == '__main__':
    create_fat32_image('/tmp/hik_disk.img', 'output/bootx64.efi')