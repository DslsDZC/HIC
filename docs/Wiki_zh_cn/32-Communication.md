<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 通信机制

## 概述

HIC 的通信模型与传统操作系统有本质区别。**IPC（进程间通信）的概念被彻底重构，甚至可以说被"精简"到了几乎不存在的程度**。这不是说 HIC 没有跨域通信，而是它用更基础的机制替代了传统意义上的 IPC，使其成为零开销、与生俱来的特性。

## HIC 通信模型

### 核心设计原则

HIC 的通信模型基于两个核心设计：

1. **能力系统**：所有跨域交互都必须通过能力验证，通信权限与资源权限统一。
2. **无锁共享内存**：服务之间通过能力授权的共享内存区域进行数据交换，使用无锁环形缓冲区，无需内核介入。

这意味着：

- **数据平面**：服务间的大批量数据交换通过共享内存直接完成，零拷贝、无内核介入。
- **控制平面**：服务间的控制信令可以通过共享内存中的标志位或轻量级消息传递，同样无需内核介入。

### 架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        HIC 通信架构                                                                                                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                                                                                                  │
│   ┌─────────────┐         能力授权                                 ┌─────────────┐                                 │
│   │   域 A                   │  ──────────────────────▶  │   域 B                   │                                 │
│   │ (发送方)                 │                                                  │ (接收方)                 │                                 │
│   └──────┬──────┘                                                  └──────┬──────┘                                 │
│                 │                                                                              │                                               │
│                 │         共享内存区域                                                         │                                               │
│                 │        ┌───────────────────────┐                    │                                               │
│                 └───▶│  无锁环形缓冲区                              │◀─────────┘                                               │
│                           │  + 数据区                                    │                                                                     │
│                           │  + 标志位区                                  │                                                                     │
│                           │  + 序列号                                    │                                                                     │
│                           └───────────────────────┘                                                                     │
│                                  ▲                                                                                                              │
│                                  │                                                                                                              │
│                ┌────────┴────────┐                                                                                            │
│                │     Core-0                       │                                                                                            │
│                │  (仅初始化时介入)                │                                                                                            │
│                └─────────────────┘                                                                                            │
│                                                                                                                                                  │
└─────────────────────────────────────────────────────────────────────────┘
```

## 通信的两个阶段

### 阶段一：初始化（内核介入）

```c
/* 1. 分配共享内存 */
cap_id_t shm_cap;
cap_handle_t shm_handle;
shmem_alloc(owner_domain, size, SHMEM_FLAG_WRITABLE, &shm_cap, &shm_handle);

/* 2. 映射到目标域（可指定衰减权限） */
cap_handle_t target_handle;
shmem_map(owner_domain, target_domain, shm_cap, 
          CAP_MEM_READ | CAP_MEM_WRITE,  /* 可以是原权限的子集 */
          &target_handle);

/* 3. 能力传递（带权限衰减） */
cap_transfer_with_attenuation(owner_domain, target_domain, 
                               shm_cap, attenuated_rights, &target_handle);
```

**关键点**：
- 内核仅在初始化阶段介入
- 能力授权后，后续访问无需再次验证
- 权限衰减确保派生权限不超过原始权限

### 阶段二：运行时（零内核介入）

```c
/* 初始化完成后，所有通信在用户态完成 */

/* 发送方：写入数据到共享内存 */
void sender_communicate(shmem_region_t *shm) {
    /* 获取写位置（无锁） */
    u32 write_idx = atomic_load(&shm->write_index);
    u32 read_idx = atomic_load(&shm->read_index);
    
    /* 检查是否有空间 */
    if ((write_idx + 1) % shm->size == read_idx) {
        return;  /* 缓冲区满 */
    }
    
    /* 写入数据（零拷贝） */
    memcpy(&shm->data[write_idx], message, message_size);
    
    /* 更新写位置（内存屏障确保顺序） */
    atomic_thread_fence(memory_order_release);
    atomic_store(&shm->write_index, (write_idx + 1) % shm->size);
    
    /* 可选：通知接收方（通过标志位） */
    atomic_store(&shm->data_ready, 1);
}

