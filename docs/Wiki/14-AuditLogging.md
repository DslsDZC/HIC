<!--
SPDX-FileCopyrightText: 2026 * <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 审计日志

## 概述

HIC 审计日志系统提供完整的系统操作记录，用于安全审计、故障诊断和合规性验证。系统记录 30 种不同类型的审计事件，确保所有关键操作都可追溯。

## 设计目标

- **完整性**: 记录所有关键系统操作
- **不可篡改**: 审计日志无法被修改
- **实时性**: 实时记录和监控
- **高效性**: 最小化性能影响

## 审计事件类型

### 能力事件

```c
typedef enum {
    // 能力创建
    AUDIT_EVENT_CAP_CREATE,
    
    // 能力转移
    AUDIT_EVENT_CAP_TRANSFER,
    
    // 能力撤销
    AUDIT_EVENT_CAP_REVOKE,
    
    // 能力验证
    AUDIT_EVENT_CAP_VERIFY,
    
    // 能力派生
    AUDIT_EVENT_CAP_DERIVE,
} audit_event_type_t;
```

### 域事件

```c
typedef enum {
    // 域创建
    AUDIT_EVENT_DOMAIN_CREATE,
    
    // 域销毁
    AUDIT_EVENT_DOMAIN_DESTROY,
    
    // 域暂停
    AUDIT_EVENT_DOMAIN_SUSPEND,
    
    // 域恢复
    AUDIT_EVENT_DOMAIN_RESUME,
} audit_event_type_t;
```

### 线程事件

```c
typedef enum {
    // 线程创建
    AUDIT_EVENT_THREAD_CREATE,
    
    // 线程销毁
    AUDIT_EVENT_THREAD_DESTROY,
    
    // 线程切换
    AUDIT_EVENT_THREAD_SWITCH,
} audit_event_type_t;
```

### 系统调用事件

```c
typedef enum {
    // 系统调用
    AUDIT_EVENT_SYSCALL,
    
    // IPC 调用
    AUDIT_EVENT_IPC_CALL,
} audit_event_type_t;
```

### 硬件事件

```c
typedef enum {
    // 中断
    AUDIT_EVENT_IRQ,
    
    // 异常
    AUDIT_EVENT_EXCEPTION,
} audit_event_type_t;
```

### 内存事件

```c
typedef enum {
    // 内存分配
    AUDIT_EVENT_PMM_ALLOC,
    
    // 内存释放
    AUDIT_EVENT_PMM_FREE,
    
    // 页表映射
    AUDIT_EVENT_PAGETABLE_MAP,
    
    // 页表取消映射
    AUDIT_EVENT_PAGETABLE_UNMAP,
} audit_event_type_t;
```

### 安全事件

```c
typedef enum {
    // 安全违规
    AUDIT_EVENT_SECURITY_VIOLATION,
    
    // 服务启动
    AUDIT_EVENT_SERVICE_START,
    
    // 服务停止
    AUDIT_EVENT_SERVICE_STOP,
    
    // 服务崩溃
    AUDIT_EVENT_SERVICE_CRASH,
} audit_event_type_t;
```

### 模块事件

```c
typedef enum {
    // 模块加载
    AUDIT_EVENT_MODULE_LOAD,
    
    // 模块卸载
    AUDIT_EVENT_MODULE_UNLOAD,
} audit_event_type_t;
```

## 审计日志结构

### 审计条目

```c
typedef struct audit_entry {
    u64                 timestamp;    // 时间戳（纳秒）
    audit_event_type_t  type;         // 事件类型
    domain_id_t         domain;       // 域ID
    cap_id_t            cap;          // 能力ID
    thread_id_t         thread;       // 线程ID
    bool                result;       // 结果
    u64                 data[4];      // 事件数据
} audit_entry_t;
```

### 审计缓冲区

```c
#define AUDIT_BUFFER_SIZE  (1024 * 1024)  // 1MB 审计缓冲区

typedef struct audit_buffer {
    audit_entry_t *entries;              // 审计条目数组
    u64            head;                  // 写入位置
    u64            tail;                  // 读取位置
    u64            count;                 // 条目数量
    u64            dropped;               // 丢弃的条目数
} audit_buffer_t;
```

## 审计日志 API

