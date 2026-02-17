<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# Application-3 层

## 概述

Application-3 层是 HIC 三层特权架构的最外层，为用户应用程序提供运行环境。这一层运行在最低特权级别（Ring 3），通过能力系统和系统调用与内核通信。

## 设计目标

- **安全性**: 应用程序被严格隔离，无法直接访问硬件
- **灵活性**: 支持多种编程语言和运行时
- **性能**: 最小化系统调用开销
- **兼容性**: 提供类 POSIX 的 API

## 架构组件

### 应用域

每个应用程序运行在独立的应用域中：

```c
typedef struct domain {
    domain_id_t    domain_id;
    domain_type_t  type;        // DOMAIN_TYPE_APPLICATION
    domain_state_t state;
    
    // 物理内存（受限）
    phys_addr_t    phys_base;
    size_t         phys_size;
    
    // 能力空间（受限）
    cap_handle_t  *cap_space;
    u32            cap_count;
    
    // 资源配额（严格限制）
    domain_quota_t quota;
} domain_t;
```

### 典型应用类型

#### 1. 命令行应用
- **功能**: 文本处理、工具程序
- **资源需求**: 低内存、单线程
- **能力需求**: 标准 I/O、文件访问

#### 2. GUI 应用
- **功能**: 图形用户界面
- **资源需求**: 中等内存、多线程
- **能力需求**: 显示能力、输入能力

#### 3. 服务器应用
- **功能**: 网络服务
- **资源需求**: 高内存、多线程
- **能力需求**: 网络能力、存储能力

## 系统调用接口

### 基本系统调用

```c
// 文件操作
hic_status_t sys_open(const char *path, int flags, cap_id_t *out);
hic_status_t sys_close(cap_id_t fd);
ssize_t sys_read(cap_id_t fd, void *buf, size_t count);
ssize_t sys_write(cap_id_t fd, const void *buf, size_t count);

// 内存操作
void *sys_mmap(size_t size, cap_rights_t prot);
hic_status_t sys_munmap(void *addr, size_t size);

// 线程操作
hic_status_t sys_thread_create(void (*func)(void *), void *arg, thread_id_t *out);
hic_status_t sys_thread_join(thread_id_t thread);
void sys_thread_exit(int status);

// IPC 操作
hic_status_t sys_ipc_call(cap_id_t endpoint, void *req, size_t req_size, 
                           void *resp, size_t resp_size);
```

### 能力相关系统调用

```c
// 能力操作
hic_status_t sys_cap_transfer(domain_id_t to, cap_id_t cap);
hic_status_t sys_cap_derive(cap_id_t parent, cap_rights_t sub_rights, cap_id_t *out);
hic_status_t sys_cap_revoke(cap_id_t cap);

// 端点操作
hic_status_t sys_endpoint_create(cap_id_t *out);
hic_status_t sys_endpoint_bind(cap_id_t endpoint, cap_id_t service);
```

## 应用生命周期

### 启动应用

```c
// 创建应用域
domain_quota_t app_quota = {
    .max_memory = 0x1000000,      // 16MB
    .max_threads = 4,
    .max_caps = 256,
    .cpu_quota_percent = 5
};

domain_id_t app_domain;
domain_create(DOMAIN_TYPE_APPLICATION, HIC_DOMAIN_CORE, 
              &app_quota, &app_domain);

// 加载应用代码
phys_addr_t app_base;
pmm_alloc_frames(app_domain, 
                 (app_size + PAGE_SIZE - 1) / PAGE_SIZE,
                 PAGE_FRAME_APPLICATION, 
                 &app_base);
memcpy((void*)app_base, app_binary, app_size);

// 授予标准能力
cap_id_t stdin_cap, stdout_cap, stderr_cap;
cap_create_endpoint(app_domain, app_domain, &stdin_cap);
cap_create_endpoint(app_domain, app_domain, &stdout_cap);
cap_create_endpoint(app_domain, app_domain, &stderr_cap);

// 创建主线程
thread_id_t main_thread;
thread_create(app_domain, (void*)app_base, 0, &main_thread);
```

### 应用初始化

```c
// 应用入口点
int main(int argc, char *argv[]) {
    domain_id_t my_domain = get_current_domain();
    
    // 获取标准 I/O 能力
    cap_id_t stdin = get_stdin_cap();
    cap_id_t stdout = get_stdout_cap();
    
    // 初始化应用
    if (init_app() != 0) {
        return 1;
    }
    
    // 运行应用逻辑
    int result = run_app(argc, argv);
    
    // 清理
    cleanup_app();
    
    return result;
}
```

