<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
-->

# HIC Privileged-1服务开发指南

## 概述

Privileged-1层是HIC系统的特权服务沙箱，运行在与Core-0相同的特权级（x86 Ring 0），但通过MMU隔离，每个服务拥有独立的物理地址空间。

## 核心特性

### 1. 物理内存直接映射

每个Privileged-1服务被分配独立的物理内存区域：
- 代码段：直接在物理地址上运行
- 数据段：直接访问物理内存
- 栈空间：独立的物理栈

**优势**：
- 无虚拟内存管理开销
- 确定的内存访问延迟
- 简化的内存管理

### 2. 基于能力的资源访问

服务必须通过能力系统获得资源访问权限：
- 内存访问能力
- 设备MMIO访问能力
- I/O端口访问能力
- 端点能力（API网关）

### 3. 故障隔离

服务的崩溃被限制在其地址空间内：
- Core-0捕获异常
- 自动回收资源
- 监控服务可自动重启

## 服务类型

```c
typedef enum {
    SERVICE_TYPE_DRIVER,       /* 硬件驱动 */
    SERVICE_TYPE_FS,           /* 文件系统 */
    SERVICE_TYPE_NETWORK,      /* 网络协议栈 */
    SERVICE_TYPE_DISPLAY,      /* 显示服务 */
    SERVICE_TYPE_AUDIO,        /* 音频服务 */
    SERVICE_TYPE_CRYPTO,       /* 加密服务 */
    SERVICE_TYPE_MONITOR,      /* 监控服务 */
    SERVICE_TYPE_AUDIT,        /* 审计服务 */
    SERVICE_TYPE_CUSTOM,       /* 自定义服务 */
} service_type_t;
```

## 服务生命周期

### 1. 加载服务

```c
/* 定义服务配额 */
domain_quota_t quota = {
    .max_memory = 2 * 1024 * 1024,  /* 2MB */
    .max_threads = 4,
    .max_caps = 64,
    .cpu_quota_percent = 10,
};

/* 从模块加载服务 */
domain_id_t domain_id;
hic_status_t status = privileged_service_load(
    module_instance_id,
    "my_service",
    SERVICE_TYPE_CUSTOM,
    &quota,
    &domain_id
);
```

### 2. 注册端点

```c
/* 注册服务端点 */
cap_id_t endpoint_cap;
status = privileged_service_register_endpoint(
    domain_id,
    "my_endpoint",
    (virt_addr_t)my_handler,
    0x2000,  /* 系统调用号 */
    &endpoint_cap
);
```

### 3. 启动服务

```c
status = privileged_service_start(domain_id);
```

### 4. 停止服务

```c
status = privileged_service_stop(domain_id);
```

### 5. 重启服务

```c
status = privileged_service_restart(domain_id);
```

### 6. 卸载服务

```c
status = privileged_service_unload(domain_id);
```

## 端点处理函数

端点是服务的API入口点，其他域通过系统调用访问：

```c
/* 端点处理函数签名 */
static hic_status_t my_handler(
    service_message_t *msg,      /* 接收到的消息 */
    service_message_t *response  /* 响应消息（由调用者分配） */
) {
    /* 验证参数 */
    if (!msg || !response) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 处理消息 */
    switch (msg->type) {
        case 0x01:  /* 请求类型1 */
            /* 处理请求 */
            response->type = 0x02;  /* 响应类型 */
            response->length = /* 响应数据长度 */;
            /* 填充响应数据 */
            break;
            
        default:
            return HIC_ERROR_INVALID_PARAM;
    }
    
    return HIC_SUCCESS;
}
```

## 中断处理

服务可以注册中断处理函数：

```c
/* 注册中断处理函数 */
status = privileged_service_register_irq_handler(
    domain_id,
    32,  /* IRQ向量 */
    (virt_addr_t)my_irq_handler
);

/* 中断处理函数 */
void my_irq_handler(u32 irq_vector) {
    /* 处理中断 */
    /* ... */
}
```

## MMIO访问

服务可以映射设备MMIO区域：

```c
/* 映射MMIO区域 */
virt_addr_t mmio_addr;
status = privileged_service_map_mmio(
    domain_id,
    0xFE000000,  /* MMIO物理地址 */
    0x1000,      /* 大小4KB */
    &mmio_addr
);

/* 访问MMIO */
u32 value = *(volatile u32*)mmio_addr;
```

## 服务间通信

### 通过Core-0中转（控制流）

```c
/* 调用其他服务的端点 */
hic_status_t ipc_call(
    cap_id_t endpoint_cap,  /* 目标端点能力 */
    service_message_t *request,
    service_message_t *response
);
```

### 通过共享内存（数据流）

1. 创建共享内存区域
2. Core-0分配物理内存
3. 创建内存能力授予两个服务
4. 服务通过共享内存交换数据

## 资源配额

服务必须遵守资源配额限制：

```c
/* 检查内存配额 */
status = privileged_service_check_memory_quota(domain_id, size);
if (status != HIC_SUCCESS) {
    /* 超出配额 */
}

/* 检查CPU配额 */
status = privileged_service_check_cpu_quota(domain_id);
if (status != HIC_SUCCESS) {
    /* 超出配额 */
}
```

## 依赖管理