### 记录事件

```c
// 记录审计事件
void audit_log_event(audit_event_type_t type, domain_id_t domain,
                     cap_id_t cap, thread_id_t thread,
                     u64 *data, u32 data_len, bool result) {
    audit_entry_t entry = {
        .timestamp = get_system_time_ns(),
        .type = type,
        .domain = domain,
        .cap = cap,
        .thread = thread,
        .result = result
    };
    
    // 复制数据
    if (data && data_len > 0) {
        u32 copy_len = data_len > 4 ? 4 : data_len;
        for (u32 i = 0; i < copy_len; i++) {
            entry.data[i] = data[i];
        }
    }
    
    // 写入缓冲区
    write_audit_entry(&entry);
}
```

### 便捷宏

```c
// 能力验证
#define AUDIT_LOG_CAP_VERIFY(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_VERIFY, domain, cap, 0, NULL, 0, result)

// 能力创建
#define AUDIT_LOG_CAP_CREATE(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_CREATE, domain, cap, 0, NULL, 0, result)

// 能力转移
#define AUDIT_LOG_CAP_TRANSFER(from, to, cap, result) \
    do { \
        u64 data[2] = {from, to}; \
        audit_log_event(AUDIT_EVENT_CAP_TRANSFER, from, cap, 0, data, 2, result); \
    } while (0)

// 能力撤销
#define AUDIT_LOG_CAP_REVOKE(domain, cap, result) \
    audit_log_event(AUDIT_EVENT_CAP_REVOKE, domain, cap, 0, NULL, 0, result)

// 域创建
#define AUDIT_LOG_DOMAIN_CREATE(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_CREATE, domain, 0, 0, NULL, 0, result)

// 域销毁
#define AUDIT_LOG_DOMAIN_DESTROY(domain, result) \
    audit_log_event(AUDIT_EVENT_DOMAIN_DESTROY, domain, 0, 0, NULL, 0, result)

// 系统调用
#define AUDIT_LOG_SYSCALL(domain, syscall_num, result) \
    do { \
        u64 data[1] = {syscall_num}; \
        audit_log_event(AUDIT_EVENT_SYSCALL, domain, 0, 0, data, 1, result); \
    } while (0)

// IPC 调用
#define AUDIT_LOG_IPC_CALL(caller, cap, result) \
    audit_log_event(AUDIT_EVENT_IPC_CALL, caller, cap, 0, NULL, 0, result)

// 中断
#define AUDIT_LOG_IRQ(vector, domain, result) \
    do { \
        u64 data[1] = {vector}; \
        audit_log_event(AUDIT_EVENT_IRQ, domain, 0, 0, data, 1, result); \
    } while (0)

// 异常
#define AUDIT_LOG_EXCEPTION(domain, exc_type, result) \
    do { \
        u64 data[1] = {exc_type}; \
        audit_log_event(AUDIT_EVENT_EXCEPTION, domain, 0, 0, data, 1, result); \
    } while (0)

// 安全违规
#define AUDIT_LOG_SECURITY_VIOLATION(domain, reason) \
    do { \
        u64 data[1] = {reason}; \
        audit_log_event(AUDIT_EVENT_SECURITY_VIOLATION, domain, 0, 0, data, 1, false); \
    } while (0)
```

## 审计日志管理

### 初始化

```c
// 初始化审计系统
void audit_system_init(u64 buffer_size) {
    // 分配审计缓冲区
    g_audit_buffer.entries = (audit_entry_t *)pmm_alloc_frames(
        HIC_DOMAIN_CORE,
        (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE,
        PAGE_FRAME_CORE,
        NULL
    );
    
    if (!g_audit_buffer.entries) {
        console_puts("[AUDIT] Failed to allocate audit buffer\n");
        return;
    }
    
    // 初始化缓冲区
    g_audit_buffer.head = 0;
    g_audit_buffer.tail = 0;
    g_audit_buffer.count = 0;
    g_audit_buffer.dropped = 0;
    
    console_puts("[AUDIT] Audit system initialized\n");
}
```

### 写入条目

