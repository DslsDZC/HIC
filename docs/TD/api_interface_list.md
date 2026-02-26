<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC API 接口列表

本文档列出了HIC系统的所有API接口，包括核心系统调用和Privileged-1服务端点。

## 版本信息

- **文档版本**: 1.0.0
- **最后更新**: 2026-02-26
- **适用内核版本**: HIC Core-0 v1.0+

---

## 接口分类原则

HIC系统将API接口分为两类：

### 1. 官方接口（Official API）

**定义**: 由HIC官方团队维护和发布的接口，属于核心系统的一部分。

**范围**:
- 所有系统调用（0-255）
- Core-0提供的核心服务端点
- Privileged-1层的官方服务（能力管理、调度、内存管理、中断控制、系统调用分发、模块管理）
- 官方扩展服务（文件系统、网络栈、块设备等）
- 官方高级服务（图形、音频、AI等）

**端点范围**: 0x0000-0x8FFF

**维护原则**:
- ✅ **只向后兼容**: 新版本永远兼容旧版本
- ✅ **不减少功能**: 永不删除或废弃现有接口
- ✅ **只增加功能**: 新版本只能添加新接口，不能修改或删除现有接口
- ✅ **稳定保证**: API签名、参数、返回值在ABI版本内永不改变
- ✅ **文档完整**: 每个接口都有完整的文档和示例

**版本演进规则**:
- **主版本**: 破坏性变更（极少发生，必须提供迁移路径）
- **次版本**: 添加新接口，保持向后兼容
- **补丁版本**: Bug修复和优化，完全兼容

### 2. 第三方接口（Third-party API）

**定义**: 由第三方开发者创建和管理的服务接口，通过模块系统加载。

**范围**:
- 用户自定义的服务
- 第三方驱动和服务
- 社区贡献的扩展服务

**端点范围**: 0x9000-0xAFFF

**维护原则**:
- 第三方接口由各自开发者维护
- 不保证向后兼容
- 版本管理由开发者自行决定
- 官方提供版本化机制支持

**注意事项**:
- 第三方接口使用0x9000-0xAFFF范围，共10,240个端点
- 第三方接口必须遵循HIC的安全和能力系统规范
- 优秀的第三方接口可以申请升级为官方接口
- 0xB000-0xFFFF保留给未来官方扩展和特殊用途

---

## 一、核心系统调用（Core-0） - 官方接口

Core-0提供的基础系统调用接口，所有应用和服务都可以使用。

### 系统调用定义

| 调用号 | 名称 | 参数 | 返回值 | 描述 |
|--------|------|------|--------|------|
| 0 | `SYSCALL_IPC_CALL` | `endpoint_cap`, `message` | `hic_status_t` | 跨域IPC调用 |
| 1 | `SYSCALL_CAP_TRANSFER` | `to_domain`, `cap_id` | `hic_status_t` | 传递能力给其他域 |
| 2 | `SYSCALL_CAP_DERIVE` | `parent_cap`, `sub_rights`, `out_cap` | `hic_status_t` | 派生能力的子集 |
| 3 | `SYSCALL_CAP_REVOKE` | `cap_id` | `hic_status_t` | 撤销能力 |
| 4 | `SYSCALL_DOMAIN_CREATE` | `config` | `domain_id` | 创建新域 |
| 5 | `SYSCALL_DOMAIN_DESTROY` | `domain_id` | `hic_status_t` | 销毁域 |
| 6 | `SYSCALL_THREAD_CREATE` | `domain_id`, `entry` | `thread_id` | 创建线程 |
| 7 | `SYSCALL_THREAD_YIELD` | 无 | `hic_status_t` | 让出CPU |
| 8 | `SYSCALL_SHMEM_ALLOC` | `size`, `flags` | `phys_addr` | 分配共享内存 |
| 9 | `SYSCALL_SHMEM_MAP` | `phys_addr`, `size` | `virt_addr` | 映射共享内存 |

### 性能目标

- **系统调用延迟**: 20-30ns（通过syscall/sysret优化）
- **上下文切换**: 无特权级切换，直接同级调用
- **参数传递**: 通过寄存器，无内存拷贝

---

## 二、Privileged-1服务端点 - 官方接口

Privileged-1层提供的服务接口，通过能力系统授权访问。以下服务均为**官方接口**，由HIC官方维护。

### 2.1 能力管理器服务（Capability Manager） - 官方接口

**服务名称**: `capability_manager`  
**服务域**: `HIC_DOMAIN_PRIVILEGED`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x1000 | `CAP_ENDPOINT_VERIFY` | 验证能力有效性 |
| 0x1001 | `CAP_ENDPOINT_REVOKE` | 撤销能力 |
| 0x1002 | `CAP_ENDPOINT_DELEGATE` | 委托能力 |
| 0x1003 | `CAP_ENDPOINT_TRANSFER` | 转移能力 |
| 0x1004 | `CAP_ENDPOINT_DERIVE` | 派生能力 |

