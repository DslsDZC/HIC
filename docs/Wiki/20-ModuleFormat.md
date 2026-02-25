<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 模块格式

## 概述

HIC 模块格式（.hicmod）定义了可加载内核模块的二进制格式。模块格式支持签名验证、版本管理和依赖解析，确保模块的安全性和兼容性。

## 模块文件结构

### 二进制布局

```
+------------------+
|   模块魔数      |  8 bytes: "HICMOD\0"
+------------------+
|   版本信息      |  4 bytes: 主版本.次版本
+------------------+
|   模块类型      |  4 bytes: 类型ID
+------------------+
|   模块大小      |  8 bytes: 总大小
+------------------+
|   代码段大小    |  8 bytes
+------------------+
|   数据段大小    |  8 bytes
+------------------+
|   BSS段大小     |  8 bytes
+------------------+
|   符号表大小    |  8 bytes
+------------------+
|   重定位表大小  |  8 bytes
+------------------+
|   依赖表大小    |  8 bytes
+------------------+
|   签名偏移      |  8 bytes
+------------------+
|   签名大小      |  8 bytes
+------------------+
|   保留字段      |  32 bytes
+------------------+
|   代码段        |  variable
+------------------+
|   数据段        |  variable
+------------------+
|   符号表        |  variable
+------------------+
|   重定位表      |  variable
+------------------+
|   依赖表        |  variable
+------------------+
|   签名          |  variable
+------------------+
```

### 模块头部

```c
typedef struct hicmod_header {
    char     magic[8];           // "HICMOD\0"
    u32      version;            // 主版本.次版本
    u32      type;               // 模块类型
    u64      total_size;         // 总大小
    u64      code_size;          // 代码段大小
    u64      data_size;          // 数据段大小
    u64      bss_size;           // BSS段大小
    u64      symbol_table_size;   // 符号表大小
    u64      reloc_table_size;    // 重定位表大小
    u64      dep_table_size;      // 依赖表大小
    u64      sig_offset;          // 签名偏移
    u64      sig_size;            // 签名大小
    u8       reserved[32];       // 保留字段
} hicmod_header_t;
```

## 模块类型

```c
typedef enum {
    HICMOD_TYPE_DRIVER,        // 设备驱动
    HICMOD_TYPE_FILESYSTEM,    // 文件系统
    HICMOD_TYPE_NETWORK,       // 网络协议
    HICMOD_TYPE_CRYPTO,        // 加密服务
    HICMOD_TYPE_MONITOR,       // 监控服务
    HICMOD_TYPE_CUSTOM,        // 自定义模块
} hicmod_type_t;
```

## 符号表

### 符号表项

```c
typedef struct hicmod_symbol {
    char     name[64];          // 符号名称
    u64      offset;            // 偏移（相对于段基址）
    u64      size;              // 大小
    u8       section;           // 段索引（0=代码, 1=数据, 2=BSS）
    u8       binding;           // 绑定类型
    u8       type;              // 符号类型
} hicmod_symbol_t;
```

### 符号类型

```c
typedef enum {
    HICMOD_SYM_FUNC,           // 函数
    HICMOD_SYM_DATA,           // 数据
    HICMOD_SYM_BSS,            // BSS
    HICMOD_SYM_IMPORT,         // 导入符号
    HICMOD_SYM_EXPORT,         // 导出符号
} hicmod_symbol_type_t;
```

## 重定位表

### 重定位类型

```c
typedef enum {
    HICMOD_RELOC_ABSOLUTE,      // 绝对地址
    HICMOD_RELOC_RELATIVE,      // 相对地址
    HICMOD_RELOC_GOT,          // 全局偏移表
    HICMOD_RELOC_PLT,          // 过程链接表
} hicmod_reloc_type_t;
```

### 重定位项

```c
typedef struct hicmod_reloc {
    u64      offset;            // 重定位偏移
    u64      symbol_index;      // 符号索引
    u8       type;              // 重定位类型
    u8       addend;            // 加数
} hicmod_reloc_t;
```

## 依赖表

### 依赖项

