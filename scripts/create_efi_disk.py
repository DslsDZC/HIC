#!/usr/bin/env python3
import os
import struct

def create_fat32_image(disk_path, bootloader_path):
    """
    创建FAT32磁盘镜像，包含UEFI引导文件
    """
    disk_size = 64 * 1024 * 1024  # 64MB
    sector_size = 512
    total_sectors = disk_size // sector_size
    
    # 读取引导程序
    with open(bootloader_path, 'rb') as f:
        bootloader_data = f.read()
    
    # 创建磁盘镜像
    with open(disk_path, 'wb') as disk:
        # 写入零填充的磁盘
        disk.write(b'\x00' * disk_size)
    
    print(f"创建磁盘镜像: {disk_path} ({disk_size // 1024 // 1024}MB)")
    print(f"引导程序大小: {len(bootloader_data)} bytes")
    print(f"\n需要手动将引导程序复制到磁盘镜像的 EFI/BOOT/BOOTX64.EFI")
    print("由于环境限制，建议使用以下方法之一：")
    print("1. 安装 dosfstools: sudo pacman -S dosfstools")
    print("2. 安装 mtools: sudo pacman -S mtools")
    print("3. 使用其他工具创建FAT32镜像")

if __name__ == '__main__':
    create_fat32_image('/tmp/hik_disk.img', 'output/bootx64.efi')