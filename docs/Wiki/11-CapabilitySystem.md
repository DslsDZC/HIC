<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 能力系统

能力系统是HIC的核心安全机制，提供细粒度的权限控制和资源访问。

## 概述

### 设计理念

HIC的能力系统基于以下核心原则：

1. **不可伪造性** - 能力不能被伪造或复制
2. **不可猜测性** - 能力ID不可预测
3. **细粒度权限** - 支持精确的权限控制
4. **动态生命周期** - 能力可以创建、传递、派生、撤销
5. **全局唯一性** - 每个能力有全局唯一的ID

### 对比访问控制

| 特性 | 传统ACL | 能力系统 |
|------|---------|----------|
| 权限检查 | 查找ACL表 | 持有能力即授权 |
| 权限传播 | 继承 | 显式传递 |
| 撤销 | 修改ACL | 立即撤销 |
| 细粒度 | 基于用户 | 基于对象 |
| 可组合性 | 差 | 好 |

## 能力结构

### 能力ID

```c
/**
 * 能力ID (64位)
 *
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * | 版本  | 检验和 | 类型   | 随机值                               |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * |  8位  |  8位   |  8位   |              40位随机值                |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 */
typedef u64 cap_id_t;

#define CAP_ID_VERSION_SHIFT    56
#define CAP_ID_CHECKSUM_SHIFT   48
#define CAP_ID_TYPE_SHIFT       40
#define CAP_ID_RANDOM_MASK      0xFFFFFFFFFFULL
```

### 能力类型

```c
/**
 * 能力类型
 */
typedef enum {
    CAP_TYPE_INVALID = 0,      /* 无效能力 */
    CAP_TYPE_MEMORY,           /* 内存能力 */
    CAP_TYPE_IO_PORT,          /* I/O端口能力 */
    CAP_TYPE_IRQ,              /* 中断能力 */
    CAP_TYPE_IPC_ENDPOINT,     /* IPC端点能力 */
    CAP_TYPE_DOMAIN,           /* 域能力 */
    CAP_TYPE_THREAD,           /* 线程能力 */
    CAP_TYPE_CAP_DERIVE,       /* 派生能力 */
    CAP_TYPE_MAX
} cap_type_t;
```

### 能力权限

```c
/**
 * 能力权限标志
 */
typedef u32 cap_rights_t;

#define CAP_PERM_READ       (1 << 0)  /* 读权限 */
#define CAP_PERM_WRITE      (1 << 1)  /* 写权限 */
#define CAP_PERM_EXECUTE    (1 << 2)  /* 执行权限 */
#define CAP_PERM_TRANSFER   (1 << 3)  /* 传递权限 */
#define CAP_PERM_DERIVE     (1 << 4)  /* 派生权限 */
#define CAP_PERM_REVOKE     (1 << 5)  /* 撤销权限 */
#define CAP_PERM_CALL       (1 << 6)  /* 调用权限 */
#define CAP_PERM_ALL        0xFFFFFFFF
```

### 能力标志

```c
/**
 * 能力标志
 */
typedef u32 cap_flags_t;

#define CAP_FLAG_REVOKED    (1 << 0)  /* 已撤销 */
#define CAP_FLAG_IMMUTABLE  (1 << 1)  /* 不可变 */
#define CAP_FLAG_PINNED     (1 << 2)  /* 固定 */
```

## 能力表

### 表结构

```c
/**
 * 能力表项
 */
typedef struct {
    cap_id_t id;              /* 能力ID */
    cap_type_t type;          /* 能力类型 */
    cap_rights_t rights;      /* 权限 */
    domain_id_t owner;        /* 所有者域ID */
    cap_flags_t flags;        /* 标志 */
    u32 ref_count;            /* 引用计数 */
    u64 create_time;          /* 创建时间 */
    union {
        struct {
            phys_addr_t base;  /* 物理地址 */
            u64 size;         /* 大小 */
        } memory;
        struct {
            u16 base;         /* I/O端口基地址 */
            u16 count;        /* 端口数量 */
        } io_port;
        struct {
            u32 vector;       /* 中断向量号 */
        } irq;
        struct {
            cap_id_t endpoint; /* IPC端点ID */
            u32 max_message_size;
        } ipc;
        struct {
            domain_id_t domain; /* 域ID */
        } domain;
        struct {
            thread_id_t thread; /* 线程ID */
        } thread;
        struct {
            cap_id_t parent;   /* 父能力ID */
            cap_rights_t sub_rights; /* 子权限 */
        } derive;
    } resource;
} cap_entry_t;
```

