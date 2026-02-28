#!/usr/bin/env python3
"""
创建 HIC 服务模块 (.hicmod)
将 .o 文件和 hicmod.txt 元数据打包成模块格式
支持数字签名（Ed25519）
"""

import struct
import sys
import os
import uuid
import hashlib
import json

HICMOD_MAGIC = 0x48494B4D  # "HICM"
HICMOD_VERSION = 1
HICMOD_SIGNATURE_OFFSET = 40

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

def create_signature(data, private_key_path=None):
    """创建签名（使用 Ed25519）"""
    if private_key_path and os.path.exists(private_key_path):
        try:
            from cryptography.hazmat.primitives import serialization
            from cryptography.hazmat.primitives.asymmetric import ed25519
            from cryptography.exceptions import InvalidSignature
            
            # 加载私钥
            with open(private_key_path, 'rb') as f:
                private_key = ed25519.Ed25519PrivateKey.from_private_bytes(
                    serialization.load_pem_private_key(f.read(), password=None).private_bytes(
                        encoding=serialization.Encoding.Raw,
                        format=serialization.PrivateFormat.Raw,
                        encryption_algorithm=serialization.NoEncryption()
                    )
                )
            
            # 签名
            signature = private_key.sign(data)
            return signature
        except ImportError:
            print("警告: cryptography 库未安装，跳过签名")
            return None
        except Exception as e:
            print(f"警告: 签名失败: {e}")
            return None
    else:
        return None

def verify_signature(data, signature, public_key_path=None):
    """验证签名"""
    if not signature or not public_key_path or not os.path.exists(public_key_path):
        return False
    
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import ed25519
        
        # 加载公钥
        with open(public_key_path, 'rb') as f:
            public_key = ed25519.Ed25519PublicKey.from_public_bytes(
                serialization.load_pem_public_key(f.read()).public_bytes(
                    encoding=serialization.Encoding.Raw,
                    format=serialization.PublicFormat.Raw
                )
            )
        
        # 验证签名
        public_key.verify(signature, data)
        return True
    except ImportError:
        return False
    except Exception as e:
        return False

def calculate_checksum(data):
    """计算模块校验和（SHA-256）"""
    return hashlib.sha256(data).digest()

def create_hicmod(obj_path, txt_path, output_path, private_key_path=None, public_key_path=None):
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
        uuid_bytes = hashlib.md5(name.encode()).digest()
    
    # 计算各部分偏移
    header_size = 72  # hicmod_header_t 的大小
    
    # 将 hicmod.txt 内容作为元数据追加
    with open(txt_path, 'r', encoding='utf-8') as f:
        txt_data = f.read().encode('utf-8')
    
    # 构建模块数据（不含签名）
    module_body = obj_data + txt_data
    
    # 计算校验和
    checksum = calculate_checksum(module_body)
    
    # 构建模块头
    header = bytearray(header_size)
    struct.pack_into('<I', header, 0, HICMOD_MAGIC)
    struct.pack_into('<I', header, 4, HICMOD_VERSION)
    header[8:24] = uuid_bytes
    struct.pack_into('<I', header, 24, semantic_version)
    struct.pack_into('<I', header, 28, 0)  # API 描述符偏移（暂时为0）
    struct.pack_into('<I', header, 32, len(obj_data))  # 代码段大小
    struct.pack_into('<I', header, 36, 0)  # 数据段大小
    struct.pack_into('<I', header, 40, 0)  # 签名偏移（将在之后设置）
    struct.pack_into('<I', header, 44, header_size)  # 头部大小
    header[48:64] = checksum  # 存储校验和
    struct.pack_into('<I', header, 64, 0)  # 签名大小（暂时为0）
    struct.pack_into('<I', header, 68, 0)  # 标志位（暂时为0）
    
    # 尝试签名
    signature = create_signature(module_body, private_key_path)
    signature_size = len(signature) if signature else 0
    
    # 如果有签名，添加到头部
    if signature:
        header[48:64] = checksum  # 校验和
        struct.pack_into('<I', header, 40, header_size + len(module_body))  # 签名偏移
        struct.pack_into('<I', header, 64, signature_size)  # 签名大小
        # 标志位：bit0 = 已签名
        struct.pack_into('<I', header, 68, 0x01)
    
    # 构建完整的模块数据
    module_data = header + module_body
    if signature:
        module_data += signature
    
    # 写入输出文件
    with open(output_path, 'wb') as f:
        f.write(module_data)
    
    print(f"成功创建模块: {output_path}")
    print(f"  模块名称: {name}")
    print(f"  模块版本: {version_str}")
    print(f"  UUID: {uuid_str}")
    print(f"  代码大小: {len(obj_data)} 字节")
    print(f"  总大小: {len(module_data)} 字节")
    if signature:
        print(f"  状态: 已签名 (Ed25519, {signature_size} 字节)")
    else:
        print(f"  状态: 未签名")
    
    return True

