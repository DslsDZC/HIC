<!--
SPDX-FileCopyrightText: 2026 * <*@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# 物理内存管理

## 概述

HIC 采用物理内存直接映射策略，避免了传统内核的虚拟内存管理开销。这种设计简化了内存管理，提高了性能，同时通过能力系统保证了安全性。

## 设计目标

- **零拷贝**: 消除地址转换开销
- **简化管理**: 无需复杂的页表维护
- **安全隔离**: 通过能力系统保证内存隔离
- **高效分配**: 快速的内存分配和释放

## 架构设计

### 直接映射模型

```
传统内核: 虚拟地址 → 页表 → TLB → 物理地址
HIC 内核:  物理地址 → 直接访问
```

### 内存层次

```
┌─────────────────────────────────────┐
│   Core-0 (内核核心)                 │
│   - 物理内存直接映射                │
│   - 管理所有内存                    │
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│   Privileged-1 (特权服务)          │
│   - 分配的物理内存直接映射          │
│   - 无地址转换开销                  │
└─────────────────────────────────────┘
┌─────────────────────────────────────┐
│   Application-3 (应用)              │
│   - 受限的物理内存直接映射          │
│   - 严格配额限制                    │
└─────────────────────────────────────┘
```

## 内存分配器

### 页帧分配器

```c
// 页帧类型
typedef enum {
    PAGE_FRAME_FREE,        // 空闲
    PAGE_FRAME_RESERVED,    // 保留（硬件、BIOS）
    PAGE_FRAME_CORE,        // Core-0 使用
    PAGE_FRAME_PRIVILEGED,  // Privileged-1 使用
    PAGE_FRAME_APPLICATION, // Application-3 使用
    PAGE_FRAME_SHARED,      // 共享内存
} page_frame_type_t;

// 页帧描述符
typedef struct page_frame {
    u64              base_addr;    // 物理基地址
    page_frame_type_t type;        // 类型
    domain_id_t       owner;       // 拥有者域
    u32              ref_count;    // 引用计数
} page_frame_t;
```

### 位图分配

使用位图管理空闲页帧：

```c
#define MAX_FRAMES (16 * 1024 * 1024 / PAGE_SIZE)  // 16MB 最大支持
#define FRAME_BITMAP_SIZE ((MAX_FRAMES + 7) / 8)

static u8 frame_bitmap[FRAME_BITMAP_SIZE];
static u64 total_frames = 0;
static u64 free_frames = 0;
```

### 分配算法

```c
hic_status_t pmm_alloc_frames(domain_id_t owner, u32 count, 
                               page_frame_type_t type, phys_addr_t *out) {
    // 检查域配额
    if (owner != HIC_DOMAIN_CORE) {
        domain_t *domain = get_domain(owner);
        if (domain->usage.memory_used + count * PAGE_SIZE > 
            domain->quota.max_memory) {
            return HIC_ERROR_QUOTA_EXCEEDED;
        }
    }
    
    // 查找连续空闲页帧
    u64 start_frame = find_free_frames(count);
    if (start_frame == INVALID_FRAME) {
        return HIC_ERROR_NO_MEMORY;
    }
    
    // 标记为已分配
    for (u32 i = 0; i < count; i++) {
        set_bit(frame_bitmap, start_frame + i);
    }
    
    free_frames -= count;
    used_memory += count * PAGE_SIZE;
    
    *out = start_frame * PAGE_SIZE;
    return HIC_SUCCESS;
}
```

## 内存区域管理

### 内存区域

```c
typedef struct mem_region {
    phys_addr_t base;     // 基地址
    size_t      size;     // 大小
    u32         type;     // 类型
    struct mem_region *next; // 下一个区域
} mem_region_t;
```

### 区域管理

```c
// 添加内存区域
hic_status_t pmm_add_region(phys_addr_t base, size_t size) {
    // 对齐到页边界
    phys_addr_t aligned_base = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t aligned_size = size - (aligned_base - base);
    aligned_size &= ~(PAGE_SIZE - 1);
    
    // 创建区域描述符
    mem_region_t *region = (mem_region_t *)aligned_base;
    region->base = aligned_base + sizeof(mem_region_t);
    region->size = aligned_size - sizeof(mem_region_t);
    region->type = MEM_REGION_USABLE;
    
    // 添加到区域链表
    region->next = mem_regions;
    mem_regions = region;
    
    // 更新位图
    u64 start_frame = aligned_base / PAGE_SIZE;
    u64 num_frames = aligned_size / PAGE_SIZE;
    for (u64 i = 0; i < num_frames; i++) {
        clear_bit(frame_bitmap, start_frame + i);
    }
    
    total_frames += num_frames;
    free_frames += num_frames;
    
    return HIC_SUCCESS;
}
```