### 表组织

```c
/**
 * 全局能力表
 */
static cap_entry_t g_cap_table[MAX_CAPABILITIES];

/**
 * 能力空间
 * 每个域有一个能力空间，存储该域持有的能力句柄
 */
typedef struct {
    cap_id_t handles[MAX_CAPABILITIES_PER_DOMAIN];
    u32 count;
} cap_space_t;

static cap_space_t g_cap_spaces[MAX_DOMAINS];
```

## 能力操作

### 创建能力

```c
/**
 * @brief 创建新能力
 * @param owner 所有者域ID
 * @param type 能力类型
 * @param rights 权限
 * @param resource 资源描述
 * @param out 输出能力ID
 * @return 状态码
 */
status_t cap_create(domain_id_t owner, cap_type_t type,
                    cap_rights_t rights, void *resource,
                    cap_id_t *out)
{
    /* 验证参数 */
    if (owner >= MAX_DOMAINS) {
        return HIC_ERROR_INVALID_DOMAIN;
    }
    if (type >= CAP_TYPE_MAX) {
        return HIC_ERROR_INVALID_TYPE;
    }
    if (rights == 0) {
        return HIC_ERROR_INVALID_RIGHTS;
    }

    /* 查找空闲槽位 */
    cap_id_t id = find_free_cap_slot();
    if (id == HIC_INVALID_CAP_ID) {
        return HIC_ERROR_NO_RESOURCE;
    }

    /* 生成能力ID */
    cap_id_t cap_id = generate_cap_id(type);

    /* 初始化能力 */
    cap_entry_t *entry = &g_cap_table[id];
    entry->id = cap_id;
    entry->type = type;
    entry->rights = rights;
    entry->owner = owner;
    entry->flags = 0;
    entry->ref_count = 1;
    entry->create_time = get_timestamp();

    /* 复制资源描述 */
    memcpy(&entry->resource, resource, sizeof(entry->resource));

    /* 添加到域的能力空间 */
    add_to_cap_space(owner, cap_id);

    /* 记录审计日志 */
    AUDIT_LOG_CAP_CREATE(owner, cap_id);

    *out = cap_id;
    return HIC_SUCCESS;
}
```

### 验证能力

```c
/**
 * @brief 验证能力
 * @param domain 域ID
 * @param cap_id 能力ID
 * @param required_rights 所需权限
 * @param out 输出能力指针
 * @return 状态码
 */
status_t cap_verify(domain_id_t domain, cap_id_t cap_id,
                    cap_rights_t required_rights,
                    cap_entry_t **out)
{
    /* 快速路径：验证能力ID格式 */
    if (!is_valid_cap_id(cap_id)) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 查找能力 */
    cap_id_t index = cap_id_to_index(cap_id);
    if (index >= MAX_CAPABILITIES) {
        return HIC_ERROR_INVALID_CAP;
    }

    cap_entry_t *entry = &g_cap_table[index];

    /* 检查ID匹配 */
    if (entry->id != cap_id) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 检查类型 */
    if (entry->type == CAP_TYPE_INVALID) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 检查撤销状态 */
    if (entry->flags & CAP_FLAG_REVOKED) {
        return HIC_ERROR_REVOKED;
    }

    /* 检查所有权 */
    if (entry->owner != domain) {
        return HIC_ERROR_PERMISSION;
    }

    /* 检查权限 */
    if ((entry->rights & required_rights) != required_rights) {
        return HIC_ERROR_PERMISSION;
    }

    /* 增加引用计数 */
    entry->ref_count++;

    /* 记录审计日志 */
    AUDIT_LOG_CAP_VERIFY(domain, cap_id, true);

    *out = entry;
    return HIC_SUCCESS;
}
```

