<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 模块管理器

## 概述

HIK 模块管理器负责模块的生命周期管理，包括加载、卸载、依赖解析和版本兼容性检查。模块管理器确保模块的安全性和系统的稳定性。

## 模块实例

### 模块实例结构

```c
typedef struct module_instance {
    u64                  instance_id;       // 实例ID
    char                 name[64];          // 模块名称
    hikmod_type_t        type;             // 模块类型
    u32                  version_major;     // 主版本
    u32                  version_minor;     // 次版本
    
    // 模块数据
    u8                  *module_data;      // 模块数据
    u64                  module_size;      // 模块大小
    hikmod_header_t     *header;          // 模块头部
    
    // 段基址
    u8                  *code_base;       // 代码段基址
    u8                  *data_base;       // 数据段基址
    u8                  *bss_base;        // BSS段基址
    
    // 符号表
    hikmod_symbol_t     *symbols;         // 符号表
    u32                  symbol_count;     // 符号数量
    
    // 依赖
    module_dependency_t  *dependencies;    // 依赖列表
    u32                  dep_count;        // 依赖数量
    
    // 状态
    module_state_t       state;           // 模块状态
    u32                  ref_count;        // 引用计数
    
    // 配额
    module_quota_t       quota;           // 资源配额
} module_instance_t;
```

### 模块状态

```c
typedef enum {
    MODULE_STATE_LOADED,        // 已加载
    MODULE_STATE_INITIALIZED,   // 已初始化
    MODULE_STATE_RUNNING,       // 运行中
    MODULE_STATE_SUSPENDED,     // 已暂停
    MODULE_STATE_UNLOADED,      // 已卸载
} module_state_t;
```

## 模块管理

### 全局模块表

```c
#define MAX_MODULES  256

static module_instance_t *g_module_table[MAX_MODULES] = {0};
static u32 g_module_count = 0;
```

### 加载模块

```c
// 加载模块
hik_status_t module_load(const char *path, u64 *instance_id) {
    // 查找空闲实例槽
    u64 slot = find_free_module_slot();
    if (slot == INVALID_MODULE_ID) {
        return HIK_ERROR_NO_RESOURCE;
    }
    
    // 分配模块实例
    module_instance_t *instance = allocate_module_instance();
    if (!instance) {
        return HIK_ERROR_NO_MEMORY;
    }
    
    // 加载模块文件
    hik_status_t status = load_module(path, instance);
    if (status != HIK_SUCCESS) {
        free_module_instance(instance);
        return status;
    }
    
    // 解析依赖
    status = resolve_module_dependencies(instance);
    if (status != HIK_SUCCESS) {
        unload_module(instance);
        return status;
    }
    
    // 初始化模块
    status = module_initialize(instance);
    if (status != HIK_SUCCESS) {
        unload_module(instance);
        return status;
    }
    
    // 注册模块
    g_module_table[slot] = instance;
    instance->instance_id = slot;
    g_module_count++;
    
    *instance_id = slot;
    
    // 记录审计日志
    AUDIT_LOG_MODULE_LOAD(instance->domain, instance->instance_id, true);
    
    return HIK_SUCCESS;
}
```

### 卸载模块

```c
// 卸载模块
hik_status_t module_unload(u64 instance_id) {
    module_instance_t *instance = g_module_table[instance_id];
    
    if (!instance) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    // 检查引用计数
    if (instance->ref_count > 0) {
        return HIK_ERROR_BUSY;
    }
    
    // 检查依赖
    if (has_dependents(instance)) {
        return HIK_ERROR_DEPENDENT;
    }
    
    // 清理模块
    module_cleanup(instance);
    
    // 卸载模块
    hik_status_t status = unload_module(instance);
    if (status != HIK_SUCCESS) {
        return status;
    }
    
    // 释放实例
    free_module_instance(instance);
    g_module_table[instance_id] = NULL;
    g_module_count--;
    
    // 记录审计日志
    AUDIT_LOG_MODULE_UNLOAD(instance->domain, instance->instance_id, true);
    
    return HIK_SUCCESS;
}
```