服务可以声明对其他服务的依赖：

```c
/* 添加依赖 */
status = privileged_service_add_dependency(
    domain_id,
    dep_domain_id  /* 依赖的服务域ID */
);

/* 检查依赖是否满足 */
bool satisfied = privileged_service_check_dependencies(domain_id);
```

## 示例服务

参见 `example_service.c`：

- 回显服务：接收消息并返回
- 性能测试服务：执行计算任务
- 端点注册：演示如何注册多个端点
- 中断处理：演示如何处理中断

## 模块格式

服务模块遵循 `.hicmod` 格式：

```
+------------------+
| Module Header    |  魔数、版本、大小等
+------------------+
| Code Segment     |  代码段
+------------------+
| Data Segment     |  数据段
+------------------+
| Signature        |  RSA-3072 + SHA-384签名
+------------------+
```

## 模块头部定义

```c
typedef struct hicmod_header {
    u32    magic;           /* 魔数：0x48494B4D ('HICM') */
    u32    version;         /* 版本 */
    u64    code_size;       /* 代码段大小 */
    u64    data_size;       /* 数据段大小 */
    u64    sig_offset;      /* 签名偏移 */
    u64    sig_size;        /* 签名大小 */
    char   service_name[64];/* 服务名称 */
    char   service_uuid[37];/* 服务UUID */
    u32    service_type;    /* 服务类型 */
} hicmod_header_t;
```

## 服务函数表

每个服务必须实现以下函数：

```c
typedef struct service_functions {
    hic_status_t (*init)(void);      /* 初始化 */
    hic_status_t (*start)(void);     /* 启动 */
    hic_status_t (*stop)(void);      /* 停止 */
    hic_status_t (*cleanup)(void);   /* 清理 */
    void (*irq_handler)(u32);        /* 中断处理 */
} service_functions_t;
```

## 构建服务

### 1. 编译服务

```bash
x86_64-elf-gcc -ffreestanding -nostdlib \
    -mno-red-zone -mcmodel=kernel -m64 \
    -I../include -c my_service.c -o my_service.o
```

### 2. 链接模块

```bash
x86_64-elf-ld -T my_service.lds -o my_service.hicmod my_service.o
```

### 3. 签名模块

```bash
# 使用构建系统自动签名
./build_system.py --target service
```

## 安全注意事项

### 1. 能力验证

所有资源访问必须通过能力验证：
- 不要直接访问未授权的内存
- 不要使用未授权的I/O端口
- 不要调用未授权的端点

### 2. 输入验证

验证所有输入参数：
- 检查指针是否为NULL
- 验证缓冲区大小
- 验证消息类型

### 3. 错误处理

正确处理所有错误情况：
- 返回适当的错误码
- 不要忽略错误
- 记录错误信息

### 4. 资源管理

正确管理资源：
- 及时释放分配的资源
- 遵守资源配额
- 不要泄漏资源

## 调试技巧

### 1. 控制台输出

```c
console_puts("[MY-SVC] Debug message\n");
console_putu(value);
console_putx(address);
```

### 2. 审计日志

```c
/* 记录审计事件 */
u64 audit_data[4] = {event_type, param1, param2, param3};
audit_log_event(AUDIT_EVENT_CUSTOM, domain_id, cap_id, thread_id,
                audit_data, 4, true);
```

### 3. 监控服务

使用监控服务获取服务状态：
```c
service_info_t *info = monitor_get_service_info(domain_id);
console_puts("Service state: ");
console_putu(info->state);
console_puts("\n");
```

## 性能优化

### 1. 避免系统调用

系统调用有开销，尽量减少：
- 批处理操作
- 使用共享内存
- 避免频繁调用

### 2. 使用零拷贝

通过共享内存实现零拷贝：
- 大数据传输
- 高频数据交换

### 3. 中断优化

- 快速处理中断
- 将耗时操作推迟到线程
- 使用无锁数据结构

## 故障恢复

### 1. 监控服务自动重启

监控服务会自动重启崩溃的服务：
- 最多重启3次
- 记录崩溃信息
- 可配置重启策略

### 2. 状态持久化

如果服务需要持久化状态：
- 在停止时保存状态
- 在启动时恢复状态
- 使用原子操作确保一致性

## 常见问题

### Q1: 服务崩溃后如何恢复？

A: 监控服务会自动重启服务。如果重启失败，需要手动检查日志。

### Q2: 如何调试服务？

A: 使用控制台输出和审计日志。监控服务提供崩溃时的栈信息。

### Q3: 服务之间如何通信？

A: 通过Core-0中转（控制流）或共享内存（数据流）。

### Q4: 如何访问硬件设备？

A: 通过MMIO映射和中断处理。需要先获得相应的设备能力。

### Q5: 服务的CPU配额如何工作？

A: Core-0会监控每个服务的CPU使用时间，超出配额时限制调度。

## 参考资料

- 三层模型文档：`TD/三层模型.md`
- 能力系统：`kernel/include/capability.h`
- 域管理：`kernel/include/domain.h`
- 监控服务：`kernel/include/monitor.h`
- 审计系统：`kernel/include/audit.h`

## 示例代码

完整的示例服务代码：`kernel/examples/example_service.c`

构建示例：
```bash
cd kernel/examples
make example_service
```