### 释放能力

```c
/**
 * @brief 释放能力引用
 * @param cap_id 能力ID
 */
void cap_release(cap_id_t cap_id)
{
    cap_id_t index = cap_id_to_index(cap_id);
    if (index >= MAX_CAPABILITIES) {
        return;
    }

    cap_entry_t *entry = &g_cap_table[index];
    if (entry->id != cap_id) {
        return;
    }

    /* 减少引用计数 */
    if (entry->ref_count > 0) {
        entry->ref_count--;
    }
}
```

### 传递能力

```c
/**
 * @brief 传递能力
 * @param from_domain 发送域ID
 * @param to_domain 接收域ID
 * @param cap_id 能力ID
 * @param sub_rights 子权限（0表示不限制）
 * @param out 输出新能力ID
 * @return 状态码
 */
status_t cap_transfer(domain_id_t from_domain, domain_id_t to_domain,
                     cap_id_t cap_id, cap_rights_t sub_rights,
                     cap_id_t *out)
{
    cap_entry_t *src_entry;

    /* 验证发送方能力 */
    status = cap_verify(from_domain, cap_id, CAP_PERM_TRANSFER, &src_entry);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 验证接收域 */
    if (to_domain >= MAX_DOMAINS) {
        cap_release(cap_id);
        return HIC_ERROR_INVALID_DOMAIN;
    }

    /* 验证子权限 */
    if (sub_rights != 0 && (sub_rights & src_entry->rights) != sub_rights) {
        cap_release(cap_id);
        return HIC_ERROR_INVALID_RIGHTS;
    }

    /* 创建派生能力 */
    cap_id_t new_id;
    cap_derive_info_t derive_info = {
        .parent = cap_id,
        .sub_rights = sub_rights ? sub_rights : src_entry->rights
    };

    status = cap_create(to_domain, CAP_TYPE_CAP_DERIVE,
                       derive_info.sub_rights, &derive_info, &new_id);
    if (status != HIC_SUCCESS) {
        cap_release(cap_id);
        return status;
    }

    /* 记录审计日志 */
    AUDIT_LOG_CAP_TRANSFER(from_domain, to_domain, cap_id);

    cap_release(cap_id);
    *out = new_id;
    return HIC_SUCCESS;
}
```

### 撤销能力

```c
/**
 * @brief 撤销能力
 * @param domain 域ID
 * @param cap_id 能力ID
 * @return 状态码
 */
status_t cap_revoke(domain_id_t domain, cap_id_t cap_id)
{
    cap_entry_t *entry;

    /* 验证能力 */
    status = cap_verify(domain, cap_id, CAP_PERM_REVOKE, &entry);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 检查是否可撤销 */
    if (entry->flags & CAP_FLAG_IMMUTABLE) {
        cap_release(cap_id);
        return HIC_ERROR_IMMUTABLE;
    }

    /* 标记为已撤销 */
    entry->flags |= CAP_FLAG_REVOKED;

    /* 从域的能力空间中移除 */
    remove_from_cap_space(domain, cap_id);

    /* 撤销所有派生能力 */
    revoke_derived_capabilities(cap_id);

    /* 记录审计日志 */
    AUDIT_LOG_CAP_REVOKE(domain, cap_id);

    cap_release(cap_id);
    return HIC_SUCCESS;
}
```

## 能力派生

### 派生机制

派生能力是从父能力创建的子能力，具有父能力的权限子集。

