/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC域切换接口
 * 遵循文档第2.1节：跨域通信和隔离
 * 实现安全的域间上下文切换
 */

#ifndef HIC_KERNEL_DOMAIN_SWITCH_H
#define HIC_KERNEL_DOMAIN_SWITCH_H

#include "types.h"
#include "domain.h"
#include "capability.h"
#include "pagetable.h"
#include "hal.h"

/* 域切换上下文 */
typedef struct {
    /* 源域信息 */
    domain_id_t from_domain;
    thread_id_t from_thread;
    cap_id_t endpoint_cap;
    
    /* 目标域信息 */
    domain_id_t to_domain;
    
    /* 调用信息 */
    u64 syscall_num;
    u64 args[4];
    
    /* 返回值 */
    hic_status_t result;
    
    /* 保存的上下文 */
    hal_context_t saved_context;
} domain_switch_context_t;

/* 域切换接口 */
void domain_switch_init(void);

/* 执行域切换 */
hic_status_t domain_switch(domain_id_t from, domain_id_t to, 
                           cap_id_t endpoint_cap, u64 syscall_num,
                           u64* args, u32 arg_count);

/* 从域切换返回 */
void domain_switch_return(hic_status_t result);

/* 保存当前域上下文 */
void domain_switch_save_context(domain_id_t domain, hal_context_t* ctx);

/* 恢复域上下文 */
void domain_switch_restore_context(domain_id_t domain, hal_context_t* ctx);

/* 获取当前域ID */
domain_id_t domain_switch_get_current(void);

/* 设置当前域ID */
void domain_switch_set_current(domain_id_t domain);

/* 设置域页表 */
hic_status_t domain_switch_set_pagetable(domain_id_t domain, page_table_t* pagetable);

/* 获取域页表 */
page_table_t* domain_switch_get_pagetable(domain_id_t domain);

#endif /* HIC_KERNEL_DOMAIN_SWITCH_H */