## 域内存管理

### 域内存分配

```c
// 域控制块
typedef struct domain {
    phys_addr_t    phys_base;     // 物理基地址
    size_t         phys_size;     // 物理大小
    domain_quota_t quota;         // 配额
    struct {
        size_t memory_used;      // 已用内存
    } usage;
} domain_t;

// 域内存分配
hic_status_t domain_alloc_memory(domain_id_t domain, size_t size, 
                                  phys_addr_t *out) {
    domain_t *d = get_domain(domain);
    
    // 检查配额
    if (d->usage.memory_used + size > d->quota.max_memory) {
        return HIC_ERROR_QUOTA_EXCEEDED;
    }
    
    // 分配内存
    hic_status_t status = pmm_alloc_frames(domain, 
                                           (size + PAGE_SIZE - 1) / PAGE_SIZE,
                                           PAGE_FRAME_APPLICATION, 
                                           out);
    
    if (status == HIC_SUCCESS) {
        d->usage.memory_used += size;
    }
    
    return status;
}
```

### 域内存释放

```c
hic_status_t domain_free_memory(domain_id_t domain, phys_addr_t addr, 
                                 size_t size) {
    domain_t *d = get_domain(domain);
    
    // 释放内存
    hic_status_t status = pmm_free_frames(addr, 
                                          (size + PAGE_SIZE - 1) / PAGE_SIZE);
    
    if (status == HIC_SUCCESS) {
        d->usage.memory_used -= size;
    }
    
    return status;
}
```

## 共享内存

### 共享内存能力

```c
// 创建共享内存
hic_status_t cap_create_shared_memory(domain_id_t owner, 
                                      size_t size, 
                                      cap_rights_t rights,
                                      cap_id_t *out) {
    // 分配共享内存
    phys_addr_t phys_addr;
    hic_status_t status = pmm_alloc_frames(owner, 
                                           (size + PAGE_SIZE - 1) / PAGE_SIZE,
                                           PAGE_FRAME_SHARED, 
                                           &phys_addr);
    
    if (status != HIC_SUCCESS) {
        return status;
    }
    
    // 创建共享内存能力
    return cap_create_memory(owner, phys_addr, size, rights, out);
}

// 共享内存到其他域
hic_status_t share_memory(domain_id_t from, domain_id_t to, 
                          cap_id_t mem_cap) {
    // 转移能力
    return cap_transfer(from, to, mem_cap);
}
```

## 内存统计

### 全局统计

```c
void pmm_get_stats(u64 *total_pages, u64 *free_pages, u64 *used_pages) {
    if (total_pages) *total_pages = total_frames;
    if (free_pages) *free_pages = free_frames;
    if (used_pages) *used_pages = total_frames - free_frames;
}
```

### 域统计

```c
void domain_get_memory_stats(domain_id_t domain, 
                             u64 *total, u64 *used) {
    domain_t *d = get_domain(domain);
    
    if (total) *total = d->quota.max_memory;
    if (used) *used = d->usage.memory_used;
}
```

## 性能优化

### 快速分配

- 使用位图快速查找空闲页
- 批量分配减少分配次数
- 预分配常用大小的内存池

### 缓存友好

- 物理内存连续，减少缓存未命中
- 直接访问，避免 TLB 未命中
- 对齐分配，提高缓存效率

## 安全机制

### 能力保护

```c
// 检查内存访问权限
hic_status_t check_memory_access(domain_id_t domain, 
                                  phys_addr_t addr, 
                                  size_t size, 
                                  cap_rights_t required) {
    // 查找包含该地址的能力
    cap_id_t mem_cap = find_memory_capability(domain, addr, size);
    
    if (mem_cap == INVALID_CAP_ID) {
        return HIC_ERROR_PERMISSION;
    }
    
    // 检查权限
    return cap_check_access(domain, mem_cap, required);
}
```

### 隔离保证

- 每个域只能访问自己的内存
- 能力系统强制内存访问控制
- 形式化验证保证内存隔离性

## 最佳实践

1. **批量分配**: 一次性分配多个页帧
2. **及时释放**: 不再使用的内存立即释放
3. **对齐访问**: 使用页对齐的地址
4. **配额管理**: 遵守域内存配额
5. **共享谨慎**: 最小化共享内存使用

## 相关文档

- [Core-0 层](./08-Core0.md) - 核心层内存管理
- [能力系统](./11-CapabilitySystem.md) - 内存能力
- [形式化验证](./15-FormalVerification.md) - 内存隔离验证

---

*最后更新: 2026-02-14*