**实现文件**: `src/Privileged-1/capability_manager_service.c`

---

### 2.2 调度器服务（Scheduler） - 官方接口  
**服务域**: `HIC_DOMAIN_CORE`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x2000 | `SCHED_ENDPOINT_CREATE` | 创建线程 |
| 0x2001 | `SCHED_ENDPOINT_TERMINATE` | 终止线程 |
| 0x2002 | `SCHED_ENDPOINT_YIELD` | 让出CPU |

**实现文件**: `src/Privileged-1/scheduler_service.c`

---

### 2.3 内存管理器服务（Memory Manager） - 官方接口  
**服务域**: `HIC_DOMAIN_PRIVILEGED`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x3000 | `MEM_ENDPOINT_ALLOC` | 分配内存 |
| 0x3001 | `MEM_ENDPOINT_FREE` | 释放内存 |
| 0x3002 | `MEM_ENDPOINT_SHARED` | 分配共享内存 |

**实现文件**: `src/Privileged-1/memory_manager_service.c`

---

### 2.4 中断控制器服务（IRQ Controller） - 官方接口  
**服务域**: `HIC_DOMAIN_PRIVILEGED`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x4000 | `IRQ_ENDPOINT_REGISTER` | 注册中断处理 |
| 0x4001 | `IRQ_ENDPOINT_UNREGISTER` | 注销中断处理 |
| 0x4002 | `IRQ_ENDPOINT_ENABLE` | 启用中断 |
| 0x4003 | `IRQ_ENDPOINT_DISABLE` | 禁用中断 |

**实现文件**: `src/Privileged-1/irq_controller_service.c`

---

### 2.5 系统调用分发器服务（Syscall Dispatcher） - 官方接口  
**服务域**: `HIC_DOMAIN_CORE`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x5000 | `SYSCALL_ENDPOINT_IPC` | IPC调用分发 |
| 0x5001 | `SYSCALL_ENDPOINT_CAP` | 能力操作分发 |
| 0x5002 | `SYSCALL_ENDPOINT_DOMAIN` | 域操作分发 |
| 0x5003 | `SYSCALL_ENDPOINT_THREAD` | 线程操作分发 |

**实现文件**: `src/Privileged-1/syscall_dispatcher_service.c`

---

### 2.6 模块管理器服务（Module Manager） - 官方接口  
**服务域**: `HIC_DOMAIN_PRIVILEGED`

| 端点ID | 名称 | 功能 |
|--------|------|------|
| 0x6000 | `MODULE_ENDPOINT_LOAD` | 加载模块 |
| 0x6001 | `MODULE_ENDPOINT_UNLOAD` | 卸载模块 |
| 0x6002 | `MODULE_ENDPOINT_QUERY` | 查询模块信息 |

**实现文件**: `src/Privileged-1/module_manager_service.c`

---

## 三、版本化API设计

### 3.1 版本化端点命名规范

为了支持API演进和向后兼容，服务端点采用版本化命名：

```
{service_name}.v{major}.{minor}.{method}
```

**示例**：
- `filesystem.v1.open` - 文件系统v1的打开方法
- `filesystem.v2.open_with_flags` - 文件系统v2的增强打开方法
- `netstack.v1.connect` - 网络栈v1的连接方法

### 3.2 版本兼容性规则

**兼容性检查**：
```c
bool is_version_compatible(u32 old_major, u32 old_minor,
                            u32 new_major, u32 new_minor)
{
    /* 主版本不同，不兼容 */
    if (old_major != new_major) {
        return false;
    }
    
    /* 次版本向下兼容 */
    return new_minor >= old_minor;
}
```

**兼容性示例**：
- `v1.0` ↔ `v1.1`: ✅ 兼容（向下兼容）
- `v1.0` ↔ `v1.0`: ✅ 兼容（相同版本）
- `v1.0` ↔ `v2.0`: ❌ 不兼容（主版本不同）

---

## 四、适配器服务

### 4.1 POSIX兼容层

**服务名称**: `posix-legacy-adapter`  
**服务域**: `HIC_DOMAIN_PRIVILEGED`  
**状态**: 计划实现

**功能**：
- 接收传统POSIX系统调用
- 转换为HIC内部服务调用
- 支持标准文件操作、进程管理等

**支持的POSIX API**：
- `open()`, `close()`, `read()`, `write()`
- `fork()`, `exec()`, `wait()`
- `socket()`, `bind()`, `listen()`, `accept()`
- `pipe()`, `mmap()`, `munmap()`

