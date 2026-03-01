#!/usr/bin/env python3
"""创建HIC内核映像格式"""

import struct
import sys
import os

HIC_IMG_MAGIC = b"HIC_IMG\x00"  # 8字节魔数
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
    
    # 段信息
    # 从ELF文件中读取段信息
    # .text 段: 0x100000, 大小 0x19ff6
    # .data 段: 0x121000, 大小 0xa03
    # .bss  段: 0x122000, 大小 0x2f000
    
    code_size = len(binary_code)
    bss_offset = 0x22000  # .bss 段在 binary_code 中的偏移 (0x122000 - 0x100000)
    bss_size = 0x2f000    # .bss 段大小
    
    print(f"代码大小: 0x{code_size:x}")
    print(f"BSS 偏移: 0x{bss_offset:x}")
    print(f"BSS 大小: 0x{bss_size:x}")
    
    # HIC映像头部 (120字节)
    header = bytearray(120)
    header[0:8] = HIC_IMG_MAGIC  # 魔数"HIC_IMG" (8字节)
    struct.pack_into('<H', header, 8, 1)
    struct.pack_into('<H', header, 10, HIC_IMG_VERSION)
    struct.pack_into('<Q', header, 12, entry_offset)  # 使用实际的入口点偏移
    total_size = 160 + len(binary_code)
    struct.pack_into('<Q', header, 20, total_size)
    struct.pack_into('<Q', header, 28, 120)
    struct.pack_into('<Q', header, 36, 2)  # 2个段
    struct.pack_into('<Q', header, 44, 0)
    struct.pack_into('<Q', header, 52, 0)
    struct.pack_into('<Q', header, 60, 0)
    struct.pack_into('<Q', header, 68, 0)
    
    # 段表 (80字节, 2个段项)
    segment_table = bytearray(80)
    
    # 段1: 代码段（包含 .text 和 .data）
    struct.pack_into('<I', segment_table, 0, 1)  # CODE
    struct.pack_into('<I', segment_table, 4, 7)  # READABLE | WRITABLE | EXECUTABLE
    struct.pack_into('<Q', segment_table, 8, 160)
    struct.pack_into('<Q', segment_table, 16, 0x100000)
    struct.pack_into('<Q', segment_table, 24, code_size)
    struct.pack_into('<Q', segment_table, 32, code_size)
    
    # 段2: BSS 段
    struct.pack_into('<I', segment_table, 40, 4)  # BSS
    struct.pack_into('<I', segment_table, 44, 3)  # READABLE | WRITABLE
    struct.pack_into('<Q', segment_table, 48, 0)  # 文件中没有数据
    struct.pack_into('<Q', segment_table, 56, 0x122000)
    struct.pack_into('<Q', segment_table, 64, 0)  # 文件大小为0
    struct.pack_into('<Q', segment_table, 72, bss_size)  # 内存大小
    
    # 调试：输出段表字节
    print(f"Segment table bytes: {' '.join(f'{b:02x}' for b in segment_table)}")
    print(f"Segment 0 type: {struct.unpack('<I', segment_table[0:4])[0]}")
    print(f"Segment 0 file_offset: {hex(struct.unpack('<Q', segment_table[8:16])[0])}")
    print(f"Segment 0 memory_offset: {hex(struct.unpack('<Q', segment_table[16:24])[0])}")
    
    # 调试：输出头部字节
    print(f"Header bytes (offsets 112-119): {' '.join(f'{b:02x}' for b in header[112:120])}")
    
    # 写入文件（使用os.write避免潜在问题）
    fd = os.open(output_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    os.write(fd, header)
    os.write(fd, segment_table)
    os.write(fd, binary_code)
    os.close(fd)
    
    # 调试：验证写入的数据
    with open(output_path, 'rb') as f:
        f.seek(112)
        header_tail = f.read(8)
        f.seek(120)
        segment_table_head = f.read(8)
        print(f"Verification - Header[112:120]: {' '.join(f'{b:02x}' for b in header_tail)}")
        print(f"Verification - Segment[120:128]: {' '.join(f'{b:02x}' for b in segment_table_head)}")
    
    print(f"✓ HIC映像创建成功: {output_path}")
    print(f"  总大小: {total_size} bytes")
    print(f"  段数: 2 (代码段 + BSS段)")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("用法: python3 create_hic_image.py <elf文件> <输出文件>")
        sys.exit(1)
    
    if not create_hic_image(sys.argv[1], sys.argv[2]):
        sys.exit(1)
