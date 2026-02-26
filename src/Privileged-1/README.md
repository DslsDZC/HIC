# HIC Privileged-1 服务层

## 概述

Privileged-1层是HIC三级架构中的第二层，负责提供模块化的特权服务。每个服务运行在独立的隔离域中，通过能力系统进行资源访问控制。

## 目录结构

```
src/Privileged-1/
├── include/                    # 公共头文件
│   ├── service_api.h          # 服务API定义
│   ├── service_types.h        # 服务类型定义
│   └── module_format.h        # 模块格式定义
├── services/                   # 服务实现目录
│   ├── password_manager_service/    # 密码管理服务
│   ├── crypto_service/             # 密码学服务
│   ├── module_manager_service/      # 模块管理服务
│   └── config_service/             # 配置管理服务
└── Makefile                    # 主编译文件
```

## 服务目录规范

每个服务目录必须包含以下文件：

```
services/<service_name>/
├── hicmod.txt       # 服务元数据（INI格式）
├── service.h        # 服务头文件
├── service.c        # 服务实现
└── Makefile        # 编译脚本
```

## hicmod.txt 格式

每个服务的元数据文件必须包含以下部分：

```ini
[service]
name = 服务名称
display_name = 显示名称
version = 版本号
api_version = API版本
uuid = 唯一标识符

[author]
name = 作者名称
email = 作者邮箱
license = 许可证

[description]
short = 简短描述
long = 详细描述

[dependencies]
# 服务依赖

[resources]
# 资源需求

[endpoints]
# 服务端点

[permissions]
# 所需权限

[security]
critical = 是否关键服务
privileged = 是否特权服务
signature_required = 是否需要签名验证

[build]
static = 是否静态编译
priority = 启动优先级
autostart = 是否自动启动
```

## 核心服务

### 1. password_manager_service (优先级: 1)

**功能**: 提供安全的密码管理和验证功能

**端点**:
- `verify_password` - 验证密码
- `set_password` - 设置密码
- `change_password` - 修改密码
- `check_password_strength` - 检查密码强度

**用途**: 为模块加载/卸载等高级操作提供密码验证

### 2. crypto_service (优先级: 2)

**功能**: 提供密码学操作服务

**端点**:
- `sha384_hash` - SHA-384哈希计算
- `rsa_verify_pss` - RSA PSS签名验证
- `rsa_verify_v1_5` - RSA v1.5签名验证
- `mgf1` - MGF1掩码生成

**用途**: 为模块签名验证提供密码学支持

### 3. module_manager_service (优先级: 3)

**功能**: 负责模块的动态加载、卸载和管理

**端点**:
- `module_load` - 加载模块
- `module_unload` - 卸载模块
- `module_start` - 启动模块
- `module_stop` - 停止模块
- `module_list` - 列出模块
- `module_info` - 获取模块信息

**特点**:
- 所有操作需要密码验证
- 支持依赖解析
- 支持签名验证
- 支持资源配额管理

### 4. config_service (优先级: 4)

**功能**: 提供YAML配置解析和管理功能

**端点**:
- `yaml_parse` - 解析YAML配置
- `yaml_get_string` - 获取字符串值
- `yaml_get_u64` - 获取u64值
- `yaml_get_bool` - 获取布尔值
- `get_default_password` - 获取默认密码

**用途**: 读取和管理系统配置，包括默认密码

## 编译和构建

### 编译所有服务

```bash
cd src/Privileged-1
make
```

### 编译单个服务

```bash
cd src/Privileged-1
make password_manager_service.o
```

### 清理构建产物

```bash
cd src/Privileged-1
make clean
```

### 查看帮助

```bash
cd src/Privileged-1
make help
```

## 服务API规范

每个服务必须实现以下标准API：

```c
typedef hic_status_t (*service_init_t)(void);
typedef hic_status_t (*service_start_t)(void);
typedef hic_status_t (*service_stop_t)(void);
typedef hic_status_t (*service_cleanup_t)(void);
typedef hic_status_t (*service_get_info_t)(char* buffer, u32 buffer_size);

typedef struct service_api {
    service_init_t init;
    service_start_t start;
    service_stop_t stop;
    service_cleanup_t cleanup;
    service_get_info_t get_info;
} service_api_t;

/* 导出符号 */
const service_api_t g_service_api = {
    .init = my_service_init,
    .start = my_service_start,
    .stop = my_service_stop,
    .cleanup = my_service_cleanup,
    .get_info = my_service_get_info,
};
```

## 密码验证机制

模块加载和卸载需要密码验证：

```c
// 加载模块
module_load("module.hicmod", "admin123", &result);

// 卸载模块
module_unload(instance_id, "admin123");
```

默认密码存储在YAML配置文件中：

```yaml
security:
  password:
    default: "admin123"
    required: true
    min_length: 8
    require_uppercase: true
    require_lowercase: true
    require_digit: true
```

## 安全特性

1. **密码保护**: 高级操作需要密码验证
2. **签名验证**: 所有模块必须通过签名验证
3. **依赖管理**: 自动解析和验证依赖关系
4. **资源配额**: 每个服务有硬性资源限制
5. **隔离执行**: 每个服务运行在独立域中

## 扩展服务

除了核心服务，还可以创建扩展服务，例如：

- `performance_monitor_service` - 性能监控
- `hardware_probe_service` - 硬件探测
- `system_monitor_service` - 系统监控
- `power_management_service` - 电源管理
- `tpm_service` - TPM支持
- `api_gateway_service` - API网关

扩展服务可以动态加载和卸载。

## 故障排查

### 编译错误

如果编译失败，请检查：
1. Core-0头文件路径是否正确
2. 编译器版本是否支持所需特性
3. 依赖的库是否已安装

### 运行时错误

如果服务运行时出错：
1. 检查密码是否正确
2. 检查模块签名是否有效
3. 检查依赖是否满足
4. 检查资源配额是否足够

## 相关文档

- [Core-0层](../Core-0/README.md) - 内核核心层
- [Application-3层](../Application-3/README.md) - 应用层
- [能力系统](../../docs/Wiki/11-CapabilitySystem.md)
- [模块格式](../../docs/Wiki/20-ModuleFormat.md)
- [模块管理器](../../docs/Wiki/21-ModuleManager.md)

---

**最后更新**: 2026-02-26