---

## 五、API访问流程

### 5.1 服务端点注册流程

```
1. 服务初始化
   ↓
2. 分配端点能力（cap_create_endpoint）
   ↓
3. 注册到服务管理器（privileged_service_register_endpoint）
   ↓
4. 添加到路由表（g_service_manager.syscall_route_table）
   ↓
5. 返回端点能力ID给客户端
```

### 5.2 客户端调用流程

```
1. 客户端持有端点能力
   ↓
2. 通过syscall发起调用（SYSCALL_IPC_CALL）
   ↓
3. Core-0验证能力权限
   ↓
4. Core-0通过路由表查找处理函数
   ↓
5. 调用Privileged-1服务的处理函数
   ↓
6. 返回结果给客户端
```

---

## 六、安全模型

### 6.1 能力验证

所有API调用必须通过能力系统验证：

```c
/* 1. 验证能力有效性 */
if (entry->cap_id != endpoint_cap || 
    (entry->flags & CAP_FLAG_REVOKED)) {
    return HIC_ERROR_CAP_INVALID;
}

/* 2. 验证权限 */
status = cap_check_access(domain, endpoint_cap, 0);
if (status != HIC_SUCCESS) {
    return HIC_ERROR_PERMISSION;
}
```

### 6.2 审计日志

所有API调用都会记录审计日志：

```c
AUDIT_LOG_SYSCALL(domain, syscall_num, result);
AUDIT_LOG_IPC_CALL(caller, cap, result);
```

---

## 七、性能指标

| 操作类型 | 目标延迟 | 实际延迟 | 优化方法 |
|---------|---------|---------|---------|
| 系统调用 | 20-30ns | ~25ns | syscall/sysret指令 |
| IPC调用 | 20-30ns | ~25ns | 同级函数调用 |
| 中断处理 | 0.5-1μs | ~0.7μs | 简化入口（7寄存器） |
| 域切换 | 120-150ns | ~130ns | 仅切换必要寄存器 |

---

## 八、未来扩展

### 8.1 官方接口扩展计划

以下服务由HIC官方开发，属于官方接口，遵循官方接口维护原则：

| 服务名称 | 端点范围 | 状态 | 维护方 |
|---------|---------|------|--------|
| 文件系统 | 0x7000-0x70FF | 计划中 | HIC官方 |
| 网络栈 | 0x7100-0x71FF | 计划中 | HIC官方 |
| 块设备 | 0x7200-0x72FF | 计划中 | HIC官方 |
| 图形服务 | 0x7300-0x73FF | 计划中 | HIC官方 |
| 音频服务 | 0x7400-0x74FF | 计划中 | HIC官方 |
| AI服务 | 0x8000-0x80FF | 计划中 | HIC官方 |
| 安全服务 | 0x8100-0x81FF | 计划中 | HIC官方 |

**官方接口保证**:
- ✅ 只向后兼容
- ✅ 不减少功能
- ✅ 只增加功能
- ✅ 完整文档支持

### 8.2 第三方接口规范

第三方开发者可以在以下范围内创建自己的服务接口：

| 端点范围 | 用途 | 规范 |
|---------|------|------|
| 0x9000-0x9FFF | 第三方基础服务（数据库、缓存、消息队列等） | 由开发者维护 |
| 0xA000-0xAFFF | 第三方高级服务（自定义服务、实验性功能等） | 由开发者维护 |

**第三方接口升级机制**:
- 优秀的第三方接口可以申请升级为官方接口
- 升级申请需要提交接口文档、测试用例和兼容性说明
- 官方团队审核通过后，接口将从第三方范围移动到官方扩展服务范围
- 原第三方接口保留一段时间用于平滑迁移

**第三方接口要求**:
- 必须遵循HIC能力系统规范
- 必须提供API文档
- 版本管理由开发者自行决定
- 不保证向后兼容

### 8.3 官方接口扩展（系统调用）

| 调用号 | 名称 | 功能 | 兼容性 |
|--------|------|------|--------|
| 10 | `SYSCALL_FSYNC` | 同步文件 | 向后兼容 |
| 11 | `SYSCALL_STAT` | 获取文件状态 | 向后兼容 |
| 12 | `SYSCALL_SOCKET` | 创建套接字 | 向后兼容 |
| 13 | `SYSCALL_BIND` | 绑定地址 | 向后兼容 |
| 14 | `SYSCALL_LISTEN` | 监听连接 | 向后兼容 |

**系统调用扩展规则**:
- ✅ 新增系统调用号只能增加
- ✅ 现有系统调用永不删除或修改
- ✅ 系统调用签名永不改变
- ✅ 返回值类型永不改变

