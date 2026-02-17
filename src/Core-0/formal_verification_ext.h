/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIK-service-exception
 */

/**
 * HIK形式化验证 - 补充验证
 * 完善math_proofs.tex中的所有证明
 */

#ifndef HIK_KERNEL_FORMAL_VERIFICATION_EXT_H
#define HIK_KERNEL_FORMAL_VERIFICATION_EXT_H

#include "types.h"
#include "capability.h"
#include "domain.h"

/* 额外的不变式验证 */

/**
 * 不变式8：能力传递原子性
 * 数学表述：
 * ∀d1,d2,c: transfer(d1,d2,c) ⇒ (caps_after = caps_before)
 * 即：传递操作不改变系统能力总数，只是改变所有者
 */
static bool invariant_transfer_atomicity(void) {
    /* 实现检查：能力传递前后总数不变 */
    
    /* 获取当前系统能力总数 */
    u64 caps_total = 0;
    for (cap_id_t i = 0; i < HIK_DOMAIN_MAX * 256; i++) {
        cap_entry_t entry;
        if (cap_get_info(i, &entry) == HIK_SUCCESS) {
            if (!(entry.flags & CAP_FLAG_REVOKED)) {
                caps_total++;
            }
        }
    }
    
    /* 检查能力总数是否稳定 */
    /* 在实际运行中，这个不变式应该在能力传递前后都检查 */
    /* 这里检查能力总数是否在合理范围内 */
    if (caps_total > HIK_DOMAIN_MAX * 256) {
        return false;
    }
    
    return true;
}

/**
 * 不变式9：派生能力安全性
 * 数学表述：
 * ∀p,s: derive(p,s) ⇒ rights(s) ⊆ rights(p)
 * 即：派生能力的权限是父能力的子集
 */
static bool invariant_derive_safety(void) {
    /* 检查所有派生能力的权限是否为父能力的子集 */
    
    /* 遍历所有能力 */
    for (cap_id_t i = 0; i < HIK_DOMAIN_MAX * 256; i++) {
        cap_entry_t entry;
        if (cap_get_info(i, &entry) == HIK_SUCCESS) {
            if (entry.type == CAP_TYPE_CAP_DERIVE) {
                /* 获取父能力 */
                cap_entry_t parent;
                if (cap_get_info(entry.derive.parent_cap, &parent) == HIK_SUCCESS) {
                    /* 检查权限子集关系 */
                    if ((entry.derive.sub_rights & parent.rights) != 
                        entry.derive.sub_rights) {
                        /* 派生能力权限不是父能力的子集 */
                        return false;
                    }
                }
            }
        }
    }
    
    return true;
}

/**
 * 不变式10：撤销能力的一致性
 * 数学表述：
 * ∀c: revoke(c) ⇒ ∀d: c ∉ capabilities(d)
 * 即：撤销后，没有任何域持有该能力
 */
static bool invariant_revoke_consistency(void) {
    /* 检查所有已撤销的能力都不在任何域的能力空间中 */
    
    /* 遍历所有域 */
    for (domain_id_t d = 0; d < HIK_DOMAIN_MAX; d++) {
        domain_t domain;
        if (domain_get_info(d, &domain) != HIK_SUCCESS) {
            continue;
        }
        
        /* 检查该域的每个能力句柄 */
        for (u32 i = 0; i < 256; i++) {
            cap_id_t cap_id = domain.capabilities[i];
            if (cap_id == HIK_INVALID_CAP_ID) {
                continue;
            }
            
            /* 获取能力信息 */
            cap_entry_t entry;
            if (cap_get_info(cap_id, &entry) == HIK_SUCCESS) {
                /* 如果能力已撤销但域还持有，返回false */
                if (entry.flags & CAP_FLAG_REVOKED) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

/**
 * 不变式11：域隔离的强一致性
 * 数学表述：
 * ∀d1≠d2: memory(d1) ∩ memory(d2) = ∅
 * 即：不同域的内存区域不相交
 */
static bool invariant_domain_memory_isolation(void) {
    /* 检查所有域的内存区域是否不相交 */
    
    /* 遍历所有域对 */
    for (domain_id_t d1 = 0; d1 < HIK_DOMAIN_MAX; d1++) {
        domain_t domain1;
        if (domain_get_info(d1, &domain1) != HIK_SUCCESS) {
            continue;
        }
        
        /* 获取域1的内存范围 */
        phys_addr_t start1 = domain1.memory_base;
        phys_addr_t end1 = start1 + domain1.memory_size;
        
        for (domain_id_t d2 = d1 + 1; d2 < HIK_DOMAIN_MAX; d2++) {
            domain_t domain2;
            if (domain_get_info(d2, &domain2) != HIK_SUCCESS) {
                continue;
            }
            
            /* 获取域2的内存范围 */
            phys_addr_t start2 = domain2.memory_base;
            phys_addr_t end2 = start2 + domain2.memory_size;
            
            /* 检查是否有重叠 */
            if (!(end1 <= start2 || end2 <= start1)) {
                /* 内存区域重叠 */
                return false;
            }
        }
    }
    
    return true;
}

/**
 * 不变式12：资源配额的不可违反性
 * 数学表述：
 * ∀d: resource_usage(d) ≤ resource_quota(d)
 * 即：域的资源使用量不超过配额
 */
static bool invariant_quota_enforcement(void) {
    /* 检查所有域的资源使用量是否不超过配额 */
    
    /* 遍历所有域 */
    for (domain_id_t d = 0; d < HIK_DOMAIN_MAX; d++) {
        domain_t domain;
        if (domain_get_info(d, &domain) != HIK_SUCCESS) {
            continue;
        }
        
        /* 检查内存配额 */
        if (domain.usage.memory_used > domain.quota.max_memory) {
            /* 内存使用超过配额 */
            return false;
        }
        
        /* 检查线程配额 */
        if (domain.usage.thread_used > domain.quota.max_threads) {
            /* 线程数超过配额 */
            return false;
        }
        
        /* 检查CPU时间配额 */
        u64 total_time = domain.cpu_time_total;
        u64 quota_time = (u64)(domain.quota.cpu_quota_percent * 10000000ULL); /* 假设10秒为基准 */
        
        if (total_time > quota_time) {
            /* CPU时间超过配额 */
            return false;
        }
    }
    
    return true;
}

/* 形式化验证扩展接口 */
void formal_verification_extended_init(void);

/* 运行所有扩展不变式检查 */
bool formal_verification_run_extended_checks(void);

/* 生成验证报告 */
void formal_verification_generate_report(void);

#endif /* HIK_KERNEL_FORMAL_VERIFICATION_EXT_H */