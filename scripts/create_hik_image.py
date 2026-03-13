#!/usr/bin/env python3
"""创建HIC内核映像格式"""

import struct
import sys
import os
import subprocess

HIC_IMG_MAGIC = b"HIC_IMG\x00"  # 8字节魔数
HIC_IMG_VERSION = 1

def get_elf_sections(elf_path):
    """从ELF文件中读取段信息"""
    sections = {}
    try:
        result = subprocess.run(
            ['objdump', '-h', elf_path],
            capture_output=True, text=True, check=True
        )
        for line in result.stdout.split('\n'):
            parts = line.split()
            if len(parts) >= 6 and parts[0].isdigit():
                name = parts[1]
                size = int(parts[2], 16)
                vma = int(parts[3], 16)
                lma = int(parts[4], 16)
                file_off = int(parts[5], 16)
                sections[name] = {
                    'size': size,
                    'vma': vma,
                    'lma': lma,
                    'file_off': file_off
                }
    except Exception as e:
        print(f"警告: 无法读取ELF段信息: {e}")
    return sections

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
    entry_point = struct.unpack('<Q', elf_data[24:32])[0]
    entry_offset = entry_point - 0x100000
    
    print(f"ELF入口点: 0x{entry_point:016x}")
    print(f"入口点偏移: 0x{entry_offset:016x}")
    
    # 从ELF文件读取实际段信息
    sections = get_elf_sections(elf_path)
    
    # 获取实际的段信息
    text_section = sections.get('.text', {'size': 0x1f6b8, 'vma': 0x100000})
    rodata_section = sections.get('.rodata', {'size': 0x6e84, 'vma': 0x120000})
    data_section = sections.get('.data', {'size': 0xa22, 'vma': 0x127000})
    bss_section = sections.get('.bss', {'size': 0x32000, 'vma': 0x128000})
    
    # 计算段在binary文件中的偏移
    # binary文件是平铺的，从0x100000开始
    # 计算每个段相对于0x100000的偏移
    text_size = text_section['size']
    text_vma = text_section['vma']
    
    rodata_size = rodata_section['size']
    rodata_vma = rodata_section['vma']
    rodata_offset = rodata_vma - 0x100000  # 在binary中的偏移
    
    data_size = data_section['size']
    data_vma = data_section['vma']
    data_offset = data_vma - 0x100000  # 在binary中的偏移
    
    bss_size = bss_section['size']
    bss_vma = bss_section['vma']
    bss_offset = bss_vma - 0x100000
    
    print(f"代码段(.text): VMA=0x{text_vma:x}, 大小=0x{text_size:x}")
    print(f"只读数据(.rodata): VMA=0x{rodata_vma:x}, 大小=0x{rodata_size:x}, binary偏移=0x{rodata_offset:x}")
    print(f"数据段(.data): VMA=0x{data_vma:x}, 大小=0x{data_size:x}, binary偏移=0x{data_offset:x}")
    print(f"BSS段: VMA=0x{bss_vma:x}, 大小=0x{bss_size:x}")
    
    # 合并 .rodata 和 .data 作为一个数据段
    # 数据段从 .rodata 开始到 .data 结束
    combined_data_offset = rodata_offset
    combined_data_vma = rodata_vma
    combined_data_size = (data_vma + data_size) - rodata_vma
    
    print(f"合并数据段: VMA=0x{combined_data_vma:x}, 大小=0x{combined_data_size:x}, binary偏移=0x{combined_data_offset:x}")
    
    # HIC映像头部 (120字节)
    header = bytearray(120)
    header[0:8] = HIC_IMG_MAGIC
    struct.pack_into('<H', header, 8, 1)
    struct.pack_into('<H', header, 10, HIC_IMG_VERSION)
    struct.pack_into('<Q', header, 12, entry_offset)
    total_size = 160 + len(binary_code)
    struct.pack_into('<Q', header, 20, total_size)
    struct.pack_into('<Q', header, 28, 120)
    struct.pack_into('<Q', header, 36, 3)  # 3个段：代码段、数据段、BSS段
    struct.pack_into('<Q', header, 44, 0)
    struct.pack_into('<Q', header, 52, 0)
    struct.pack_into('<Q', header, 60, 0)
    struct.pack_into('<Q', header, 68, 0)
    
    # 段表 (240字节, 3个段项，每个40字节)
    segment_table = bytearray(240)
    
    # 段1: 代码段（.text）
    struct.pack_into('<I', segment_table, 0, 1)  # CODE
    struct.pack_into('<I', segment_table, 4, 7)  # READABLE | WRITABLE | EXECUTABLE
    struct.pack_into('<Q', segment_table, 8, 360)  # file_offset = 360 = 头部(120) + 段表(240)
    struct.pack_into('<Q', segment_table, 16, text_vma)  # memory_offset
    struct.pack_into('<Q', segment_table, 24, text_size)  # file_size
    struct.pack_into('<Q', segment_table, 32, text_size)  # memory_size
    
    # 段2: 数据段（.rodata + .data 合并）
    struct.pack_into('<I', segment_table, 40, 2)  # DATA
    struct.pack_into('<I', segment_table, 44, 3)  # READABLE | WRITABLE
    struct.pack_into('<Q', segment_table, 48, 360 + combined_data_offset)  # file_offset
    struct.pack_into('<Q', segment_table, 56, combined_data_vma)  # memory_offset
    struct.pack_into('<Q', segment_table, 64, combined_data_size)  # file_size
    struct.pack_into('<Q', segment_table, 72, combined_data_size)  # memory_size
    
    # 段3: BSS 段
    struct.pack_into('<I', segment_table, 80, 4)  # BSS
    struct.pack_into('<I', segment_table, 84, 3)  # READABLE | WRITABLE
    struct.pack_into('<Q', segment_table, 88, 0)  # file_offset = 0 (BSS无文件数据)
    struct.pack_into('<Q', segment_table, 96, bss_vma)  # memory_offset
    struct.pack_into('<Q', segment_table, 104, 0)  # file_size = 0
    struct.pack_into('<Q', segment_table, 112, bss_size)  # memory_size
    
    # 调试输出
    print(f"段表字节: {len(segment_table)} bytes")
    print(f"段0 (CODE): type={struct.unpack('<I', segment_table[0:4])[0]}, file_off=0x{struct.unpack('<Q', segment_table[8:16])[0]:x}, mem_off=0x{struct.unpack('<Q', segment_table[16:24])[0]:x}, size=0x{struct.unpack('<Q', segment_table[24:32])[0]:x}")
    print(f"段1 (DATA): type={struct.unpack('<I', segment_table[40:44])[0]}, file_off=0x{struct.unpack('<Q', segment_table[48:56])[0]:x}, mem_off=0x{struct.unpack('<Q', segment_table[56:64])[0]:x}, size=0x{struct.unpack('<Q', segment_table[64:72])[0]:x}")
    print(f"段2 (BSS): type={struct.unpack('<I', segment_table[80:84])[0]}, mem_off=0x{struct.unpack('<Q', segment_table[96:104])[0]:x}, size=0x{struct.unpack('<Q', segment_table[112:120])[0]:x}")
    
    # 写入文件
    fd = os.open(output_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
    os.write(fd, header)
    os.write(fd, segment_table)
    os.write(fd, binary_code)
    os.close(fd)
    
    # 验证
    with open(output_path, 'rb') as f:
        f.seek(112)
        header_tail = f.read(8)
        f.seek(120)
        segment_table_head = f.read(8)
        print(f"验证 - Header[112:120]: {' '.join(f'{b:02x}' for b in header_tail)}")
        print(f"验证 - Segment[120:128]: {' '.join(f'{b:02x}' for b in segment_table_head)}")
    
    print(f"✓ HIC映像创建成功: {output_path}")
    print(f"  总大小: {total_size} bytes")
    print(f"  段数: 3 (代码段 + 数据段 + BSS段)")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("用法: python3 create_hik_image.py <elf文件> <输出文件>")
        sys.exit(1)
    
    if not create_hic_image(sys.argv[1], sys.argv[2]):
        sys.exit(1)