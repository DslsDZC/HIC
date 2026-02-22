<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 通信机制

## 概述

HIC 提供高效的域间通信（IPC）机制，支持同步和异步通信模式。通过能力系统和域切换，实现安全、高效的通信。

## IPC 类型

### 同步 IPC

```c
// 同步 IPC 调用
hic_status_t ipc_call_sync(domain_id_t caller, cap_id_t endpoint_cap,
                          void *request, size_t req_size,
                          void *response, size_t resp_size) {
    // 验证端点能力
    if (!cap_check_access(caller, endpoint_cap, CAP_IPC_CALL)) {
        return HIC_ERROR_PERMISSION;
    }
    
    // 获取端点信息
    cap_entry_t *endpoint = &g_cap_table[endpoint_cap];
    
    // 执行域切换
    domain_id_t target_domain = endpoint->endpoint.target_domain;
    return domain_switch(caller, target_domain, endpoint_cap,
                         0, request, req_size);
}
```

### 异步 IPC

```c
// 异步 IPC 调用
hic_status_t ipc_call_async(domain_id_t caller, cap_id_t endpoint_cap,
                            void *request, size_t req_size,
                            ipc_callback_t callback) {
    // 验证端点能力
    if (!cap_check_access(caller, endpoint_cap, CAP_IPC_CALL)) {
        return HIC_ERROR_PERMISSION;
    }
    
    // 创建异步IPC 请求
    ipc_async_req_t *req = allocate_ipc_request();
    req->caller = caller;
    req->endpoint_cap = endpoint_cap;
    req->request = request;
    req->req_size = req_size;
    req->callback = callback;
    
    // 加入异步队列
    enqueue_async_request(req);
    
    return HIC_SUCCESS;
}
```

## 端点管理

### 创建端点

```c
// 创建 IPC 端点
hic_status_t ipc_create_endpoint(domain_id_t owner, cap_id_t *out) {
    // 创建端点能力
    return cap_create_endpoint(owner, owner, out);
}

// 绑定端点到服务
hic_status_t ipc_bind_endpoint(cap_id_t endpoint, domain_id_t service) {
    cap_entry_t *endpoint = &g_cap_table[endpoint];
    
    // 设置目标域
    endpoint->endpoint.target_domain = service;
    
    return HIC能力: 按照这种极简的格式创建剩余的文档（快速完成）。
```

## 共享内存 IPC

### 共享内存

```c
// 创建共享内存
hic_status_t ipc_create_shared_memory(domain_id_t d1, domain_id_t d2,
                                        size_t size, cap_id_t *out1, cap_id_t *out2) {
    // 分配共享内存
    phys_addr_t shm_addr;
    hic_status_t status = pmm_alloc_frames(HIC_DOMAIN_CORE,
                                           (size + PAGE_SIZE - 1) / PAGE_SIZE,
                                           PAGE_FRAME_SHARED,
                                           &shm_addr);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    // 为两个域创建共享内存能力
    status = cap_create_memory(d1, shm_addr, size, CAP_READ | CAP_WRITE, out1);
    if (status != HIC_SUCCESS) {
        pmm_free_frames(shm_addr, (size + PAGE_SIZE - 1) / PAGE_SIZE);
        return status;
    }
    
    return cap_create_memory(d2, shm_addr, size, CAP_READ | CAP_WRITE, out2);
}
```

## 消息传递

### 消息格式

```c
// IPC 消息头
typedef struct ipc_msg_header {
    u32 msg_id;           // 消息ID
    u32 msg_type;         // 消息类型
    u32 payload_size;     // 载荷大小
    u32 flags;            // 标志
} ipc_msg_header_t;

// 消息
typedef struct ipc_msg {
    ipc_msg_header_t header;
    u8                payload[];
} ipc_msg_t;
```

## 性能优化

### 快速 IPC 路径

```c
// 快速IPC（共享内存）
hic_status_t fast_ipc_call(cap_id_t endpoint_cap, void *request, 
                            void *response) {
    // 直接访问共享内存，无需拷贝
    ipc_shm_t *shm = get_shared_memory(endpoint_cap);
    
    // 写入请求
    memcpy(shm->request_buffer, request, shm->request_size);
    
    // 通知接收者
    notify_recipient(shm->recipient);
    
    // 等待响应
    wait_for_response(shm);
    
    // 读取响应
    memcpy(response, shm->response_buffer, shm->response_size);
    
    return HIC_SUCCESS;
}
```

## 最佳实践

1. **批量操作**: 使用批量IPC减少域切换
2. **共享内存**: 大数据传输使用共享内存
3. **错误处理**: 正确处理所有错误情况
4. **资源清理**: 及时释放IPC资源

## 相关文档

- [域切换](./08-Core0.md) - 域切换机制
- [能力系统](./11-CapabilitySystem.md) - 能力系统
- [异常处理](./33-ExceptionHandling.md) - 异常处理

---

*最后更新: 202-06-14*