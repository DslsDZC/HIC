<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 滚动更新

## 概述

HIC 滚动更新机制允许在不中断服务的情况下更新模块和内核组件。通过模块化和版本管理，实现零停机时间的服务更新。

## 设计目标

- **零停机**: 更新过程中服务不中断
- **原子性**: 更新要么完全成功，要么完全失败
- **可回滚**: 失败时可以快速回滚到旧版本
- **兼容性**: 保持向后兼容

## 滚动更新流程

### 1. 准备阶段

```c
// 准备滚动更新
hic_status_t prepare_rolling_update(const char *module_name,
                                    const char *new_version) {
    // 1. 检查当前模块状态
    module_instance_t *current = find_module_by_name(module_name);
    if (!current) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    // 2. 检查依赖关系
    if (has_active_dependents(current)) {
        return HIC_ERROR_DEPENDENT;
    }
    
    // 3. 备份当前模块状态
    backup_module_state(current);
    
    // 4. 验证新版本
    hic_status_t status = verify_new_version(module_name, new_version);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    return HIC_SUCCESS;
}
```

### 2. 加载新版本

```c
// 加载新版本模块
hic_status_t load_new_version(const char *module_name,
                               const char *new_version,
                               u64 *new_instance_id) {
    // 构造新版本路径
    char new_path[256];
    snprintf(new_path, sizeof(new_path), "/modules/%s-%s.hicmod",
             module_name, new_version);
    
    // 加载新版本
    return module_load(new_path, new_instance_id);
}
```

### 3. 迁移状态

```c
// 迁移模块状态
hic_status_t migrate_module_state(u64 old_instance_id,
                                    u64 new_instance_id) {
    module_instance_t *old_inst = g_module_table[old_instance_id];
    module_instance_t *new_inst = g_module_table[new_instance_id];
    
    // 1. 迁移配置
    migrate_configuration(old_inst, new_inst);
    
    // 2. 迁移连接
    migrate_connections(old_inst, new_inst);
    
    // 3. 迁移状态数据
    migrate_state_data(old_inst, new_inst);
    
    return HIC_SUCCESS;
}
```

### 4. 切换版本

```c
// 切换到新版本
hic_status_t switch_to_new_version(const char *module_name,
                                   u64 new_instance_id) {
    // 1. 暂停模块
    module_instance_t *current = find_module_by_name(module_name);
    module_suspend(current->instance_id);
    
    // 2. 切换符号表
    switch_symbol_table(module_name, new_instance_id);
    
    // 3. 激活新版本
    module_instance_t *new_inst = g_module_table[new_instance_id];
    new_inst->state = MODULE_STATE_RUNNING;
    
    // 4. 清理旧版本
    module_unload(current->instance_id);
    
    return HIC_SUCCESS;
}
```

### 5. 回滚机制

```c
// 回滚到旧版本
hic_status_t rollback_to_old_version(const char *module_name) {
    // 1. 从备份恢复旧版本
    module_state_t *backup = get_module_backup(module_name);
    if (!backup) {
        return HIC_ERROR_NOT_FOUND;
    }
    
    // 2. 重新加载旧版本
    u64 old_instance_id;
    hic_status_t status = module_load(backup->module_path, &old_instance_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    // 3. 恢复状态
    restore_module_state(old_instance_id, backup);
    
    // 4. 切换到旧版本
    return switch_to_new_version(module_name, old_instance_id);
}
```

## 版本管理

### 版本兼容性矩阵

```c
// 版本兼容性检查
typedef struct version_compatibility {
    u32 old_major;
    u32 old_minor;
    u32 new_major;
    u32 new_minor;
    bool  compatible;
} version_compatibility_t;

// 版本兼容性规则
bool is_version_compatible(u32 old_major, u32 old_minor,
                            u32 new_major, u32 new_minor) {
    // 主版本不同，不兼容
    if (old_major != new_major) {
        return false;
    }
    
    // 次版本向下兼容
    return new_minor >= old_minor;
}
```

## 更新策略

### 蓝绿部署

```c
// 蓝绿部署更新
hic_status_t blue_green_update(const char *module_name,
                                const char *new_version) {
    // 1. 加载新版本到备用实例
    u64 new_instance_id;
    hic_status_t status = load_new_version(module_name, new_version, &new_instance_id);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    // 2. 测试新版本
    status = test_new_version(new_instance_id);
    if (status != HIC_SUCCESS) {
        module_unload(new_instance_id);
        return status;
    }
    
    // 3. 切换流量
    status = switch_traffic(module_name, new_instance_id);
    if (status != HIC_SUCCESS) {
        // 回滚
        rollback_to_old_version(module_name);
        return status;
    }
    
    return HIC_SUCCESS;
}
```

### 金丝雀发布

```c
// 金丝雀发布
hic_status_t canary_update(const char *module_name,
                             const char *new_version) {
    // 1. 选择金丝雀节点
    u32 canary_node = select_canary_node();
    
    // 2. 在金丝雀节点更新
    hic_status_t status = update_node(module_name, new_version, canary_node);
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    // 3. 监控金丝雀节点
    if (monitor_canary(canary_node, 60)) {  // 监控60秒
        // 金丝雀正常，继续更新其他节点
        return update_all_nodes(module_name, new_version);
    } else {
        // 金丝雀异常，停止更新
        return HIC_ERROR_CANARY_FAILED;
    }
}
```

## 最佳实践

1. **充分测试**: 更新前充分测试新版本
2. **备份状态**: 始终备份当前状态
3. **监控状态**: 实时监控更新过程
4. **快速回滚**: 准备快速回滚机制
5. **渐进发布**: 逐步发布到所有节点

## 相关文档

- [模块格式](./20-ModuleFormat.md) - 模块格式
- [模块管理器](./21-ModuleManager.md) - 模块管理
- [API版本管理](./23-APIVersioning.md) - 版本管理

---

*最后更新: 2026-02-14*