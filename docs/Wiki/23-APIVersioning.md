# API 版本管理

## 概述

HIK API 版本管理确保系统调用、能力和模块接口的兼容性和稳定性。通过语义化版本和向后兼容策略，支持长期演进。

## 版本号格式

### 语义化版本

```
格式: MAJOR.MINOR.PATCH

MAJOR: 主版本号 - 不兼容的API变更
MINOR: 次版本号 - 向后兼容的功能新增
PATCH: 修订号 - 向后兼容的问题修复
```

### 版本示例

```c
// 版本号结构
typedef struct version {
    u32 major;
    u32 minor;
    u32 patch;
} version_t;

// 版本比较
int compare_versions(version_t v1, version_t v2) {
    if (v1.major != v2.major) {
        return v1.major < v2.major ? -1 : 1;
    }
    if (v1.minor != v2.minor) {
        return v1.minor < v2.minor ? -1 : 1;
    }
    if (v1.patch != v2.patch) {
        return v1.patch < v2.patch ? -1 : 1;
    }
    return 0;
}
```

## 系统调用版本管理

### 系统调用表

```c
// 系统调用版本信息
typedef struct syscall_version {
    u32  syscall_num;       // 系统调用号
    version_t version;      // 引入版本
    version_t deprecated;   // 弃用版本
    bool  active;           // 是否活跃
} syscall_version_t;

// 系统调用版本表
static syscall_version_t syscall_versions[] = {
    {SYSCALL_IPC_CALL,   {1, 0, 0}, {0, 0, 0}, true},
    {SYSCALL_CAP_TRANSFER, {1, 0, 0}, {0, 0, 0}, true},
    {SYSCALL_CAP_DERIVE,   {1, 0, 0}, {0, 0, 0}, true},
    {SYSCALL_CAP_REVOKE,   {1, 0, 0}, {0, 0, 0}, true},
};
```

### 系统调用兼容性检查

```c
// 检查系统调用兼容性
bool check_syscall_compatibility(u32 syscall_num, version_t caller_version) {
    for (u32 i = 0; i < sizeof(syscall_versions)/sizeof(syscall_version_t); i++) {
        if (syscall_versions[i].syscall_num == syscall_num) {
            // 检查系统调用是否在调用者版本中引入
            return compare_versions(caller_version, syscall_versions[i].version) >= 0;
        }
    }
    return false;
}
```

## 能力版本管理

### 能力版本信息

```c
// 能力版本信息
typedef struct capability_version {
    version_t version;      // 能力版本
    version_t min_version;   // 最低兼容版本
    bool      deprecated;    // 是否弃用
} capability_version_t;

// 能力版本表
static capability_version_t cap_versions[] = {
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_MEMORY
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_MMIO
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_IRQ
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_ENDPOINT
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_CAP_DERIVE
    {{1, 0, 0}, {1, 0, 0}, false},  // CAP_TYPE_SERVICE
};
```

### 能力兼容性检查

```c
// 检查能力兼容性
bool check_capability_compatibility(cap_type_t type, version_t domain_version) {
    // 检查能力是否在域版本中引入
    if (type < sizeof(cap_versions)/sizeof(capability_version_t)) {
        return compare_versions(domain_version, cap_versions[type].version) >= 0;
    }
    return false;
}
```

## 模块API版本管理

### 模块API版本

```c
// 模块API版本
typedef struct module_api_version {
    char     name[64];          // API名称
    version_t version;         // API版本
    version_t min_version;     // 最低版本
    u32      flags;            // 标志
} module_api_version_t;

// 模块API版本表
static module_api_version_t module_apis[] = {
    {"file_open", {1, 0, 0}, {1, 0, 0}, 0},
    {"file_read", {1, 0, 0}, {1, 0, 0}, 0},
    {"file_write", {1, 0, 0}, {1, 0, 0}, 0},
};
```

### API兼容性检查

```c
// 检查模块API兼容性
bool check_module_api_compatibility(module_instance_t *module,
                                     version_t kernel_version) {
    for (u32 i = 0; i < module->api_count; i++) {
        module_api_version_t *api = &module->apis[i];
        
        // 检查API是否在内核版本中引入
        if (compare_versions(kernel_version, api->min_version) < 0) {
            return false;
        }
    }
    
    return true;
}
```

## 版本兼容性策略

### 向后兼容原则

1. **主版本变更**: 不兼容的API变更，需要更新调用者
2. **次版本变更**: 向后兼容的功能新增，调用者无需更新
3. **修订版本变更**: 向后兼容的问题修复，调用者无需更新

### 弃用策略

```c
// 弃用系统调用
void deprecate_syscall(u32 syscall_num, version_t deprecate_version) {
    for (u32 i = 0; i < sizeof(syscall_versions)/sizeof(syscall_version_t); i++) {
        if (syscall_versions[i].syscall_num == syscall_num) {
            syscall_versions[i].deprecated = deprecate_version;
            syscall_versions[i].active = false;
        }
    }
}

// 检查弃用
bool is_syscall_deprecated(u32 syscall_num, version_t current_version) {
    for (u32 i = 0; i < sizeof(syscall_versions)/sizeof(syscall_version_t); i++) {
        if (syscall_versions[i].syscall_num == syscall_num) {
            return syscall_versions[i].deprecated.version != 0 &&
                   compare_versions(current_version, syscall_versions[i].deprecated) >= 0;
        }
    }
    return false;
}
```

## 版本查询

### 获取系统版本

```c
// 获取系统版本
version_t get_system_version(void) {
    version_t version;
    version.major = HIK_VERSION_MAJOR;
    version.minor = HIK_VERSION_MINOR;
    version.patch = HIK_VERSION_PATCH;
    return version;
}

// 获取模块版本
version_t get_module_version(const char *module_name) {
    module_instance_t *module = find_module_by_name(module_name);
    if (!module) {
        version_t empty = {0, 0, 0};
        return empty;
    }
    
    version_t version;
    version.major = module->version_major;
    version.minor = module->version_minor;
    version.patch = module->version_patch;
    return version;
}
```

## 最佳实践

1. **语义化版本**: 严格遵循语义化版本规范
2. **向后兼容**: 保持向后兼容性
3. **充分测试**: 版本变更前充分测试
4. **文档更新**: 更新API文档
5. **弃用通知**: 提前通知弃用

## 相关文档

- [模块格式](./20-ModuleFormat.md) - 模块格式
- [模块管理器](./21-ModuleManager.md) - 模块管理
- [滚动更新](./22-RollingUpdate.md) - 滚动更新

---

*最后更新: 2026-02-14*