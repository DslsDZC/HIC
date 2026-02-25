<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 最佳实践

## 概述

本文档提供了 HIC 开发和使用的最佳实践建议。

## 开发最佳实践

### 代码风格

- **遵循编码规范**: 严格遵守 [编码规范](./06-CodingStandards.md)
- **一致性**: 保持代码风格一致性
- **注释**: 添加必要的注释，解释"为什么"而不是"是什么"
- **函数长度**: 保持函数简短，单个函数不超过 50 行

### 错误处理

```c
// 检查返回值
hic_status_t status = some_function();
if (status != HIC_SUCCESS) {
    log_error("Function failed: %d", status);
    return status;
}

// 检查指针
if (ptr == NULL) {
    log_error("Null pointer");
    return HIC_ERROR_INVALID;
}
```

### 内存管理

```c
// 分配后立即检查
u8 *buffer = kmalloc(size);
if (buffer == NULL) {
    return HIC_ERROR_NO_MEMORY;
}

// 使用后立即释放
kfree(buffer);
buffer = NULL;  // 防止悬空指针
```

### 并发安全（无锁设计）

HIC 采用无锁架构设计，不使用传统的锁机制。所有并发操作通过以下方式保证：

```c
// 使用原子操作保护共享数据
atomic_inc_u64(&counter);

// 禁用中断保护关键区域（单核原子性）
bool irq_state = atomic_enter_critical();
critical_section();
atomic_exit_critical(irq_state);

// 使用内存屏障确保内存访问顺序
atomic_acquire_barrier();  // 读取前
// ... 读操作 ...
atomic_release_barrier();  // 写入后

// 使用无锁环形缓冲区进行生产者-消费者通信
lockfree_ringbuffer_push(&rb, data);
void* item = lockfree_ringbuffer_pop(&rb);
```

## 性能优化

### 快速路径

```c
// 快速路径: 直接检查缓存
if (cache_hit) {
    return cached_value;  // 避免慢路径
}

// 慢路径: 完整处理
return full_computation();
```

### 批量操作

```c
// 批量处理减少开销
void process_batch(item_t *items, u32 count) {
    for (u32 i = 0; i < count; i++) {
        process_item(&items[i]);
    }
}

// 而不是逐个处理
for (u32 i = 0; i < count; i++) {
    process_item(&items[i]);  // 每次都有开销
}
```

### 避免锁竞争

```c
// 使用无锁算法
u32 atomic_increment(u32 *value) {
    return __atomic_add_fetch(value, 1, __ATOMIC_ACQ_REL);
}

// 使用per-CPU数据
per_cpu_data_t *data = get_per_cpu_data();
data->counter++;
```

## 安全最佳实践

### 能力使用

```c
// 验证能力
if (!cap_check_access(current_domain, cap, CAP_READ)) {
    return HIC_ERROR_PERMISSION;
}

// 撤销不需要的能力
cap_revoke(current_domain, temp_cap);
```

### 输入验证

```c
// 验证输入参数
if (offset >= size) {
    return HIC_ERROR_INVALID;
}

// 验证指针
if (!is_valid_user_pointer(ptr, size)) {
    return HIC_ERROR_INVALID;
}
```

### 信息泄露防护

```c
// 清除敏感数据
void clear_sensitive_data(u8 *data, size_t size) {
    volatile u8 *p = data;
    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}
```

## 测试最佳实践

### 单元测试

```c
// 测试正常情况
TEST(test_function_normal) {
    int result = function(42);
    ASSERT_EQUAL(result, 84);
}

// 测试边界情况
TEST(test_function_boundary) {
    int result = function(INT_MAX);
    ASSERT_EQUAL(result, INT_MAX * 2);
}

// 测试错误情况
TEST(test_function_error) {
    int result = function(-1);
    ASSERT_EQUAL(result, -1);
}
```

### 集成测试

```c
// 测试组件交互
TEST(test_ipc_communication) {
    domain_id_t d1 = create_domain();
    domain_id_t d2 = create_domain();
    
    cap_id_t endpoint = create_endpoint(d2);
    
    int request = 42;
    int response;
    
    hic_status_t status = ipc_call(d1, endpoint, &request, sizeof(request),
                                    &response, sizeof(response));
    
    ASSERT_EQUAL(status, HIC_SUCCESS);
    ASSERT_EQUAL(response, 84);
}
```

## 调试最佳实践

### 日志记录

```c
// 使用适当的日志级别
log_debug("Entering function");  // 调试信息
log_info("System started");      // 一般信息
log_warning("Memory low");       // 警告
log_error("Crash detected");     // 错误
```

### 断言

```c
// 使用断言检查不变式
ASSERT(domain != NULL);
ASSERT(cap != INVALID_CAP_ID);
ASSERT(size > 0);
```

## 部署最佳实践

### 滚动更新

```c
// 分阶段更新
1. 准备新版本
2. 验证新版本
3. 逐个域更新
4. 验证更新成功
5. 回滚计划
```

### 监控

```c
// 持续监控系统状态
- CPU 使用率
- 内存使用量
- 系统调用延迟
- 错误率
```

## 相关文档

- [编码规范](./06-CodingStandards.md) - 编码规范
- [测试](./07-Testing.md) - 测试
- [故障排除](./38-Troubleshooting.md) - 故障排除

---

*最后更新: 2026-02-14*