/* 接收方：从共享内存读取数据 */
void receiver_communicate(shmem_region_t *shm) {
    /* 检查是否有数据（无锁） */
    while (atomic_load(&shm->data_ready) == 0) {
        /* 等待或执行其他任务 */
        cpu_relax();
    }
    
    /* 读取数据（零拷贝） */
    u32 read_idx = atomic_load(&shm->read_index);
    process_message(&shm->data[read_idx]);
    
    /* 更新读位置 */
    atomic_thread_fence(memory_order_release);
    atomic_store(&shm->read_index, (read_idx + 1) % shm->size);
    atomic_store(&shm->data_ready, 0);
}
```

**关键点**：
- 所有数据交换在用户态完成
- 无锁设计避免内核调度
- 内存屏障确保数据一致性

## 精简版 IPC 原语

HIC 只保留两个核心原语：

### 1. 能力调用（domain_switch）

用于服务向 Core-0 请求能力操作：

```c
/* 系统调用号：SYSCALL_IPC_CALL */
hic_status_t syscall_ipc_call(ipc_call_params_t *params);
```

这本质上是一种系统调用，仅用于能力管理，而非数据通信。

### 2. 共享内存映射（shmem_map）

用于建立通信通道的初始化步骤：

```c
/* 系统调用号：SYSCALL_SHMEM_MAP */
hic_status_t shmem_map(domain_id_t from, domain_id_t to, cap_id_t cap,
                        cap_rights_t rights, cap_handle_t *out_handle);
```

## 与传统 IPC 的对比

| 特性 | 传统 IPC | HIC 通信模型 |
|------|----------|--------------|
| 数据拷贝 | 多次（用户态→内核态→用户态） | 零拷贝 |
| 内核介入 | 每次通信 | 仅初始化时 |
| 同步机制 | 内核调度 | 用户态无锁 |
| 权限检查 | 每次通信 | 一次建立，永久有效 |
| 性能 | 微秒级 | 纳秒级 |
| 复杂度 | 高（消息队列、管道、socket） | 低（共享内存 + 能力） |

## 为什么 HIC 可以做到 IPC 近乎不存在？

因为 HIC 的架构从根本上改变了通信的性质：

1. **同特权级运行**：Privileged-1 服务之间、服务与 Core-0 之间都在 Ring 0，无需特权级切换即可共享内存。

2. **物理隔离**：服务拥有独立物理内存，但可以通过能力授权共享特定区域，既保证了隔离，又实现了零拷贝通信。

3. **无锁设计**：整个系统无锁，通信通过无锁队列完成，无需内核调度介入。

4. **能力系统**：通信权限在共享内存建立时就已经确定，后续访问无需再次检查。

## 共享内存能力接口

### 分配共享内存

```c
/**
 * @brief 分配共享内存区域（机制层）
 */
hic_status_t shmem_alloc(domain_id_t owner, size_t size, u32 flags,
                          cap_id_t *out_cap, cap_handle_t *out_handle);
```

### 映射共享内存

```c
/**
 * @brief 映射共享内存到目标域（机制层）
 */
hic_status_t shmem_map(domain_id_t from, domain_id_t to, cap_id_t cap,
                        cap_rights_t rights, cap_handle_t *out_handle);
```

### 解除映射

```c
/**
 * @brief 解除共享内存映射（机制层）
 */
hic_status_t shmem_unmap(domain_id_t domain, cap_handle_t handle);
```

### 获取信息

```c
/**
 * @brief 获取共享内存信息
 */
hic_status_t shmem_get_info(cap_id_t cap, shmem_region_t *info);
```

## 最佳实践

1. **一次建立，永久使用**：初始化时建立通信通道，运行时零开销
2. **权限衰减原则**：派生权限必须是原始权限的子集
3. **无锁设计**：使用原子操作和内存屏障，避免锁竞争
4. **批量传输**：共享内存天然支持批量数据传输

## 相关文档

- [能力系统](./11-CapabilitySystem.md) - 能力系统详解
- [域切换](./08-Core0.md) - Core-0 域切换机制
- [安全机制](./13-SecurityMechanisms.md) - 安全机制

---

*最后更新: 2026-03-16*
