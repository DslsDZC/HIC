#!/usr/bin/env python3
import os
import subprocess
import sys

def create_fat32_image(disk_path, bootloader_path, kernel_path):
    """
    创建FAT32磁盘镜像，包含UEFI引导文件和内核 (无需sudo)
    """
    print(f"创建FAT32磁盘镜像: {disk_path}")
    print(f"引导程序: {bootloader_path}")
    print(f"内核: {kernel_path}")
    
    # 检查文件是否存在
    if not os.path.exists(bootloader_path):
        print(f"错误: 引导程序不存在: {bootloader_path}")
        return False
    
    if not os.path.exists(kernel_path):
        print(f"错误: 内核不存在: {kernel_path}")
        return False
    
    # 如果内核是 ELF 文件，转换为 HIK 格式
    if kernel_path.endswith('.elf'):
        print("检测到 ELF 格式内核，转换为 HIK 格式...")
        hik_path = kernel_path.replace('.elf', '.hic')
        
        # 首先创建 .bin 文件
        bin_path = kernel_path.replace('.elf', '.bin')
        if not os.path.exists(bin_path):
            print(f"创建二进制文件: {bin_path}")
            subprocess.run([
                'objcopy', '-O', 'binary', kernel_path, bin_path
            ], check=True)
        
        # 使用 create_hik_image.py 转换
        script_dir = os.path.dirname(os.path.abspath(__file__))
        create_hik_script = os.path.join(script_dir, 'create_hik_image.py')
        subprocess.run([
            'python3', create_hik_script, kernel_path, hik_path
        ], check=True)
        
        print(f"✓ HIK 镜像创建成功: {hik_path}")
        kernel_path = hik_path
    
    try:
        # 计算需要的块数
        bootloader_size = os.path.getsize(bootloader_path)
        kernel_size = os.path.getsize(kernel_path)
        total_size = bootloader_size + kernel_size + 32 * 1024 * 1024  # 额外32MB
        block_count = (total_size // 512) + 10000  # 增加更多空间
        
        # 创建FAT32镜像
        print(f"创建镜像文件 (块数: {block_count})...")
        subprocess.run([
            'dd', 'if=/dev/zero', f'of={disk_path}', 
            f'bs=512', f'count={block_count}'
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # 格式化为FAT32
        print("格式化为FAT32...")
        subprocess.run([
            'mkfs.vfat', '-F', '32', disk_path
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # 使用mtools复制文件 (无需sudo)
        print("复制EFI引导文件...")
        # 创建EFI/BOOT目录
        subprocess.run([
            'mmd', '-i', disk_path, 'EFI'
        ], check=True)
        subprocess.run([
            'mmd', '-i', disk_path, 'EFI/BOOT'
        ], check=True)
        
        # 复制引导程序
        subprocess.run([
            'mcopy', '-i', disk_path, bootloader_path, '::EFI/BOOT/BOOTX64.EFI'
        ], check=True)
        
        # 复制内核
        print("复制内核...")
        kernel_filename = 'hic-kernel.hic' if kernel_path.endswith('.hic') else 'hic-kernel.bin'
        subprocess.run([
            'mcopy', '-i', disk_path, kernel_path, f'::{kernel_filename}'
        ], check=True)
        
        print(f"✓ 磁盘镜像创建成功: {disk_path}")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"错误: 创建FAT32镜像失败: {e}")
        return False

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("用法: python3 create_efi_disk_no_root.py --bootloader <path> --kernel <path> --output <path>")
        sys.exit(1)
    
    bootloader_path = sys.argv[sys.argv.index('--bootloader') + 1]
    kernel_path = sys.argv[sys.argv.index('--kernel') + 1]
    output_path = sys.argv[sys.argv.index('--output') + 1]
    
    if not create_fat32_image(output_path, bootloader_path, kernel_path):
        sys.exit(1)