```c
/**
 * @brief 派生能力
 * @param domain 域ID
 * @param parent_cap_id 父能力ID
 * @param sub_rights 子权限
 * @param out 输出派生能力ID
 * @return 状态码
 */
status_t cap_derive(domain_id_t domain, cap_id_t parent_cap_id,
                   cap_rights_t sub_rights, cap_id_t *out)
{
    cap_entry_t *parent_entry;

    /* 验证父能力 */
    status = cap_verify(domain, parent_cap_id, CAP_PERM_DERIVE, &parent_entry);
    if (status != HIC_SUCCESS) {
        return status;
    }

    /* 验证子权限 */
    if ((sub_rights & parent_entry->rights) != sub_rights) {
        cap_release(parent_cap_id);
        return HIC_ERROR_INVALID_RIGHTS;
    }

    /* 创建派生能力 */
    cap_id_t new_id;
    cap_derive_info_t derive_info = {
        .parent = parent_cap_id,
        .sub_rights = sub_rights
    };

    status = cap_create(domain, CAP_TYPE_CAP_DERIVE,
                       sub_rights, &derive_info, &new_id);
    if (status != HIC_SUCCESS) {
        cap_release(parent_cap_id);
        return status;
    }

    /* 记录审计日志 */
    AUDIT_LOG_CAP_DERIVE(domain, parent_cap_id, new_id);

    cap_release(parent_cap_id);
    *out = new_id;
    return HIC_SUCCESS;
}
```

### 派生树

```
父能力 (READ|WRITE|EXECUTE)
├── 子能力1 (READ)
├── 子能力2 (WRITE)
└── 子能力3 (READ|WRITE)
    ├── 孙能力1 (READ)
    └── 孙能力2 (WRITE)
```

## 能力撤销传播

### 传播机制

当撤销一个能力时，所有派生自该能力的能力也会被撤销。

```c
/**
 * @brief 撤销所有派生能力
 * @param parent_cap_id 父能力ID
 */
void revoke_derived_capabilities(cap_id_t parent_cap_id)
{
    /* 遍历所有能力 */
    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        cap_entry_t *entry = &g_cap_table[i];

        /* 跳过无效能力 */
        if (entry->type == CAP_TYPE_INVALID) {
            continue;
        }

        /* 检查是否是派生能力 */
        if (entry->type == CAP_TYPE_CAP_DERIVE &&
            entry->resource.derive.parent == parent_cap_id) {

            /* 递归撤销 */
            revoke_derived_capabilities(entry->id);

            /* 标记为已撤销 */
            entry->flags |= CAP_FLAG_REVOKED;

            /* 从域的能力空间中移除 */
            remove_from_cap_space(entry->owner, entry->id);
        }
    }
}
```

## 安全特性

### 不可伪造性

能力ID包含随机数和校验和，无法伪造：

```c
/**
 * @brief 生成能力ID
 * @param type 能力类型
 * @return 能力ID
 */
static cap_id_t generate_cap_id(cap_type_t type)
{
    /* 生成随机值 */
    u64 random = get_random64();

    /* 计算校验和 */
    u8 checksum = calculate_checksum(type, random);

    /* 组合能力ID */
    cap_id_t cap_id = 0;
    cap_id |= (CAP_VERSION << CAP_ID_VERSION_SHIFT);
    cap_id |= (checksum << CAP_ID_CHECKSUM_SHIFT);
    cap_id |= (type << CAP_ID_TYPE_SHIFT);
    cap_id |= (random & CAP_ID_RANDOM_MASK);

    return cap_id;
}
```

### 能力验证

所有能力使用前必须验证：

```c
/* 使用内存能力 */
cap_entry_t *cap;
status = cap_verify(domain, mem_cap, CAP_PERM_READ | CAP_PERM_WRITE, &cap);
if (status != HIC_SUCCESS) {
    return status;
}

/* 安全访问内存 */
memcpy(buffer, (void *)cap->resource.memory.base, size);

/* 释放能力 */
cap_release(mem_cap);
```

### 最小权限原则

只授予必要的权限：

```c
/* 好：只授予读权限 */
cap_create(domain, CAP_TYPE_MEMORY, CAP_PERM_READ, &mem_resource, &cap);

/* 不好：授予所有权限 */
cap_create(domain, CAP_TYPE_MEMORY, CAP_PERM_ALL, &mem_resource, &cap);
```

## 性能优化

### 快速验证