### 应用退出

```c
// 退出应用
void exit(int status) {
    domain_id_t my_domain = get_current_domain();
    
    // 释放所有资源
    cleanup_resources();
    
    // 退出当前线程
    sys_thread_exit(status);
}
```

## 安全机制

### 能力检查

所有系统调用都进行能力验证：

```c
hic_status_t sys_open(const char *path, int flags, cap_id_t *out) {
    domain_id_t caller = get_current_domain();
    
    // 验证路径权限
    if (!check_path_permission(caller, path, flags)) {
        return HIC_ERROR_PERMISSION;
    }
    
    // 创建文件能力
    return cap_create_file(caller, path, flags, out);
}
```

### 资源限制

严格遵守域配额：

```c
hic_status_t sys_mmap(size_t size, cap_rights_t prot) {
    domain_id_t caller = get_current_domain();
    domain_t *domain = get_domain(caller);
    
    // 检查内存配额
    if (domain->usage.memory_used + size > domain->quota.max_memory) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    // 分配内存
    phys_addr_t addr;
    hic_status_t status = pmm_alloc_frames(caller, 
                                           (size + PAGE_SIZE - 1) / PAGE_SIZE,
                                           PAGE_FRAME_APPLICATION, 
                                           &addr);
    
    if (status == HIC_SUCCESS) {
        domain->usage.memory_used += size;
    }
    
    return status;
}
```

### 隔离保证

- 内存隔离: 应用只能访问自己的内存
- 能力隔离: 应用只能访问拥有的能力
- 执行隔离: 应用间不能直接通信（必须通过 IPC）

## 性能优化

### 快速系统调用

使用快速系统调用路径：

```
传统系统调用: 中断 → 上下文切换 → 内核 → 系统调用 → 上下文切换 → 返回
HIC 系统调用:  快速指令 → 能力检查 → 直接操作 → 快速返回
```

### 批量操作

支持批量系统调用：

```c
// 批量读写
struct iovec {
    void *iov_base;
    size_t iov_len;
};

ssize_t sys_readv(cap_id_t fd, struct iovec *iov, int iovcnt);
ssize_t sys_writev(cap_id_t fd, const struct iovec *iov, int iovcnt);
```

## 开发指南

### 应用模板

```c
#include "hic.h"

int main(int argc, char *argv[]) {
    printf("Hello from Application-3!\n");
    
    // 解析参数
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <args>\n", argv[0]);
        return 1;
    }
    
    // 处理参数
    for (int i = 1; i < argc; i++) {
        printf("Arg %d: %s\n", i, argv[i]);
    }
    
    return 0;
}
```

### 编译应用

```bash
# 编译应用
hic-gcc -o app main.c

# 运行应用
hic-run ./app args...
```

## 标准 API

### POSIX 兼容

提供部分 POSIX API：

```c
// 标准 I/O
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

// 字符串操作
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);

// 内存操作
void *malloc(size_t size);
void free(void *ptr);
void *memcpy(void *dest, const void *src, size_t n);
```

## 运行时支持

### C 运行时

提供标准 C 库支持：

```c
// 程序启动
void _start(void) {
    int argc;
    char **argv;
    
    // 获取参数
    __hic_get_args(&argc, &argv);
    
    // 调用 main
    int result = main(argc, argv);
    
    // 退出
    exit(result);
}
```

### 其他语言

支持其他运行时：

- Python: 通过 FFI 调用 HIC API
- Rust: 使用 HIC 绑定
- Go: 使用 cgo 调用 HIC API

## 最佳实践

1. **资源管理**: 及时释放资源，避免泄漏
2. **错误处理**: 检查所有系统调用返回值
3. **权限最小化**: 只请求必需的能力
4. **性能优化**: 使用批量操作减少系统调用
5. **安全编码**: 遵循安全编码规范

## 相关文档

- [Core-0 层](./08-Core0.md) - 核心层文档
- [Privileged-1 层](./09-Privileged1.md) - 服务层文档
- [系统调用](./32-Communication.md) - IPC 通信机制
- [能力系统](./11-CapabilitySystem.md) - 能力系统详解

## 示例

应用示例参见 `src/Application-3/examples/` 目录。

---

*最后更新: 2026-02-14*