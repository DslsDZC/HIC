#!/usr/bin/env python3
"""
创建 HIC 服务模块 (.hicmod)
将 .o 文件和 hicmod.txt 元数据打包成模块格式
"""

import struct
import sys
import os
import uuid

HICMOD_MAGIC = 0x48494B4D  # "HICM"
HICMOD_VERSION = 1

def parse_hicmod_txt(txt_path):
    """解析 hicmod.txt 元数据文件"""
    metadata = {}
    current_section = None
    
    with open(txt_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # 跳过注释和空行
            if not line or line.startswith('#'):
                continue
            
            # 检查是否是节
            if line.startswith('[') and line.endswith(']'):
                current_section = line[1:-1]
                metadata[current_section] = {}
            elif current_section and '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                metadata[current_section][key] = value
    
    return metadata

def create_hicmod(obj_path, txt_path, output_path):
    """创建 .hicmod 模块文件"""
    
    # 检查输入文件
    if not os.path.exists(obj_path):
        print(f"错误: {obj_path} 不存在")
        return False
    
    if not os.path.exists(txt_path):
        print(f"错误: {txt_path} 不存在")
        return False
    
    # 读取对象文件
    with open(obj_path, 'rb') as f:
        obj_data = f.read()
    
    # 解析元数据
    metadata = parse_hicmod_txt(txt_path)
    
    # 获取服务基本信息
    service_info = metadata.get('service', {})
    name = service_info.get('name', 'unknown')
    version_str = service_info.get('version', '1.0.0')
    api_version_str = service_info.get('api_version', '1.0')
    uuid_str = service_info.get('uuid', str(uuid.uuid4()))
    
    # 解析版本号 (major.minor.patch -> packed)
    version_parts = [int(x) for x in version_str.split('.')]
    semantic_version = (version_parts[0] << 16) | (version_parts[1] << 8) | version_parts[2]
    
    # 解析 API 版本
    api_version_parts = [int(x) for x in api_version_str.split('.')]
    while len(api_version_parts) < 3:
        api_version_parts.append(0)
    api_version = (api_version_parts[0] << 16) | (api_version_parts[1] << 8) | api_version_parts[2]
    
    # 构建 UUID 字节数组
    try:
        # 尝试解析 UUID
        uuid_obj = uuid.UUID(uuid_str)
        uuid_bytes = uuid_obj.bytes
    except:
        # 使用字符串的 MD5 作为 UUID
        import hashlib
        uuid_bytes = hashlib.md5(name.encode()).digest()
    
    # 计算各部分偏移
    header_size = 72  # hicmod_header_t 的大小
    metadata_size = len(obj_data) + 128  # 元数据区域大小（代码+额外空间）
    
    # 构建模块头
    header = bytearray(header_size)
    struct.pack_into('<I', header, 0, HICMOD_MAGIC)
    struct.pack_into('<I', header, 4, HICMOD_VERSION)
    header[8:24] = uuid_bytes
    struct.pack_into('<I', header, 24, semantic_version)
    struct.pack_into('<I', header, 28, 0)  # API 描述符偏移（暂时为0）
    struct.pack_into('<I', header, 32, len(obj_data))  # 代码段大小
    struct.pack_into('<I', header, 36, 0)  # 数据段大小
    struct.pack_into('<I', header, 40, 0)  # 签名偏移（暂时为0）
    struct.pack_into('<I', header, 44, header_size)  # 头部大小
    
    # 将 hicmod.txt 内容作为元数据追加
    with open(txt_path, 'r', encoding='utf-8') as f:
        txt_data = f.read().encode('utf-8')
    
    # 构建完整的模块数据
    module_data = header + obj_data + txt_data
    
    # 写入输出文件
    with open(output_path, 'wb') as f:
        f.write(module_data)
    
    print(f"成功创建模块: {output_path}")
    print(f"  模块名称: {name}")
    print(f"  模块版本: {version_str}")
    print(f"  UUID: {uuid_str}")
    print(f"  代码大小: {len(obj_data)} 字节")
    print(f"  总大小: {len(module_data)} 字节")
    
    return True

def main():
    if len(sys.argv) < 4:
        print("用法: create_hicmod.py <对象文件> <元数据文件> <输出文件>")
        print("示例: create_hicmod.py serial_service.o hicmod.txt serial_service.hicmod")
        sys.exit(1)
    
    obj_path = sys.argv[1]
    txt_path = sys.argv[2]
    output_path = sys.argv[3]
    
    if create_hicmod(obj_path, txt_path, output_path):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()