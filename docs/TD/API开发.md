<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC API 开发者指南

本指南为开发者提供完整的API使用说明，确保API/ABI兼容性和开发友好性。

## 接口分类和原则

HIC系统将API接口分为两类，各有不同的维护原则：

### 官方接口（Official API）

**定义**: 由HIC官方团队维护和发布的接口。

**范围**:
- 所有系统调用（0-255）
- Core-0提供的核心服务端点
- Privileged-1层的官方服务（能力管理、调度、内存管理、中断控制、系统调用分发、模块管理）
- 官方扩展服务（文件系统、网络栈、块设备等）

**维护原则**:
- ✅ **只向后兼容**: 新版本永远兼容旧版本
- ✅ **不减少功能**: 永不删除或废弃现有接口
- ✅ **只增加功能**: 新版本只能添加新接口，不能修改或删除现有接口
- ✅ **稳定保证**: API签名、参数、返回值在ABI版本内永不改变
- ✅ **文档完整**: 每个接口都有完整的文档和示例

**对开发者的意义**:
- 代码可以长期使用，无需担心API变更
- 新版本发布后，现有代码无需修改即可运行
- 可以安全地升级到新版本

### 第三方接口（Third-party API）

**定义**: 由第三方开发者创建和管理的服务接口。

**范围**:
- 用户自定义的服务
- 第三方驱动和服务
- 社区贡献的扩展服务

**维护原则**:
- 第三方接口由各自开发者维护
- 不保证向后兼容
- 版本管理由开发者自行决定
- 官方提供版本化机制支持

**对开发者的意义**:
- 需要关注接口的版本兼容性
- 建议使用版本化API（如 `service.v1.open`）
- 可以创建自己的服务接口

### 本指南的适用范围

本指南主要介绍**官方接口**的使用方法，因为：
- 官方接口有完整的向后兼容保证
- 官方接口是大多数应用的基础
- 官方接口有完整的文档和支持

第三方接口的使用请参考各自的文档。

---

## 快速开始

### 1. 包含头文件

```c
#include <hic_api.h>
```

### 2. 基础错误处理

```c
hic_error_t err = hic_domain_create(&config, &domain);
if (!hic_is_ok(err)) {
    printf("Error: %s\n", hic_strerror(err));
    return;
}
```

### 3. C++ 使用

```cpp
#include <hic_api.h>

using namespace hic;

Error err = hic_ipc_call(cap, msg, size, response, &resp_size);
if (!is_ok(err)) {
    // 处理错误
}
```

---

## 完整示例

### 示例1：创建域并运行线程

```c
#include <hic_api.h>
#include <stdio.h>

/* 线程入口函数 */
void my_thread(void* arg) {
    printf("Thread running\n");
    
    /* 让出CPU */
    hic_thread_yield();
    
    printf("Thread exiting\n");
}

int main() {
    /* 检查API版本 */
    u32 major, minor, patch;
    hic_api_get_version(&major, &minor, &patch);
    printf("HIC API Version: %u.%u.%u\n", major, minor, patch);
    
    /* 配置域 */
    hic_domain_config_t domain_config = {
        .name = "my_domain",
        .stack_size = 64 * 1024,      /* 64KB */
        .heap_size = 1024 * 1024,      /* 1MB */
        .priority = 10,
        .flags = 0,
    };
    
    /* 创建域 */
    hic_domain_t domain;
    hic_error_t err = hic_domain_create(&domain_config, &domain);
    if (!hic_is_ok(err)) {
        printf("Failed to create domain: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("Domain created: %llu\n", (unsigned long long)domain);
    
    /* 配置线程 */
    hic_thread_config_t thread_config = {
        .entry = my_thread,
        .arg = NULL,
        .stack_size = 16 * 1024,      /* 16KB */
        .priority = 10,
    };
    
    /* 创建线程 */
    hic_thread_t thread;
    err = hic_thread_create(domain, &thread_config, &thread);
    if (!hic_is_ok(err)) {
        printf("Failed to create thread: %s\n", hic_strerror(err));
        hic_domain_destroy(domain);
        return 1;
    }
    
    printf("Thread created: %llu\n", (unsigned long long)thread);
    
    /* 主线程让出CPU */
    hic_thread_yield();
    
    /* 清理 */
    hic_thread_terminate(thread);
    hic_domain_destroy(domain);
    
    return 0;
}
```

### 示例2：跨域通信（IPC）

