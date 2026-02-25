<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 故障排除

## 概述

本文档提供了 HIC 系统常见问题的诊断和解决方案。

## 启动问题

### 系统无法启动

**症状**: 系统启动时停止响应

**诊断步骤**:
1. 检查引导加载程序是否正确加载
2. 检查内核签名是否验证通过
3. 检查硬件配置是否支持
4. 查看串口输出日志

**解决方案**:
- 确保引导加载程序正确编译
- 验证内核签名使用正确的密钥
- 检查 CPU 是否支持必要特性
- 启用调试输出查看详细信息

### 内核签名验证失败

**症状**: "Signature verification failed" 错误

**诊断步骤**:
1. 检查签名文件是否存在
2. 检查公钥是否正确
3. 检查内核文件是否完整

**解决方案**:
```bash
# 重新签名内核
./scripts/sign_kernel.sh kernel.bin private_key.pem

# 验证签名
./scripts/verify_kernel.sh kernel.bin public_key.pem
```

## 运行时问题

### 系统调用失败

**症状**: 系统调用返回错误

**诊断步骤**:
1. 检查能力是否有效
2. 检查参数是否正确
3. 检查权限是否足够

**解决方案**:
```c
// 检查能力有效性
if (!cap_is_valid(cap)) {
    log_error("Invalid capability");
    return HIC_ERROR_INVALID;
}

// 检查权限
if (!cap_check_access(current_domain, cap, CAP_READ)) {
    log_error("Permission denied");
    return HIC_ERROR_PERMISSION;
}
```

### 内存分配失败

**症状**: 内存分配返回 NULL

**诊断步骤**:
1. 检查内存是否充足
2. 检查配额是否足够
3. 检查碎片是否严重

**解决方案**:
```c
// 检查内存状态
memory_stats_t stats;
pmm_get_stats(&stats);

if (stats.free_memory < required_size) {
    log_error("Insufficient memory");
    return HIC_ERROR_NO_MEMORY;
}

// 使用更大的页
pmm_alloc_frames(domain, pages, PAGE_FRAME_HUGE, &addr);
```

### 死锁

**症状**: 系统停止响应

**诊断步骤**:
1. 检查锁的获取顺序
2. 检查循环等待
3. 检查资源竞争

**解决方案**:
```c
// 按固定顺序获取锁
spinlock_lock(&lock_a);
spinlock_lock(&lock_b);
// ...
spinlock_unlock(&lock_b);
spinlock_unlock(&lock_a);

// 使用超时
if (!spinlock_trylock_timeout(&lock, 1000)) {
    log_error("Lock timeout");
    return HIC_ERROR_TIMEOUT;
}
```

## 性能问题

### 系统调用延迟高

**症状**: 系统调用延迟超过 30ns

**诊断步骤**:
1. 测量系统调用延迟
2. 检查快速路径是否工作
3. 检查缓存命中率

**解决方案**:
```c
// 启用快速路径
#define USE_FAST_PATH 1

// 优化热点代码
// 减少分支
// 使用内联函数
// 优化缓存访问
```

### 上下文切换延迟高

**症状**: 上下文切换延迟超过 150ns

**诊断步骤**:
1. 测量上下文切换延迟
2. 检查保存/恢复寄存器
3. 检查 TLB 刷新

**解决方案**:
```c
// 最小化保存的寄存器
// 使用 lazy FPU 切换
// 优化页表切换
```

## 安全问题

### 能力泄露

**症状**: 能力未正确撤销

**诊断步骤**:
1. 检查能力引用计数
2. 检查能力撤销逻辑
3. 检查能力派生链

**解决方案**:
```c
// 确保撤销所有能力
cap_revoke_all(domain);

// 使用 RAII 模式
typedef struct cap_guard {
    cap_id_t cap;
} cap_guard_t;

void cap_guard_cleanup(cap_guard_t *guard) {
    cap_revoke(current_domain, guard->cap);
}
```

### 信息泄露

**症状**: 敏感信息泄露

**诊断步骤**:
1. 检查日志输出
2. 检查内存泄露
3. 检查错误消息

**解决方案**:
```c
// 清除敏感数据
clear_sensitive_data(secret_data, size);

// 使用安全的错误消息
log_error("Access denied");  // 而不是 "Access denied for user X"
```

## 调试技巧

### 启用调试输出

```c
// 在构建配置中启用调试
#define HIC_DEBUG 1

// 使用调试日志
log_debug("Function called with parameter %d", param);
```

### 使用性能分析

```c
// 测量函数执行时间
u64 start = get_system_time_ns();
function();
u64 end = get_system_time_ns();

log_debug("Function took %llu ns", end - start);
```

### 使用断言

```c
// 检查不变式
ASSERT(count >= 0);
ASSERT(ptr != NULL);
ASSERT(size > 0);
```

## 获取帮助

如果以上解决方案都无法解决问题，请：

1. 收集完整的日志信息
2. 提供重现步骤
3. 提供系统配置信息
4. 在问题跟踪器中提交问题

## 相关文档

- [日志系统](./14-AuditLogging.md) - 日志系统
- [监控服务](./34-MonitorService.md) - 监控服务
- [性能指标](./17-PerformanceMetrics.md) - 性能指标

---

*最后更新: 2026-02-14*