```c
/**
 * @brief 快速能力验证（内联）
 */
static inline status_t cap_verify_fast(domain_id_t domain, cap_id_t cap_id,
                                      cap_rights_t required_rights)
{
    /* 快速路径：检查能力ID格式 */
    if (UNLIKELY(!is_valid_cap_id(cap_id))) {
        return HIC_ERROR_INVALID_CAP;
    }

    cap_id_t index = cap_id_to_index(cap_id);
    cap_entry_t *entry = &g_cap_table[index];

    /* 快速路径：检查ID匹配 */
    if (UNLIKELY(entry->id != cap_id)) {
        return HIC_ERROR_INVALID_CAP;
    }

    /* 快速路径：检查撤销状态 */
    if (UNLIKELY(entry->flags & CAP_FLAG_REVOKED)) {
        return HIC_ERROR_REVOKED;
    }

    /* 快速路径：检查所有权 */
    if (UNLIKELY(entry->owner != domain)) {
        return HIC_ERROR_PERMISSION;
    }

    /* 快速路径：检查权限 */
    if (UNLIKELY((entry->rights & required_rights) != required_rights)) {
        return HIC_ERROR_PERMISSION;
    }

    /* 增加引用计数 */
    entry->ref_count++;

    return HIC_SUCCESS;
}
```

### 缓存优化

```c
/**
 * 能力缓存
 * 缓存最近使用的能力
 */
typedef struct {
    cap_id_t cap_id;
    cap_entry_t *entry;
    u64 timestamp;
} cap_cache_entry_t;

static cap_cache_entry_t g_cap_cache[CAP_CACHE_SIZE];

/**
 * @brief 从缓存获取能力
 */
static inline cap_entry_t *cap_cache_get(cap_id_t cap_id)
{
    /* 查找缓存 */
    for (int i = 0; i < CAP_CACHE_SIZE; i++) {
        if (g_cap_cache[i].cap_id == cap_id) {
            g_cap_cache[i].timestamp = get_timestamp();
            return g_cap_cache[i].entry;
        }
    }

    return NULL;
}
```

## 调试支持

### 转储能力表

```c
/**
 * @brief 转储能力表
 */
void cap_dump_table(void)
{
    console_puts("=== Capability Table ===\n");

    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        cap_entry_t *entry = &g_cap_table[i];

        if (entry->type == CAP_TYPE_INVALID) {
            continue;
        }

        console_puts("Cap ID: 0x");
        console_puthex64(entry->id);
        console_puts("\n");
        console_puts("  Type: ");
        console_puts(cap_type_to_string(entry->type));
        console_puts("\n");
        console_puts("  Owner: ");
        console_putu64(entry->owner);
        console_puts("\n");
        console_puts("  Rights: 0x");
        console_puthex32(entry->rights);
        console_puts("\n");
        console_puts("  Flags: 0x");
        console_puthex32(entry->flags);
        console_puts("\n");
    }
}
```

### 统计信息

```c
/**
 * @brief 能力统计
 */
typedef struct {
    u64 total_caps;         /* 总能力数 */
    u64 active_caps;        /* 活跃能力数 */
    u64 revoked_caps;       /* 已撤销能力数 */
    u64 caps_by_type[CAP_TYPE_MAX];  /* 按类型统计 */
} cap_stats_t;

/**
 * @brief 获取能力统计
 */
void cap_get_stats(cap_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));

    for (cap_id_t i = 0; i < MAX_CAPABILITIES; i++) {
        cap_entry_t *entry = &g_cap_table[i];

        if (entry->type == CAP_TYPE_INVALID) {
            continue;
        }

        stats->total_caps++;
        stats->caps_by_type[entry->type]++;

        if (entry->flags & CAP_FLAG_REVOKED) {
            stats->revoked_caps++;
        } else {
            stats->active_caps++;
        }
    }
}
```

## 参考资料

- [Core-0层](./08-Core0.md)
- [形式化验证](./15-FormalVerification.md)
- [安全机制](./13-SecurityMechanisms.md)

---

*最后更新: 2026-02-14*