#!/usr/bin/env python3
"""
创建 HIC 服务模块 (.hicmod) - 多架构版本
将多个架构的 .o 文件和 hicmod.txt 元数据打包成模块格式
支持数字签名（Ed25519）

.hicmod 文件布局:
┌─────────────────────────────────────┐
│  模块头 (hicmod_header_t)           │
│  - magic, version, uuid             │
│  - arch_count, arch_table_offset    │
│  - metadata_offset/size             │
│  - signature_offset/size            │
├─────────────────────────────────────┤
│  架构表 (hicmod_arch_section_t[])   │
│  - arch_id, flags                   │
│  - code_offset/size, data_offset    │
│  - entry_offset, reloc_offset       │
├─────────────────────────────────────┤
│  架构段1 (x86_64)                   │
│  - 代码段, 数据段, 只读数据         │
├─────────────────────────────────────┤
│  架构段2 (aarch64)                  │
│  - 代码段, 数据段, 只读数据         │
├─────────────────────────────────────┤
│  元数据段 (架构中立)                │
│  - name, version, dependencies      │
├─────────────────────────────────────┤
│  符号表 (架构中立)                  │
├─────────────────────────────────────┤
│  签名 (可选)                        │
└─────────────────────────────────────┘
"""

import struct
import sys
import os
import uuid
import hashlib
import json
import argparse

# 常量定义
HICMOD_MAGIC = 0x48494B4D  # "HICM"
HICMOD_VERSION = 2          # 版本2支持多架构

# 架构标识符
HICMOD_ARCH_X86_64    = 0x01
HICMOD_ARCH_AARCH64   = 0x02
HICMOD_ARCH_RISCV64   = 0x03
HICMOD_ARCH_ARM32     = 0x04
HICMOD_ARCH_RISCV32   = 0x05
HICMOD_ARCH_MAX       = 8

# 架构名称映射
ARCH_NAME_MAP = {
    'x86_64': HICMOD_ARCH_X86_64,
    'amd64': HICMOD_ARCH_X86_64,
    'aarch64': HICMOD_ARCH_AARCH64,
    'arm64': HICMOD_ARCH_AARCH64,
    'riscv64': HICMOD_ARCH_RISCV64,
    'arm32': HICMOD_ARCH_ARM32,
    'arm': HICMOD_ARCH_ARM32,
    'riscv32': HICMOD_ARCH_RISCV32,
}

# 结构体大小
HICMOD_HEADER_SIZE = 88   # hicmod_header_t
HICMOD_ARCH_SECTION_SIZE = 56  # hicmod_arch_section_t


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


def parse_arch_spec(arch_spec):
    """
    解析架构规格字符串
    格式: arch_name:obj_file 或 obj_file (默认当前架构)
    返回: (arch_id, obj_path)
    """
    if ':' in arch_spec:
        arch_name, obj_path = arch_spec.split(':', 1)
        arch_id = ARCH_NAME_MAP.get(arch_name.lower())
        if arch_id is None:
            raise ValueError(f"未知架构: {arch_name}")
        return arch_id, obj_path
    else:
        # 默认 x86_64
        return HICMOD_ARCH_X86_64, arch_spec


def build_arch_section(arch_id, obj_data, offset_base):
    """
    构建架构段数据
    
    返回: (arch_section_bytes, total_size)
    """
    # 架构段头
    section = bytearray(HICMOD_ARCH_SECTION_SIZE)
    
    struct.pack_into('<I', section, 0, arch_id)           # arch_id
    struct.pack_into('<I', section, 4, 0)                  # flags
    struct.pack_into('<I', section, 8, offset_base)        # code_offset
    struct.pack_into('<I', section, 12, len(obj_data))     # code_size
    struct.pack_into('<I', section, 16, 0)                 # data_offset
    struct.pack_into('<I', section, 20, 0)                 # data_size
    struct.pack_into('<I', section, 24, 0)                 # bss_size
    struct.pack_into('<I', section, 28, 0)                 # rodata_offset
    struct.pack_into('<I', section, 32, 0)                 # rodata_size
    struct.pack_into('<I', section, 36, 0)                 # entry_offset
    struct.pack_into('<I', section, 40, 0)                 # reloc_offset
    struct.pack_into('<I', section, 44, 0)                 # reloc_count
    
    return bytes(section), len(obj_data)