---

## 九、参考文献

- [HIC架构文档](./README.md)
- [三层模型](./三层模型.md)
- [能力系统](../Wiki/11-CapabilitySystem.md)
- [API版本管理](../Wiki/23-APIVersioning.md)

---

## 接口分类与原则

**重要**: 关于接口分类和官方接口维护原则的详细信息，请参阅：
- [接口分类与维护原则](./interface_classification.md)

### 官方接口维护原则总结

### 核心承诺

HIC官方团队对官方接口（系统调用0-255，端点0x0000-0x8FFF）做出以下**严格承诺**：

#### 1. 向后兼容性

**保证**: 新版本永远兼容旧版本
- 所有现有代码无需修改即可在新版本上运行
- 所有现有API继续正常工作
- 二进制兼容性得到保证

#### 2. 功能稳定性

**保证**: 永不删除或减少功能
- 永不删除任何公开API
- 永不修改任何公开API的签名
- 永不改变任何公开结构体的布局

#### 3. 增量演进

**保证**: 只增加，不删除
- 新版本只能添加新API
- 新版本只能添加新功能
- 新版本只能扩展结构体（在末尾）

#### 4. 文档完整性

**保证**: 每个接口都有完整文档
- 所有公开API都有详细文档
- 所有变更都有迁移指南
- 所有版本都有更新日志

### 对开发者的价值

这些承诺确保开发者可以：

✅ **长期投资**: 编写的代码可以长期使用，无需担心API变更
✅ **安全升级**: 可以安全地升级到新版本，无需修改代码
✅ **稳定开发**: 依赖稳定的API，专注于业务逻辑
✅ **降低风险**: 减少因API变更导致的维护成本

### 第三方接口说明

第三方接口（0x9000-0xAFFF）由各自开发者维护，不提供上述保证。优秀接口可申请升级为官方接口。

---

## 附录

### A. 端点ID分配规则

| 范围 | 用途 | 类型 | 维护方 | 兼容性保证 |
|------|------|------|--------|----------|
| 0x0000-0x0FFF | Core-0系统调用 | 官方 | HIC官方 | 只向后兼容 |
| 0x1000-0x1FFF | 能力管理器 | 官方 | HIC官方 | 只向后兼容 |
| 0x2000-0x2FFF | 调度器 | 官方 | HIC官方 | 只向后兼容 |
| 0x3000-0x3FFF | 内存管理器 | 官方 | HIC官方 | 只向后兼容 |
| 0x4000-0x4FFF | 中断控制器 | 官方 | HIC官方 | 只向后兼容 |
| 0x5000-0x5FFF | 系统调用分发 | 官方 | HIC官方 | 只向后兼容 |
| 0x6000-0x6FFF | 模块管理器 | 官方 | HIC官方 | 只向后兼容 |
| 0x7000-0x7FFF | 官方扩展服务 | 官方 | HIC官方 | 只向后兼容 |
| 0x8000-0x8FFF | 官方高级服务 | 官方 | HIC官方 | 只向后兼容 |
| 0x9000-0x9FFF | 第三方基础服务 | 第三方 | 各自开发者 | 不保证 |
| 0xA000-0xAFFF | 第三方高级服务 | 第三方 | 各自开发者 | 不保证 |
| 0xB000-0xFFFF | 保留 | 官方 | HIC官方 | - |

**官方接口范围**: 0x0000-0x8FFF（共34,816个端点）
**第三方接口范围**: 0x9000-0xAFFF（共10,240个端点）
**保留范围**: 0xB000-0xFFFF（共20,480个端点）

**官方接口保证**:
- ✅ 只向后兼容
- ✅ 不减少功能
- ✅ 只增加功能
- ✅ API签名永不改变
- ✅ 完整文档支持

**第三方接口说明**:
- 由各自开发者维护
- 不保证向后兼容
- 版本管理由开发者自行决定
- 必须遵循HIC安全规范

### B. 错误代码

| 代码 | 名称 | 描述 |
|------|------|------|
| 0 | `HIC_SUCCESS` | 成功 |
| 1 | `HIC_ERROR_INVALID_PARAM` | 无效参数 |
| 2 | `HIC_ERROR_NO_MEMORY` | 内存不足 |
| 3 | `HIC_ERROR_CAP_INVALID` | 能力无效 |
| 4 | `HIC_ERROR_PERMISSION` | 权限不足 |
| 5 | `HIC_ERROR_NOT_FOUND` | 未找到 |
| 6 | `HIC_ERROR_CAP_REVOKED` | 能力已撤销 |
| 10 | `HIC_ERROR_CANCELLED` | 操作已取消 |