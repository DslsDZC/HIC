#!/usr/bin/env python3
"""创建HIC内核映像格式"""

import struct
import sys
import os

HIC_IMG_MAGIC = b"HIC_IMG"
HIC_IMG_VERSION = 1

def create_hic_image(elf_path, output_path):
    with open(elf_path, 'rb') as f:
        elf_data = f.read()
    
    bin_path = elf_path.replace('.elf', '.bin')
    if not os.path.exists(bin_path):
        print(f"错误: {bin_path} 不存在")
        return False
    
    with open(bin_path, 'rb') as f:
        binary_code = f.read()
    
    # 从ELF文件中读取入口点偏移
    # ELF header: e_ident[16] (16字节), e_type (2), e_machine (2), e_version (4), e_entry (8)
    # e_entry在偏移24处，是64位值
    entry_point = struct.unpack('<Q', elf_data[24:32])[0]
    # 计算入口点相对于代码段开始的偏移
    entry_offset = entry_point - 0x100000
    
    print(f"ELF入口点: 0x{entry_point:016x}")
    print(f"入口点偏移: 0x{entry_offset:016x}")
    
    # HIC映像头部 (120字节)
    header = bytearray(120)
    header[0:8] = HIC_IMG_MAGIC
    struct.pack_into('<H', header, 8, 1)
    struct.pack_into('<H', header, 10, HIC_IMG_VERSION)
    struct.pack_into('<Q', header, 12, entry_offset)  # 使用实际的入口点偏移
    total_size = 160 + len(binary_code)
    struct.pack_into('<Q', header, 20, total_size)
    struct.pack_into('<Q', header, 28, 120)
    struct.pack_into('<Q', header, 36, 1)
    struct.pack_into('<Q', header, 44, 0)
    struct.pack_into('<Q', header, 52, 0)
    struct.pack_into('<Q', header, 60, 0)
    struct.pack_into('<Q', header, 68, 0)
    
    # 段表 (40字节, 1个段项)
    segment_table = bytearray(40)
    struct.pack_into('<I', segment_table, 0, 1)
    struct.pack_into('<I', segment_table, 4, 0)
    struct.pack_into('<Q', segment_table, 8, 160)
    struct.pack_into('<Q', segment_table, 16, 0x100000)
    struct.pack_into('<Q', segment_table, 24, len(binary_code))
    struct.pack_into('<Q', segment_table, 32, len(binary_code))
    
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(segment_table)
        f.write(binary_code)
    
    print(f"✓ HIC映像创建成功: {output_path}")
    print(f"  总大小: {total_size} bytes")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("用法: python3 create_hic_image.py <elf文件> <输出文件>")
        sys.exit(1)
    
    if not create_hic_image(sys.argv[1], sys.argv[2]):
        sys.exit(1)