```c
typedef struct hicmod_dependency {
    char     name[64];          // 依赖名称
    u32      version_major;      // 主版本要求
    u32      version_minor;      // 次版本要求
    u32      flags;             // 标志
} hicmod_dependency_t;
```

## 签名

### 签名格式

```c
typedef struct hicmod_signature {
    u8       public_key[32];     // Ed25519 公钥
    u8       signature[64];      // Ed25519 签名
    u64      timestamp;         // 签名时间戳
} hicmod_signature_t;
```

### 签名验证

```c
// 验证模块签名
bool verify_module_signature(u8 *module_data, u64 module_size,
                              hicmod_signature_t *sig) {
    // 计算模块哈希（不包括签名部分）
    u64 signed_size = sig->timestamp;
    u8 hash[48];
    sha384(module_data, signed_size, hash);
    
    // 验证 Ed25519 签名
    ed25519_public_key_t *pubkey = (ed25519_public_key_t *)sig->public_key;
    return ed25519_verify(pubkey, hash, 48, sig->signature, 64);
}
```

## 模块加载

### 加载流程

```c
// 加载模块
hic_status_t load_module(const char *path, module_instance_t *instance) {
    // 1. 读取模块文件
    u8 *module_data = read_module_file(path, &instance->module_size);
    
    // 2. 解析模块头部
    hicmod_header_t *header = (hicmod_header_t *)module_data;
    
    // 3. 验证魔数
    if (memcmp(header->magic, "HICMOD\0", 8) != 0) {
        return HIC_ERROR_INVALID_FORMAT;
    }
    
    // 4. 验证签名
    hicmod_signature_t *sig = (hicmod_signature_t *)
        (module_data + header->sig_offset);
    if (!verify_module_signature(module_data, instance->module_size, sig)) {
        return HIC_ERROR_SIGNATURE;
    }
    
    // 5. 解析依赖
    parse_dependencies(header, module_data, instance);
    
    // 6. 解析符号表
    parse_symbols(header, module_data, instance);
    
    // 7. 解析重定位表
    parse_relocations(header, module_data, instance);
    
    // 8. 分配内存
    instance->code_base = allocate_memory(header->code_size);
    instance->data_base = allocate_memory(header->data_size);
    instance->bss_base = allocate_memory(header->bss_size);
    
    // 9. 加载段
    memcpy(instance->code_base, module_data + sizeof(hicmod_header_t),
           header->code_size);
    memcpy(instance->data_base, 
           module_data + sizeof(hicmod_header_t) + header->code_size,
           header->data_size);
    
    // 10. 执行重定位
    apply_relocations(instance);
    
    // 11. 解析依赖
    resolve_dependencies(instance);
    
    return HIC_SUCCESS;
}
```

## 模块卸载

### 卸载流程

```c
// 卸载模块
hic_status_t unload_module(module_instance_t *instance) {
    // 1. 检查模块是否在使用
    if (instance->ref_count > 0) {
        return HIC_ERROR_BUSY;
    }
    
    // 2. 释放符号
    unregister_symbols(instance);
    
    // 3. 释放内存
    free_memory(instance->code_base, instance->header->code_size);
    free_memory(instance->data_base, instance->header->data_size);
    free_memory(instance->bss_base, instance->header->bss_size);
    
    // 4. 释放模块数据
    free(instance->module_data);
    
    return HIC_SUCCESS;
}
```

## 模块创建

### 创建工具

```bash
# 编译模块
hic-modcc -o mymodule.hicmod mymodule.c

# 签名模块
hic-modsign --private-key module.priv --public-key module.pub \
               --output mymodule.hicmod mymodule.hicmod

# 验证模块
hic-modverify --public-key module.pub mymodule.hicmod
```

## 最佳实践

1. **模块化设计**: 保持模块小而专注
2. **版本管理**: 使用语义化版本
3. **依赖管理**: 明确声明依赖
4. **签名验证**: 始终使用签名
5. **测试验证**: 充分测试模块

## 相关文档

- [模块管理器](./21-ModuleManager.md) - 模块管理
- [滚动更新](./22-RollingUpdate.md) - 滚动更新
- [API版本管理](./23-APIVersioning.md) - 版本管理

---

*最后更新: 2026-02-14*