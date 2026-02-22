<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# Privileged-1 层

## 概述

Privileged-1 层是 HIC 三层特权架构的第二层，位于 Core-0 和 Application-3 之间。它为特权服务提供了一个安全的沙箱环境，允许这些服务在受控的条件下执行特权操作。

## 设计目标

- **隔离性**: 每个 Privileged-1 服务都在独立的隔离域中运行
- **最小权限**: 服务只能访问其明确需要的资源
- **可审计**: 所有操作都被审计日志记录
- **可验证**: 符合形式化验证的不变式约束

## 架构组件

### 域沙箱

每个 Privileged-1 服务运行在一个独立的域中：

```c
typedef struct domain {
    domain_id_t    domain_id;
    domain_type_t  type;        // DOMAIN_TYPE_PRIVILEGED
    domain_state_t state;
    
    // 物理内存直接映射（无虚拟地址转换）
    phys_addr_t    phys_base;
    size_t         phys_size;
    
    // 能力空间
    cap_handle_t  *cap_space;
    u32            cap_count;
    
    // 资源配额
    domain_quota_t quota;
    struct {
        size_t memory_used;
        u32    thread_used;
    } usage;
} domain_t;
```

### 典型 Privileged-1 服务

#### 1. 设备驱动服务
- **功能**: 管理硬件设备
- **能力需求**: MMIO 能力、IRQ 能力
- **配额**: 适度内存、有限线程

#### 2. 文件系统服务
- **功能**: 提供文件存储
- **能力需求**: 内存能力、存储设备能力
- **配额**: 大内存、多线程

#### 3. 网络服务
- **功能**: 网络协议栈
- **能力需求**: 网络设备能力、内存能力
- **配额**: 中等内存、中等线程

#### 4. 图形服务
- **功能**: 图形渲染
- **能力需求**: 显存能力、DMA 能力
- **配额**: 大内存、多线程

## 服务生命周期

### 创建服务

```c
// 定义服务配额
domain_quota_t service_quota = {
    .max_memory = 0x1000000,      // 16MB
    .max_threads = 32,
    .max_caps = 1024,
    .cpu_quota_percent = 10
};

// 创建服务域
domain_id_t service_domain;
hic_status_t status = domain_create(
    DOMAIN_TYPE_PRIVILEGED,
    HIC_DOMAIN_CORE,  // 父域是 Core-0
    &service_quota,
    &service_domain
);
```

### 启动服务

```c
// 分配服务代码内存
phys_addr_t code_base;
pmm_alloc_frames(service_domain, 
                 (code_size + PAGE_SIZE - 1) / PAGE_SIZE,
                 PAGE_FRAME_PRIVILEGED, 
                 &code_base);

// 加载服务代码
memcpy((void*)code_base, service_code, code_size);

// 创建服务线程
thread_id_t service_thread;
thread_create(service_domain, (void*)code_base, 0, &service_thread);
```

### 服务 IPC 接口

```c
// 注册 IPC 端点
cap_id_t endpoint_cap;
cap_create_endpoint(service_domain, service_domain, &endpoint_cap);

// 处理 IPC 请求
hic_status_t handle_ipc_request(ipc_call_params_t *params) {
    // 验证调用者权限
    if (!cap_check_access(caller_domain, params->endpoint_cap, 0)) {
        return HIC_ERROR_PERMISSION;
    }
    
    // 处理请求
    switch (params->request_type) {
        case REQ_READ:
            return handle_read_request(params);
        case REQ_WRITE:
            return handle_write_request(params);
        default:
            return HIC_ERROR_INVALID_PARAM;
    }
}
```

### 停止服务

```c
// 暂停服务
domain_suspend(service_domain);

// 销毁服务
domain_destroy(service_domain);
```

## 安全机制

### 能力访问控制

Privileged-1 服务只能通过能力访问资源：

```c
// 检查能力访问权限
hic_status_t access_resource(cap_id_t resource_cap, cap_rights_t required) {
    domain_id_t my_domain = get_current_domain();
    
    // 验证能力存在且有效
    if (!capability_exists(resource_cap)) {
        return HIC_ERROR_CAP_INVALID;
    }
    
    // 验证能力权限
    return cap_check_access(my_domain, resource_cap, required);
}
```

### 域间隔离

- 内存隔离: 每个域有独立的物理内存区域
- 能力隔离: 能力不能跨域共享（除非明确授予）
- 执行隔离: 域间切换通过 Core-0 控制

### 审计日志

所有特权操作都被记录：

```c
// 记录设备访问
AUDIT_LOG_CAP_ACCESS(domain, device_cap, ACCESS_TYPE_READ, true);

// 记录内存分配
AUDIT_LOG_PMM_ALLOC(domain, phys_addr, pages, true);

// 记录 IPC 调用
AUDIT_LOG_IPC_CALL(caller, endpoint_cap, true);
```

## 性能考虑

### 直接内存映射

Privileged-1 服务使用物理内存直接映射，避免了虚拟地址转换开销：

```
虚拟地址 → 页表查找 → 物理地址  (传统内核)
物理地址 → 直接访问               (HIC Privileged-1)
```

**优势**:
- 消除 TLB 未命中
- 减少页表遍历开销
- 简化内存管理

### 快速 IPC

使用能力系统和域切换机制：

```
传统 IPC: 上下文切换 + 拷贝数据
HIC IPC:  域切换 + 直接内存访问
```

## 开发指南

### 服务模板

```c
#include "domain.h"
#include "capability.h"
#include "syscall.h"

// 服务主函数
void service_main(void) {
    domain_id_t my_domain = get_current_domain();
    
    // 注册 IPC 端点
    cap_id_t endpoint;
    cap_create_endpoint(my_domain, my_domain, &endpoint);
    
    // 服务循环
    while (1) {
        // 等待 IPC 请求
        ipc_call_params_t params;
        hic_status_t status = syscall_ipc_wait(&params);
        
        if (status == HIC_SUCCESS) {
            // 处理请求
            handle_request(&params);
        }
    }
}

// 请求处理函数
hic_status_t handle_request(ipc_call_params_t *params) {
    // 验证请求
    if (!validate_request(params)) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    // 执行操作
    switch (params->request_type) {
        case REQ_IOCTL:
            return handle_ioctl(params);
        default:
            return HIC_ERROR_NOT_SUPPORTED;
    }
}
```

### 编译服务

```bash
# 编译服务为 .hicmod 模块
make MODULE=my_service

# 加载服务到内核
./build/tools/module_loader load my_service.hicmod
```

## 最佳实践

1. **最小权限原则**: 只请求必需的能力
2. **资源限制**: 遵守域配额限制
3. **错误处理**: 正确处理所有错误情况
4. **审计记录**: 记录所有重要操作
5. **线程安全**: 正确使用同步原语

## 相关文档

- [Core-0 层](./08-Core0.md) - 核心层文档
- [Application-3 层](./10-Application3.md) - 应用层文档
- [能力系统](./11-CapabilitySystem.md) - 能力系统详解
- [审计日志](./14-AuditLogging.md) - 审计机制

## 示例

完整的服务示例参见:
- `src/Core-0/examples/example_service.c`
- `src/Core-0/examples/PRIVILEGED_SERVICE_GUIDE.md`

---

*最后更新: 2026-02-14*