```c
#include <hic_api.h>
#include <stdio.h>
#include <string.h>

/* 服务端 */
void service_handler(void* arg) {
    /* 等待IPC调用 */
    while (1) {
        hic_thread_yield();
    }
}

/* 客户端 */
int client_main(hic_cap_t service_cap) {
    /* 构造消息 */
    struct {
        u32 type;
        u32 id;
        char data[256];
    } message;
    
    message.type = 1;
    message.id = 100;
    strcpy(message.data, "Hello from client");
    
    /* 发送IPC调用 */
    void* response = NULL;
    u64 response_size = 0;
    
    hic_error_t err = hic_ipc_call(service_cap, &message, sizeof(message),
                                   response, &response_size);
    if (!hic_is_ok(err)) {
        printf("IPC call failed: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("IPC call successful\n");
    return 0;
}
```

### 示例3：共享内存

```c
#include <hic_api.h>
#include <stdio.h>
#include <string.h>

int main() {
    /* 分配共享内存 */
    hic_shmem_t shmem;
    hic_error_t err = hic_shmem_alloc(4096, 0, &shmem);
    if (!hic_is_ok(err)) {
        printf("Failed to allocate shared memory: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("Shared memory allocated: %llu\n", (unsigned long long)shmem);
    
    /* 映射共享内存 */
    void* addr;
    err = hic_shmem_map(shmem, 0, 4096, &addr);
    if (!hic_is_ok(err)) {
        printf("Failed to map shared memory: %s\n", hic_strerror(err));
        hic_shmem_free(shmem);
        return 1;
    }
    
    printf("Shared memory mapped at: %p\n", addr);
    
    /* 写入数据 */
    strcpy((char*)addr, "Hello, shared memory!");
    printf("Data written: %s\n", (char*)addr);
    
    /* 解除映射 */
    err = hic_shmem_unmap(addr, 4096);
    if (!hic_is_ok(err)) {
        printf("Failed to unmap: %s\n", hic_strerror(err));
    }
    
    /* 释放共享内存 */
    err = hic_shmem_free(shmem);
    if (!hic_is_ok(err)) {
        printf("Failed to free: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("Shared memory freed\n");
    return 0;
}
```

### 示例4：能力传递

```c
#include <hic_api.h>
#include <stdio.h>

int main() {
    /* 获取当前域 */
    hic_domain_t current_domain = hic_domain_get_current();
    printf("Current domain: %llu\n", (unsigned long long)current_domain);
    
    /* 创建目标域 */
    hic_domain_config_t target_config = {
        .name = "target_domain",
        .stack_size = 64 * 1024,
        .heap_size = 1024 * 1024,
        .priority = 10,
        .flags = 0,
    };
    
    hic_domain_t target_domain;
    hic_error_t err = hic_domain_create(&target_config, &target_domain);
    if (!hic_is_ok(err)) {
        printf("Failed to create target domain: %s\n", hic_strerror(err));
        return 1;
    }
    
    /* 假设我们有一个能力 */
    hic_cap_t cap = 0x1000;  /* 示例能力 */
    
    /* 传递能力给目标域 */
    hic_cap_t out_handle;
    err = hic_cap_transfer(target_domain, cap, &out_handle);
    if (!hic_is_ok(err)) {
        printf("Failed to transfer capability: %s\n", hic_strerror(err));
        hic_domain_destroy(target_domain);
        return 1;
    }
    
    printf("Capability transferred: 0x%llx\n", (unsigned long long)out_handle);
    
    /* 清理 */
    hic_domain_destroy(target_domain);
    return 0;
}
```

### 示例5：查询服务端点

```c
#include <hic_api.h>
#include <stdio.h>

int main() {
    /* 查询文件系统服务v1 */
    hic_endpoint_info_t info;
    hic_error_t err = hic_endpoint_query("filesystem", 1, 0, &info);
    
    if (!hic_is_ok(err)) {
        printf("Failed to query endpoint: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("Endpoint found:\n");
    printf("  Name: %s\n", info.name);
    printf("  API Version: %u.%u.%u\n", 
           info.api_version_major, 
           info.api_version_minor, 
           info.api_version_patch);
    printf("  Syscall Num: 0x%x\n", info.syscall_num);
    printf("  Max Message Size: %u bytes\n", info.max_message_size);
    printf("  Call Count: %llu\n", info.call_count);
    
    /* 获取端点能力 */
    hic_cap_t cap;
    err = hic_endpoint_get_cap("filesystem", 1, 0, &cap);
    if (!hic_is_ok(err)) {
        printf("Failed to get endpoint capability: %s\n", hic_strerror(err));
        return 1;
    }
    
    printf("Endpoint capability: 0x%llx\n", (unsigned long long)cap);
    
    return 0;
}
```

---

## API/ABI 兼容性保证

### 1. 调用约定

HIC API使用标准的C调用约定，确保跨编译器兼容：

- **x86-64**: System V AMD64 ABI
- **ARM64**: AAPCS64
- **RISC-V**: RISC-V calling convention

### 2. 类型稳定性

所有公开类型都有固定大小，确保ABI稳定：

