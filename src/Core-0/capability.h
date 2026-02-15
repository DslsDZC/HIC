/**
 * HIK能力系统(Capability System)头文件
 * 遵循三层模型文档第3.1节：能力系统的静态生命周期与安全传递
 */

#ifndef HIK_KERNEL_CAPABILITY_H
#define HIK_KERNEL_CAPABILITY_H

#include "types.h"
#include "formal_verification.h"  /* 包含 cap_type_t 和 mem_region_t 定义 */

/* 能力表大小 */
#define CAP_TABLE_SIZE  65536

/* 内存权限标志 */
#define CAP_MEM_READ   (1U << 0)
#define CAP_MEM_WRITE  (1U << 1)
#define CAP_MEM_EXEC   (1U << 2)
#define CAP_MEM_DEVICE (1U << 3)  /* 设备内存 */

/* 能力权限 */
typedef u32 cap_rights_t;

/* 全局能力表项 */
typedef struct cap_entry {
    cap_id_t       cap_id;       /* 能力ID */
    cap_type_t     type;         /* 能力类型 */
    cap_rights_t   rights;       /* 权限标志 */
    domain_id_t    owner;        /* 拥有者域ID */
    
    /* 类型特定的数据 */
    union {
        struct {
            phys_addr_t base;    /* 物理基地址 */
            size_t      size;    /* 大小(字节) */
        } memory;
        
        struct {
            phys_addr_t base;
            size_t      size;
        } mmio;
        
        struct {
            irq_vector_t vector;
        } irq;
        
        struct {
            domain_id_t  target_domain;
            cap_id_t     endpoint_id;
        } endpoint;
        
        struct {
            cap_id_t parent_cap;  /* 父能力 */
            cap_rights_t sub_rights; /* 子集权限 */
        } derive;
        
        struct {
            u64 service_uuid;
        } service;
    };
    
    u32 ref_count;            /* 引用计数 */
    u8  flags;                /* 标志位 */
#define CAP_FLAG_REVOKED  (1U << 0)  /* 已撤销 */
#define CAP_FLAG_IMMUTABLE (1U << 1) /* 不可变 */
} cap_entry_t;

/* 域能力空间句柄 */
typedef struct cap_handle {
    cap_id_t  cap_id;        /* 全局能力ID */
    u32       token;         /* 混淆令牌 */
} cap_handle_t;

/* 能力系统接口 */
void capability_system_init(void);

/* 创建能力 */
hik_status_t cap_create_memory(domain_id_t owner, phys_addr_t base, 
                               size_t size, cap_rights_t rights, cap_id_t *out);
hik_status_t cap_create_mmio(domain_id_t owner, phys_addr_t base, 
                             size_t size, cap_id_t *out);
hik_status_t cap_create_irq(domain_id_t owner, irq_vector_t vector, cap_id_t *out);

/* 传递能力 */
hik_status_t cap_transfer(domain_id_t from, domain_id_t to, cap_id_t cap);
hik_status_t cap_derive(domain_id_t owner, cap_id_t parent, 
                        cap_rights_t sub_rights, cap_id_t *out);

/* 撤销能力 */
hik_status_t cap_revoke(cap_id_t cap);

/* 验证能力访问 */
hik_status_t cap_check_access(domain_id_t domain, cap_id_t cap, cap_rights_t required);

/* 查询能力信息 */
hik_status_t cap_get_info(cap_id_t cap, cap_entry_t *info);

#endif /* HIK_KERNEL_CAPABILITY_H */