## 依赖管理

### 依赖解析

```c
// 解析模块依赖
hik_status_t resolve_module_dependencies(module_instance_t *instance) {
    for (u32 i = 0; i < instance->dep_count; i++) {
        module_dependency_t *dep = &instance->dependencies[i];
        
        // 查找依赖模块
        module_instance_t *dep_module = find_module_by_name(dep->name);
        if (!dep_module) {
            // 尝试加载依赖
            u64 dep_instance_id;
            hik_status_t status = module_load(dep->name, &dep_instance_id);
            if (status != HIK_SUCCESS) {
                return HIK_ERROR_DEPENDENCY;
            }
            dep_module = g_module_table[dep_instance_id];
        }
        
        // 检查版本兼容性
        if (!check_version_compatibility(dep_module, dep)) {
            return HIK_ERROR_VERSION_MISMATCH;
        }
        
        // 增加引用计数
        dep_module->ref_count++;
    }
    
    return HIK_SUCCESS;
}
```

### 版本兼容性检查

```c
// 检查版本兼容性
bool check_version_compatibility(module_instance_t *module,
                                   module_dependency_t *dep) {
    // 主版本必须匹配
    if (module->version_major != dep->version_major) {
        return false;
    }
    
    // 次版本必须 >= 要求版本
    if (module->version_minor < dep->version_minor) {
        return false;
    }
    
    return true;
}
```

## 模块初始化

### 初始化流程

```c
// 初始化模块
hik_status_t module_initialize(module_instance_t *instance) {
    // 查找初始化函数
    hikmod_symbol_t *init_sym = find_symbol(instance, "module_init");
    if (!init_sym) {
        return HIK_ERROR_NOT_FOUND;
    }
    
    // 调用初始化函数
    module_init_func_t init_func = (module_init_func_t)
        (instance->code_base + init_sym->offset);
    
    hik_status_t status = init_func(instance);
    if (status != HIK_SUCCESS) {
        return status;
    }
    
    instance->state = MODULE_STATE_INITIALIZED;
    
    return HIK_SUCCESS;
}
```

## 模块查询

### 查找模块

```c
// 按名称查找模块
module_instance_t* find_module_by_name(const char *name) {
    for (u32 i = 0; i < MAX_MODULES; i++) {
        module_instance_t *instance = g_module_table[i];
        if (instance && strcmp(instance->name, name) == 0) {
            return instance;
        }
    }
    return NULL;
}

// 按类型查找模块
u32 find_modules_by_type(hikmod_type_t type, module_instance_t **modules,
                           u32 max_count) {
    u32 count = 0;
    
    for (u32 i = 0; i < MAX_MODULES && count < max_count; i++) {
        module_instance_t *instance = g_module_table[i];
        if (instance && instance->type == type) {
            modules[count++] = instance;
        }
    }
    
    return count;
}
```

## 模块统计

### 获取统计信息

```c
// 获取模块统计
void module_get_stats(u64 *total, u64 *loaded, u64 *running) {
    if (total) *total = MAX_MODULES;
    if (loaded) *loaded = g_module_count;
    
    if (running) {
        u64 count = 0;
        for (u32 i = 0; i < MAX_MODULES; i++) {
            module_instance_t *instance = g_module_table[i];
            if (instance && instance->state == MODULE_STATE_RUNNING) {
                count++;
            }
        }
        *running = count;
    }
}
```

## 最佳实践

1. **版本管理**: 使用语义化版本
2. **依赖声明**: 明确声明所有依赖
3. **资源限制**: 遵守模块配额
4. **错误处理**: 正确处理所有错误
5. **测试验证**: 充分测试模块

## 相关文档

- [模块格式](./20-ModuleFormat.md) - 模块格式详解
- [滚动更新](./22-RollingUpdate.md) - 滚动更新
- [示例服务](../src/Core-0/examples/PRIVILEGED_SERVICE_GUIDE.md) - 服务示例

---

*最后更新: 2026-02-14*