def create_hicmod_multiarch(arch_specs, txt_path, output_path, 
                            private_key_path=None, public_key_path=None):
    """
    创建多架构 .hicmod 模块文件
    
    参数:
        arch_specs: 架构规格列表，格式 ["x86_64:obj1.o", "aarch64:obj2.o"]
        txt_path: 元数据文件路径
        output_path: 输出文件路径
        private_key_path: 私钥路径（可选）
        public_key_path: 公钥路径（可选）
    """
    
    # 检查输入文件
    if not os.path.exists(txt_path):
        print(f"错误: {txt_path} 不存在")
        return False
    
    # 解析元数据
    metadata = parse_hicmod_txt(txt_path)
    
    # 获取服务基本信息
    service_info = metadata.get('service', {})
    name = service_info.get('name', 'unknown')
    version_str = service_info.get('version', '1.0.0')
    api_version_str = service_info.get('api_version', '1.0')
    uuid_str = service_info.get('uuid', str(uuid.uuid4()))
    
    # 解析版本号
    version_parts = [int(x) for x in version_str.split('.')]
    while len(version_parts) < 3:
        version_parts.append(0)
    semantic_version = (version_parts[0] << 16) | (version_parts[1] << 8) | version_parts[2]
    
    # 解析 API 版本
    api_version_parts = [int(x) for x in api_version_str.split('.')]
    while len(api_version_parts) < 3:
        api_version_parts.append(0)
    api_version = (api_version_parts[0] << 16) | (api_version_parts[1] << 8) | api_version_parts[2]
    
    # 构建 UUID 字节数组
    try:
        uuid_obj = uuid.UUID(uuid_str)
        uuid_bytes = uuid_obj.bytes
    except:
        uuid_bytes = hashlib.md5(name.encode()).digest()
    
    # 读取所有架构的对象文件
    arch_data_list = []
    for spec in arch_specs:
        arch_id, obj_path = parse_arch_spec(spec)
        
        if not os.path.exists(obj_path):
            print(f"错误: {obj_path} 不存在")
            return False
        
        with open(obj_path, 'rb') as f:
            obj_data = f.read()
        
        arch_data_list.append((arch_id, obj_data, obj_path))
    
    if not arch_data_list:
        print("错误: 没有指定任何架构对象文件")
        return False
    
    # 计算偏移量
    arch_count = len(arch_data_list)
    arch_table_offset = HICMOD_HEADER_SIZE
    
    # 架构段数据起始位置
    arch_data_offset = arch_table_offset + arch_count * HICMOD_ARCH_SECTION_SIZE
    
    # 构建架构段表和段数据
    arch_sections = bytearray()
    arch_binary_data = bytearray()
    current_offset = arch_data_offset
    
    # 兼容性字段（单一架构时使用）
    legacy_code_offset = 0
    legacy_code_size = 0
    legacy_data_offset = 0
    legacy_data_size = 0
    
    for i, (arch_id, obj_data, obj_path) in enumerate(arch_data_list):
        # 构建架构段头
        section = bytearray(HICMOD_ARCH_SECTION_SIZE)
        struct.pack_into('<I', section, 0, arch_id)           # arch_id
        struct.pack_into('<I', section, 4, 0)                  # flags
        struct.pack_into('<I', section, 8, current_offset)     # code_offset
        struct.pack_into('<I', section, 12, len(obj_data))     # code_size
        struct.pack_into('<I', section, 16, 0)                 # data_offset
        struct.pack_into('<I', section, 20, 0)                 # data_size
        struct.pack_into('<I', section, 24, 0)                 # bss_size
        struct.pack_into('<I', section, 28, 0)                 # rodata_offset
        struct.pack_into('<I', section, 32, 0)                 # rodata_size
        struct.pack_into('<I', section, 36, 0)                 # entry_offset
        struct.pack_into('<I', section, 40, 0)                 # reloc_offset
        struct.pack_into('<I', section, 44, 0)                 # reloc_count
        
        arch_sections.extend(section)
        arch_binary_data.extend(obj_data)
        
        # 兼容性字段：第一个架构
        if i == 0:
            legacy_code_offset = current_offset
            legacy_code_size = len(obj_data)
        
        current_offset += len(obj_data)
    
    # 元数据段位置
    metadata_offset = current_offset
    
    # 读取元数据文本
    with open(txt_path, 'r', encoding='utf-8') as f:
        txt_data = f.read().encode('utf-8')
    metadata_size = len(txt_data)
    
    # 符号表位置（暂时为空）
    symbol_offset = metadata_offset + metadata_size
    symbol_size = 0
    
    # 签名位置
    signature_offset = symbol_offset + symbol_size
    
    # 构建模块头
    header = bytearray(HICMOD_HEADER_SIZE)
    struct.pack_into('<I', header, 0, HICMOD_MAGIC)           # magic
    struct.pack_into('<I', header, 4, HICMOD_VERSION)         # version
    header[8:24] = uuid_bytes                                  # uuid
    struct.pack_into('<I', header, 24, semantic_version)      # semantic_version
    struct.pack_into('<I', header, 28, api_version)           # api_version
    struct.pack_into('<I', header, 32, HICMOD_HEADER_SIZE)    # header_size
    
    # 多架构支持
    struct.pack_into('<I', header, 36, arch_count)            # arch_count
    struct.pack_into('<I', header, 40, arch_table_offset)     # arch_table_offset
    struct.pack_into('<I', header, 44, arch_count * HICMOD_ARCH_SECTION_SIZE)  # arch_table_size
    
    # 元数据
    struct.pack_into('<I', header, 48, metadata_offset)       # metadata_offset
    struct.pack_into('<I', header, 52, metadata_size)         # metadata_size
    
    # 符号表
    struct.pack_into('<I', header, 56, symbol_offset)         # symbol_offset
    struct.pack_into('<I', header, 60, symbol_size)           # symbol_size
    
    # 签名（将在之后设置）
    struct.pack_into('<I', header, 64, 0)                     # signature_offset
    struct.pack_into('<I', header, 68, 0)                     # signature_size
    
    # 兼容性字段
    struct.pack_into('<I', header, 72, legacy_code_offset)    # legacy_code_offset
    struct.pack_into('<I', header, 76, legacy_code_size)      # legacy_code_size
    struct.pack_into('<I', header, 80, legacy_data_offset)    # legacy_data_offset
    struct.pack_into('<I', header, 84, legacy_data_size)      # legacy_data_size
    
    # 构建模块体（不含签名）
    module_body = bytes(header) + bytes(arch_sections) + bytes(arch_binary_data) + txt_data
    
    # 尝试签名
    signature = create_signature(module_body, private_key_path)
    signature_size = len(signature) if signature else 0
    
    # 更新签名信息
    if signature:
        struct.pack_into('<I', header, 64, signature_offset)
        struct.pack_into('<I', header, 68, signature_size)
        module_body = bytes(header) + bytes(arch_sections) + bytes(arch_binary_data) + txt_data
    
    # 构建完整模块
    module_data = module_body
    if signature:
        module_data += signature
    
    # 写入输出文件
    with open(output_path, 'wb') as f:
        f.write(module_data)
    
    # 输出信息
    print(f"成功创建多架构模块: {output_path}")
    print(f"  模块名称: {name}")
    print(f"  模块版本: {version_str}")
    print(f"  UUID: {uuid_str}")
    print(f"  架构数量: {arch_count}")
    for arch_id, obj_data, obj_path in arch_data_list:
        arch_name = {v: k for k, v in ARCH_NAME_MAP.items()}.get(arch_id, f"未知({arch_id})")
        print(f"    - {arch_name}: {len(obj_data)} 字节 ({os.path.basename(obj_path)})")
    print(f"  总大小: {len(module_data)} 字节")
    if signature:
        print(f"  状态: 已签名 (Ed25519, {signature_size} 字节)")
    else:
        print(f"  状态: 未签名")
    
    return True