```c
typedef u64 hic_domain_t;      /* 永远64位 */
typedef u64 hic_cap_t;         /* 永远64位 */
typedef u64 hic_thread_t;      /* 永远64位 */
typedef u64 hic_shmem_t;       /* 永远64位 */
```

### 3. 版本兼容性

API支持版本化查询和兼容性检查：

```c
/* 检查API版本 */
u32 req_major = 1, req_minor = 0;
u32 avail_major, avail_minor;
hic_api_get_version(&avail_major, &avail_minor, NULL);

if (hic_api_check_compatibility(req_major, req_minor, 
                                avail_major, avail_minor)) {
    /* 版本兼容，可以安全使用 */
} else {
    /* 版本不兼容，需要处理 */
}
```

### 4. 错误处理

所有函数返回统一的错误码：

```c
typedef enum {
    HIC_OK = 0,
    HIC_ERR_INVALID_PARAM = 1,
    HIC_ERR_NO_MEMORY = 2,
    /* ... */
} hic_error_t;
```

---

## 最佳实践

### 1. 错误处理

```c
/* ✅ 好的做法 */
hic_error_t err = hic_domain_create(&config, &domain);
if (!hic_is_ok(err)) {
    printf("Error: %s\n", hic_strerror(err));
    return err;
}

/* ❌ 不好的做法 */
hic_domain_create(&config, &domain);  /* 忽略错误 */
```

### 2. 资源清理

```c
/* ✅ 使用RAII风格（C++） */
{
    auto domain = hic::Domain::create(&config);
    if (!domain) return;
    
    /* 使用域 */
}  /* 自动清理 */

/* ✅ 手动清理（C） */
hic_domain_t domain;
hic_error_t err = hic_domain_create(&config, &domain);
if (!hic_is_ok(err)) return err;

/* 使用域 */

hic_domain_destroy(domain);  /* 清理 */
```

### 3. 版本检查

```c
/* ✅ 在程序启动时检查版本 */
u32 major, minor, patch;
hic_api_get_version(&major, &minor, &patch);

if (major != HIC_API_VERSION_MAJOR) {
    printf("API major version mismatch\n");
    return;
}
```

### 4. 性能监控

```c
/* 定期检查性能统计 */
hic_perf_stats_t stats;
hic_get_perf_stats(&stats);

printf("Syscall count: %llu\n", stats.syscall_count);
printf("Avg syscall time: %llu ns\n", 
       stats.syscall_total_ns / stats.syscall_count);
```

---

## 性能优化建议

### 1. 减少系统调用

```c
/* ❌ 不好的做法 */
for (int i = 0; i < 1000; i++) {
    hic_ipc_call(cap, &msg, sizeof(msg), NULL, NULL);  /* 1000次系统调用 */
}

/* ✅ 好的做法 */
/* 批量处理，减少系统调用次数 */
```

### 2. 使用共享内存

```c
/* ✅ 大数据传输使用共享内存 */
hic_shmem_t shmem;
hic_shmem_alloc(1024 * 1024, 0, &shmem);  /* 1MB */
void* addr;
hic_shmem_map(shmem, 0, 1024 * 1024, &addr);

/* 直接访问共享内存，零拷贝 */
memcpy(addr, data, size);

/* 通过IPC通知对方 */
hic_ipc_call(cap, &notification, sizeof(notification), NULL, NULL);
```

### 3. 缓存端点能力

```c
/* ✅ 缓存能力句柄 */
static hic_cap_t cached_fs_cap = HIC_CAP_INVALID;

if (cached_fs_cap == HIC_CAP_INVALID) {
    hic_endpoint_get_cap("filesystem", 1, 0, &cached_fs_cap);
}

/* 使用缓存的句柄 */
hic_ipc_call(cached_fs_cap, &msg, sizeof(msg), NULL, NULL);
```

---

## 调试技巧

### 1. 启用详细日志

```c
/* 在代码中添加调试输出 */
hic_error_t err = hic_ipc_call(cap, &msg, size, response, &resp_size);
if (!hic_is_ok(err)) {
    printf("IPC call failed:\n");
    printf("  Capability: 0x%llx\n", (unsigned long long)cap);
    printf("  Message size: %llu\n", size);
    printf("  Error: %s\n", hic_strerror(err));
}
```

### 2. 检查性能统计

```c
/* 在关键点检查性能 */
hic_perf_stats_t stats_before, stats_after;
hic_get_perf_stats(&stats_before);

/* 执行操作 */
hic_ipc_call(cap, &msg, size, response, &resp_size);

hic_get_perf_stats(&stats_after);

u64 elapsed = stats_after.syscall_total_ns - stats_before.syscall_total_ns;
printf("Operation took: %llu ns\n", elapsed);
```

