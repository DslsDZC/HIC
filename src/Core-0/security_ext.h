/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全扩展机制层 (Security Extension Mechanism Layer)
 * 
 * Core-0 提供安全特性操作的原语接口：
 * - 内存加密区域管理
 * - TEE 会话管理
 * - CFI 控制
 * - 随机数生成
 * - 密钥操作
 * 
 * 策略层（security_service）负责：
 * - 安全策略配置
 * - CFI 违规响应
 * - 密钥生命周期策略
 * - 安全事件响应
 */

#ifndef HIC_KERNEL_SECURITY_EXT_H
#define HIC_KERNEL_SECURITY_EXT_H

#include "types.h"

/* ========== 内存加密类型 ========== */

typedef enum sec_mem_enc_type {
    SEC_MEM_ENC_NONE = 0,
    SEC_MEM_ENC_AMD_SEV,
    SEC_MEM_ENC_AMD_SEV_ES,
    SEC_MEM_ENC_AMD_SEV_SNP,
    SEC_MEM_ENC_INTEL_TDX,
    SEC_MEM_ENC_ARM_REALM,
    SEC_MEM_ENC_MAX
} sec_mem_enc_type_t;

typedef struct sec_mem_enc_caps {
    sec_mem_enc_type_t type;
    const char *name;
    bool supported;
    bool enabled;
    u32 key_bits;
} sec_mem_enc_caps_t;

/* ========== TEE 类型 ========== */

typedef enum sec_tee_type {
    SEC_TEE_NONE = 0,
    SEC_TEE_ARM_TRUSTZONE,
    SEC_TEE_INTEL_TDX,
    SEC_TEE_AMD_PSP,
    SEC_TEE_MAX
} sec_tee_type_t;

typedef struct sec_tee_caps {
    sec_tee_type_t type;
    const char *name;
    bool supported;
    u64 secure_memory_size;
} sec_tee_caps_t;

/* ========== CFI 模式 ========== */

typedef enum sec_cfi_mode {
    SEC_CFI_NONE = 0,
    SEC_CFI_SHADOW_STACK,
    SEC_CFI_IBT,
    SEC_CFI_PAUTH,
    SEC_CFI_MAX
} sec_cfi_mode_t;

typedef struct sec_cfi_caps {
    u32 modes_supported;
    bool hw_enforced;
    u64 violations_detected;
} sec_cfi_caps_t;

/* ========== RNG 能力 ========== */

typedef struct sec_rng_caps {
    bool supported;
    u32 entropy_bits;
    bool passes_fips;
} sec_rng_caps_t;

/* ========== 密钥管理 ========== */

typedef enum sec_key_type {
    SEC_KEY_TYPE_SYMMETRIC = 0,
    SEC_KEY_TYPE_ASYMMETRIC_PRIV,
    SEC_KEY_TYPE_DERIVED,
} sec_key_type_t;

typedef enum sec_key_algo {
    SEC_KEY_ALGO_AES_256 = 0,
    SEC_KEY_ALGO_RSA_2048,
    SEC_KEY_ALGO_ECC_P256,
} sec_key_algo_t;

/* ========== 架构操作回调 ========== */

typedef struct sec_arch_ops {
    void (*detect_mem_enc)(void);
    void (*detect_tee)(void);
    void (*detect_cfi)(void);
    void (*detect_rng)(void);
    
    hic_status_t (*mem_enc_enable)(sec_mem_enc_type_t type);
    void (*mem_enc_disable)(void);
    u64 (*mem_enc_create_region)(phys_addr_t base, u64 size, u32 flags);
    void (*mem_enc_destroy_region)(u64 region_id);
    
    u64 (*tee_open_session)(u64 app_id);
    void (*tee_close_session)(u64 session_id);
    hic_status_t (*tee_invoke)(u64 session_id, u32 command, void *params, u32 count);
    
    hic_status_t (*cfi_enable)(sec_cfi_mode_t mode);
    void (*cfi_disable)(void);
    u64 (*cfi_get_violations)(void);
    
    bool (*rng_get_bytes)(void *buffer, u32 count);
    
    u64 (*key_create)(sec_key_type_t type, sec_key_algo_t algo, const void *data, u32 size, u32 flags);
    void (*key_destroy)(u64 key_id);
    bool (*key_use)(u64 key_id, void *data, u32 size, bool encrypt);
} sec_arch_ops_t;

/* ========== 安全状态（供策略层查询） ========== */

typedef struct sec_state {
    sec_mem_enc_type_t mem_enc_type;
    bool mem_enc_enabled;
    sec_tee_type_t tee_type;
    bool tee_available;
    sec_cfi_mode_t cfi_mode;
    bool cfi_enabled;
    bool rng_available;
} sec_state_t;

/* ========== 机制层接口 ========== */

/* 初始化 */
void sec_init(void);
void sec_shutdown(void);
void sec_register_arch_ops(const sec_arch_ops_t *ops);

/* 状态查询 */
const sec_state_t* sec_get_state(void);

/* 内存加密 */
bool sec_mem_enc_supported(sec_mem_enc_type_t type);
const sec_mem_enc_caps_t* sec_mem_enc_get_caps(sec_mem_enc_type_t type);
hic_status_t sec_mem_enc_enable(sec_mem_enc_type_t type);
void sec_mem_enc_disable(void);
u64 sec_mem_enc_create_region(phys_addr_t base, u64 size, u32 flags);
void sec_mem_enc_destroy_region(u64 region_id);

/* TEE */
bool sec_tee_supported(void);
const sec_tee_caps_t* sec_tee_get_caps(void);
u64 sec_tee_open_session(u64 app_id);
void sec_tee_close_session(u64 session_id);
hic_status_t sec_tee_invoke(u64 session_id, u32 command, void *params, u32 count);

/* CFI */
bool sec_cfi_supported(sec_cfi_mode_t mode);
const sec_cfi_caps_t* sec_cfi_get_caps(void);
hic_status_t sec_cfi_enable(sec_cfi_mode_t mode);
void sec_cfi_disable(void);
sec_cfi_mode_t sec_cfi_get_mode(void);
u64 sec_cfi_get_violations(void);

/* RNG */
bool sec_rng_supported(void);
const sec_rng_caps_t* sec_rng_get_caps(void);
bool sec_rng_get_bytes(void *buffer, u32 count);
u32 sec_rng_get_u32(void);
u64 sec_rng_get_u64(void);

/* 密钥管理 */
u64 sec_key_create(sec_key_type_t type, sec_key_algo_t algo, const void *data, u32 size, u32 flags);
void sec_key_destroy(u64 key_id);
bool sec_key_use(u64 key_id, void *data, u32 size, bool encrypt);

#endif /* HIC_KERNEL_SECURITY_EXT_H */