def create_hicmod_single(obj_path, txt_path, output_path,
                         private_key_path=None, public_key_path=None):
    """
    创建单架构 .hicmod 模块文件（兼容模式）
    """
    return create_hicmod_multiarch([obj_path], txt_path, output_path,
                                   private_key_path, public_key_path)


def verify_hicmod(module_path, public_key_path=None):
    """验证 .hicmod 模块文件"""
    if not os.path.exists(module_path):
        print(f"错误: {module_path} 不存在")
        return False
    
    with open(module_path, 'rb') as f:
        module_data = f.read()
    
    # 解析头部
    if len(module_data) < HICMOD_HEADER_SIZE:
        print("错误: 模块数据过小")
        return False
    
    magic = struct.unpack_from('<I', module_data, 0)[0]
    if magic != HICMOD_MAGIC:
        print("错误: 无效的模块魔数")
        return False
    
    version = struct.unpack_from('<I', module_data, 4)[0]
    if version < 1 or version > HICMOD_VERSION:
        print(f"错误: 不支持的模块版本 {version}")
        return False
    
    header_size = struct.unpack_from('<I', module_data, 32)[0]
    arch_count = struct.unpack_from('<I', module_data, 36)[0]
    arch_table_offset = struct.unpack_from('<I', module_data, 40)[0]
    signature_offset = struct.unpack_from('<I', module_data, 64)[0]
    signature_size = struct.unpack_from('<I', module_data, 68)[0]
    
    print(f"模块信息:")
    print(f"  版本: {version}")
    print(f"  架构数量: {arch_count}")
    
    # 解析架构表
    arch_names = {v: k for k, v in ARCH_NAME_MAP.items()}
    for i in range(arch_count):
        arch_offset = arch_table_offset + i * HICMOD_ARCH_SECTION_SIZE
        arch_id = struct.unpack_from('<I', module_data, arch_offset)[0]
        code_size = struct.unpack_from('<I', module_data, arch_offset + 12)[0]
        arch_name = arch_names.get(arch_id, f"未知({arch_id})")
        print(f"  架构 {i+1}: {arch_name}, 代码大小: {code_size} 字节")
    
    # 验证签名
    if signature_offset > 0 and signature_size > 0:
        if signature_offset + signature_size > len(module_data):
            print("错误: 签名数据超出文件范围")
            return False
        
        signed_data = module_data[:signature_offset]
        signature = module_data[signature_offset:signature_offset + signature_size]
        
        if public_key_path and os.path.exists(public_key_path):
            if verify_signature(signed_data, signature, public_key_path):
                print("签名验证: 有效")
            else:
                print("签名验证: 失败")
                return False
        else:
            print("签名状态: 已签名（未验证，缺少公钥）")
    else:
        print("签名状态: 未签名")
    
    print("验证成功: 模块格式正确")
    return True