### 3. 验证能力有效性

```c
/* 在使用能力前验证 */
if (cap == HIC_CAP_INVALID) {
    printf("Invalid capability\n");
    return HIC_ERR_CAP_INVALID;
}

/* 进一步验证（如果需要） */
/* hic_status_t status = cap_check_access(domain, cap, 0); */
```

---

## 常见问题

### Q1: 官方接口的稳定性如何保证？

**A**: HIC官方接口遵循严格的向后兼容原则：

- **永不删除**任何公开API
- **永不修改**任何公开API的签名
- **永远保持**二进制兼容性
- **只添加**新功能，不影响现有功能

这意味着您使用官方接口编写的代码可以长期使用，无需担心API变更。

### Q2: 如何获取服务端点能力？

```c
hic_cap_t cap;
hic_error_t err = hic_endpoint_get_cap("service_name", 1, 0, &cap);
if (!hic_is_ok(err)) {
    printf("Failed to get capability: %s\n", hic_strerror(err));
    return;
}
```

### Q2: 如何处理版本不兼容？

```c
if (!hic_api_check_compatibility(1, 0, avail_major, avail_minor)) {
    printf("API version not compatible\n");
    printf("Required: 1.0, Available: %u.%u\n", avail_major, avail_minor);
    
    /* 选项1: 降级功能 */
    /* 选项2: 提示用户升级 */
    /* 选项3: 使用适配器 */
}
```

### Q3: 如何确保线程安全？

HIC API设计为线程安全，每个域独立运行。但需要注意：

```c
/* ✅ 每个域独立，无需锁 */
hic_thread_create(domain1, &config1, &thread1);
hic_thread_create(domain2, &config2, &thread2);

/* ⚠️ 同一域内多线程需要同步 */
/* 使用共享内存或IPC进行通信 */
```

/* ⚠️ 同一域内多线程需要同步 */
/* 使用共享内存或IPC进行通信 */
```

---

## 官方接口 vs 第三方接口

**重要**: 关于接口分类和官方接口维护原则的详细信息，请参阅：
- [接口分类与维护原则](./interface_classification.md)

### 对比表

| 特性 | 官方接口 | 第三方接口 |
|------|---------|-----------|
| **维护方** | HIC官方团队 | 各自开发者 |
| **兼容性** | 向后兼容 | 不保证 |
| **稳定性** | 高（永不删除） | 中等 |
| **文档** | 完整 | 各自提供 |
| **支持** | 官方支持 | 社区支持 |
| **端点范围** | 0x0000-0x8FFF | 0x9000-0xAFFF |
| **更新频率** | 受控 | 开发者决定 |
| **版本保证** | 严格 | 开发者决定 |

### 使用建议

**推荐使用官方接口**:
- 需要长期稳定的应用
- 需要官方支持的服务
- 核心功能和性能关键路径

**可以使用第三方接口**:
- 特定需求的服务
- 社区提供的扩展功能
- 实验性或原型功能

### 混合使用示例

```c
/* 使用官方接口创建域 */
hic_domain_t domain;
hic_domain_create(&config, &domain);

/* 使用官方接口创建线程 */
hic_thread_t thread;
hic_thread_create(domain, &thread_config, &thread);

/* 使用官方接口分配共享内存 */
hic_shmem_t shmem;
hic_shmem_alloc(1024, 0, &shmem);

/* 使用第三方接口（示例：自定义驱动） */
hic_cap_t custom_driver_cap;
hic_endpoint_get_cap("custom_driver", 1, 0, &custom_driver_cap);

/* 使用官方IPC调用第三方接口 */
hic_ipc_call(custom_driver_cap, &msg, sizeof(msg), NULL, NULL);
```

### 创建第三方接口

如果您想创建自己的服务接口：

1. **选择端点范围**: 使用0x9000-0xAFFF
2. **遵循安全规范**: 通过能力系统授权
3. **提供版本化**: 使用语义化版本
4. **编写文档**: 提供完整的API文档
5. **明确兼容性**: 说明版本兼容性规则

示例：
```c
/* 注册第三方服务端点 */
hic_endpoint_info_t info = {
    .name = "my_service",
    .api_version_major = 1,
    .api_version_minor = 0,
    .api_version_patch = 0,
};

/* 端点ID: 0x7500 */
hic_endpoint_register("my_service", 0x7500, handler);
```

---

## 参考文档

- [API接口列表](./api_interface_list.md)
- [能力系统](../Wiki/11-CapabilitySystem.md)
- [三层模型](./三层模型.md)
- [性能指标](./README.md#性能指标)

---

## 更新日志

### v1.0.0 (2026-02-26)
- 初始版本
- 完整的API接口定义
- 开发者友好的封装
- ABI兼容性保证
- 完整的使用示例