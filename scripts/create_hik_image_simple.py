#!/usr/bin/env python3
"""简化版的HIC内核映像生成脚本"""

import struct

# 读取二进制代码
with open('/home/DslsDZC/HIC/build/bin/hic-kernel.bin', 'rb') as f:
    binary_code = f.read()

# 创建HIC头部（使用切片赋值，避免struct.pack_into）
header = bytearray(120)
header[0:8] = b'HIC_IMG'
header[8:10] = struct.pack('<H', 1)
header[10:12] = struct.pack('<H', 1)
header[12:20] = struct.pack('<Q', 0)
header[20:28] = struct.pack('<Q', 160 + len(binary_code))
header[28:36] = struct.pack('<Q', 120)
header[36:44] = struct.pack('<Q', 2)
header[44:52] = struct.pack('<Q', 0)
header[52:60] = struct.pack('<Q', 0)
header[60:68] = struct.pack('<Q', 0)
header[68:76] = struct.pack('<Q', 0)
# 偏移76-119：保留（44字节），自动为0

# 创建段表（使用切片赋值）
segment_table = bytearray(80)
segment_table[0:4] = struct.pack('<I', 1)
segment_table[4:8] = struct.pack('<I', 7)
segment_table[8:16] = struct.pack('<Q', 160)
segment_table[16:24] = struct.pack('<Q', 0x100000)
segment_table[24:32] = struct.pack('<Q', len(binary_code))
segment_table[32:40] = struct.pack('<Q', len(binary_code))
segment_table[40:44] = struct.pack('<I', 4)
segment_table[44:48] = struct.pack('<I', 3)
segment_table[48:56] = struct.pack('<Q', 0)
segment_table[56:64] = struct.pack('<Q', 0x122000)
segment_table[64:72] = struct.pack('<Q', 0)
segment_table[72:80] = struct.pack('<Q', 0x2f000)

# 调试输出
print('header length:', len(header))
print('segment_table length:', len(segment_table))
print('segment_table[0]:', hex(segment_table[0]))
if len(header) > 119:
    print('header[119]:', hex(header[119]))
else:
    print('header[119] out of range!')

# 写入文件
output_path = '/home/DslsDZC/HIC/build/bin/hic-kernel.hic'
with open(output_path, 'wb') as f:
    f.write(header)
    f.write(segment_table)
    f.write(binary_code)

# 验证
with open(output_path, 'rb') as f:
    f.seek(119)
    byte119 = f.read(1)[0]
    f.seek(120)
    byte120 = f.read(1)[0]
    print('File byte 119:', hex(byte119))
    print('File byte 120:', hex(byte120))
    print('Expected byte 119: 0x00')
    print('Expected byte 120: 0x01')
    print()
    if byte119 == 0x00 and byte120 == 0x01:
        print('✓ HIC镜像生成成功！')
    else:
        print('✗ HIC镜像生成失败！')