def main():
    parser = argparse.ArgumentParser(
        description='创建/验证 HIC 多架构模块 (.hicmod)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例:
  # 单架构模式（兼容）
  %(prog)s service.o hicmod.txt service.hicmod
  
  # 多架构模式
  %(prog)s -a x86_64:service_x64.o -a aarch64:service_arm64.o hicmod.txt service.hicmod
  
  # 带签名
  %(prog)s -a x86_64:service.o hicmod.txt service.hicmod -k private.pem
  
  # 验证模块
  %(prog)s --verify service.hicmod -p public.pem
'''
    )
    
    parser.add_argument('obj_file', nargs='?', help='对象文件（单架构模式）')
    parser.add_argument('txt_file', nargs='?', help='元数据文件')
    parser.add_argument('output', nargs='?', help='输出文件')
    
    parser.add_argument('-a', '--arch', action='append', dest='archs',
                       metavar='ARCH:OBJ', help='架构规格（可多次指定）')
    parser.add_argument('-k', '--private-key', help='私钥文件路径')
    parser.add_argument('-p', '--public-key', help='公钥文件路径')
    parser.add_argument('--verify', metavar='MODULE', help='验证模块文件')
    
    args = parser.parse_args()
    
    # 验证模式
    if args.verify:
        if verify_hicmod(args.verify, args.public_key):
            sys.exit(0)
        else:
            sys.exit(1)
    
    # 创建模式
    # 确定架构列表
    if args.archs:
        # 多架构模式
        arch_specs = args.archs
    elif args.obj_file:
        # 单架构兼容模式
        arch_specs = [args.obj_file]
    else:
        parser.print_help()
        sys.exit(1)
    
    if not args.txt_file or not args.output:
        print("错误: 需要指定元数据文件和输出文件")
        sys.exit(1)
    
    if create_hicmod_multiarch(arch_specs, args.txt_file, args.output,
                               args.private_key, args.public_key):
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