def verify_hicmod(module_path, public_key_path=None):
    """验证 .hicmod 模块文件"""
    if not os.path.exists(module_path):
        print(f"错误: {module_path} 不存在")
        return False
    
    with open(module_path, 'rb') as f:
        module_data = f.read()
    
    # 解析头部
    if len(module_data) < 72:
        print("错误: 模块数据过小")
        return False
    
    magic = struct.unpack_from('<I', module_data, 0)[0]
    if magic != HICMOD_MAGIC:
        print("错误: 无效的模块魔数")
        return False
    
    version = struct.unpack_from('<I', module_data, 4)[0]
    if version != HICMOD_VERSION:
        print(f"错误: 不支持的模块版本 {version}")
        return False
    
    code_size = struct.unpack_from('<I', module_data, 32)[0]
    header_size = struct.unpack_from('<I', module_data, 44)[0]
    signature_offset = struct.unpack_from('<I', module_data, 40)[0]
    signature_size = struct.unpack_from('<I', module_data, 64)[0]
    flags = struct.unpack_from('<I', module_data, 68)[0]
    
    is_signed = (flags & 0x01) != 0
    
    # 提取模块体
    module_body = module_data[header_size:header_size + code_size + 256]  # 包含元数据
    
    # 验证校验和
    stored_checksum = module_data[48:64]
    calculated_checksum = calculate_checksum(module_body)
    
    if stored_checksum != calculated_checksum:
        print("错误: 校验和不匹配，模块可能已损坏")
        return False
    
    # 验证签名
    if is_signed and public_key_path:
        if signature_offset == 0 or signature_size == 0:
            print("错误: 模块标记为已签名，但缺少签名数据")
            return False
        
        signature = module_data[signature_offset:signature_offset + signature_size]
        if verify_signature(module_body, signature, public_key_path):
            print("验证成功: 签名有效")
        else:
            print("错误: 签名验证失败")
            return False
    elif is_signed:
        print("警告: 模块已签名，但未提供公钥进行验证")
    
    print(f"验证成功: 模块格式正确 (版本 {version})")
    return True

def main():
    if len(sys.argv) < 4:
        print("用法: create_hicmod.py <对象文件> <元数据文件> <输出文件> [私钥文件] [公钥文件]")
        print("示例: create_hicmod.py serial_service.o hicmod.txt serial_service.hicmod")
        print("       create_hicmod.py serial_service.o hicmod.txt serial_service.hicmod private.pem public.pem")
        print("\n验证模块: create_hicmod.py --verify <模块文件> [公钥文件]")
        sys.exit(1)
    
    if sys.argv[1] == '--verify':
        # 验证模式
        if len(sys.argv) < 3:
            print("错误: 需要指定要验证的模块文件")
            sys.exit(1)
        
        module_path = sys.argv[2]
        public_key_path = sys.argv[3] if len(sys.argv) > 3 else None
        
        if verify_hicmod(module_path, public_key_path):
            sys.exit(0)
        else:
            sys.exit(1)
    
    # 创建模式
    obj_path = sys.argv[1]
    txt_path = sys.argv[2]
    output_path = sys.argv[3]
    private_key_path = sys.argv[4] if len(sys.argv) > 4 else None
    public_key_path = sys.argv[5] if len(sys.argv) > 5 else None
    
    if create_hicmod(obj_path, txt_path, output_path, private_key_path, public_key_path):
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()