```c
// 写入审计条目
static void write_audit_entry(audit_entry_t *entry) {
    // 检查缓冲区是否已满
    if (g_audit_buffer.count >= AUDIT_MAX_ENTRIES) {
        g_audit_buffer.dropped++;
        return;
    }
    
    // 写入条目
    g_audit_buffer.entries[g_audit_buffer.head] = *entry;
    g_audit_buffer.head = (g_audit_buffer.head + 1) % AUDIT_MAX_ENTRIES;
    g_audit_buffer.count++;
}
```

### 读取条目

```c
// 读取审计条目
bool read_audit_entry(audit_entry_t *entry) {
    if (g_audit_buffer.count == 0) {
        return false;
    }
    
    *entry = g_audit_buffer.entries[g_audit_buffer.tail];
    g_audit_buffer.tail = (g_audit_buffer.tail + 1) % AUDIT_MAX_ENTRIES;
    g_audit_buffer.count--;
    
    return true;
}
```

## 安全监控

### 安全事件检测

```c
// 检测安全事件
void monitor_security_events(void) {
    audit_entry_t entry;
    
    // 遍历审计日志
    while (read_audit_entry(&entry)) {
        switch (entry.type) {
        case AUDIT_EVENT_SECURITY_VIOLATION:
            handle_security_violation(&entry);
            break;
            
        case AUDIT_EVENT_CAP_VERIFY:
            if (!entry.result) {
                handle_cap_verify_failure(&entry);
            }
            break;
            
        case AUDIT_EVENT_DOMAIN_DESTROY:
            log_domain_termination(&entry);
            break;
            
        case AUDIT_EVENT_SERVICE_CRASH:
            handle_service_crash(&entry);
            break;
            
        default:
            break;
        }
    }
}
```

### 威胁检测

```c
// 检测威胁
bool detect_threat(audit_entry_t *entry) {
    // 检测多次能力验证失败
    static u64 cap_verify_failures[MAX_DOMAINS] = {0};
    
    if (entry->type == AUDIT_EVENT_CAP_VERIFY && !entry->result) {
        cap_verify_failures[entry->domain]++;
        
        if (cap_verify_failures[entry->domain] > 10) {
            // 可能的暴力破解攻击
            console_puts("[AUDIT] Possible brute force attack detected\n");
            return true;
        }
    }
    
    // 检测异常的系统调用模式
    if (entry->type == AUDIT_EVENT_SYSCALL) {
        // 分析系统调用频率
        // ...
    }
    
    return false;
}
```

## 审计日志导出

### 导出为文本

```c
// 导出审计日志为文本
void export_audit_log_text(void) {
    audit_entry_t entry;
    u64 index = 0;
    
    console_puts("\n=== Audit Log ===\n");
    
    while (read_audit_entry(&entry)) {
        console_printf("%llu: Type=%u, Domain=%u, Result=%s\n",
                      entry.timestamp, entry.type, entry.domain,
                      entry.result ? "Success" : "Failure");
        index++;
    }
    
    console_printf("Total entries: %llu\n", index);
    console_printf("Dropped entries: %llu\n", g_audit_buffer.dropped);
}
```

### 导出为二进制

```c
// 导出审计日志为二进制
void export_audit_log_binary(const char *filename) {
    // 实现导出到文件
    // ...
}
```

## 性能考虑

### 缓冲区管理

- 使用环形缓冲区，避免内存拷贝
- 预分配固定大小的缓冲区
- 批量写入减少开销

### 异步记录

```c
// 异步写入审计条目
void async_write_audit_entry(audit_entry_t *entry) {
    // 将条目加入队列
    enqueue_audit_entry(entry);
    
    // 触发异步写入
    if (queue_size() > THRESHOLD) {
        flush_audit_queue();
    }
}
```

### 最小化开销

- 使用快速时间戳获取
- 避免字符串格式化
- 批量处理多个条目

## 最佳实践

1. **全面记录**: 记录所有关键操作
2. **及时处理**: 定期处理审计日志
3. **监控告警**: 设置安全事件告警
4. **定期备份**: 定期备份审计日志
5. **分析模式**: 分析审计日志模式

## 相关文档

- [安全架构](./13-SecurityArchitecture.md) - 安全架构
- [监控服务](./34-MonitorService.md) - 监控服务
- [形式化验证](./15-FormalVerification.md) - 形式化验证

---

*最后更新: 2026-02-14*