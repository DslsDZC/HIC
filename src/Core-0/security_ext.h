/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 安全扩展抽象层 (Security Extension Abstraction Layer)
 * 
 * 提供跨平台的安全特性统一接口：
 * 1. 内存加密：AMD SEV、Intel SGX、TDX
 * 2. 可信执行环境（TEE）：ARM TrustZone、Intel TDX
 * 3. 控制流完整性（CFI）：硬件 CFI 支持
 * 4. 随机数生成：硬件 RNG
 * 5. 密钥管理：安全密钥存储与使用
 */

#ifndef HIC_KERNEL_SECURITY_EXT_H
#define HIC_KERNEL_SECURITY_EXT_H

#include "types.h"

/* ========== 内存加密接口 ========== */

/* 内存加密类型 */
typedef enum sec_mem_enc_type {
    SEC_MEM_ENC_NONE = 0,           /* 无加密 */
    SEC_MEM_ENC_AMD_SEV,            /* AMD SEV (Secure Encrypted Virtualization) */
    SEC_MEM_ENC_AMD_SEV_ES,         /* AMD SEV-ES (Encrypted State) */
    SEC_MEM_ENC_AMD_SEV_SNP,        /* AMD SEV-SNP (Secure Nested Paging) */
    SEC_MEM_ENC_INTEL_SGX,          /* Intel SGX (Software Guard Extensions) */
    SEC_MEM_ENC_INTEL_TDX,          /* Intel TDX (Trust Domain Extensions) */
    SEC_MEM_ENC_ARM_REALM,          /* ARM Realm (CCA) */
    SEC_MEM_ENC_MAX
} sec_mem_enc_type_t;

/* 内存加密能力 */
typedef struct sec_mem_enc_caps {
    sec_mem_enc_type_t type;        /* 加密类型 */
    const char *name;               /* 名称 */
    bool supported;                 /* 是否支持 */
    bool enabled;                   /* 是否启用 */
    
    /* 加密属性 */
    u32 key_bits;                   /* 密钥位数 */
    u32 asid_bits;                  /* ASID/Handle 位数 */
    u64 max_pages;                  /* 最大加密页数 */
    
    /* 功能标志 */
    bool supports_attestation;      /* 支持远程认证 */
    bool supports_migration;        /* 支持迁移 */
    bool supports_debug;            /* 支持调试 */
} sec_mem_enc_caps_t;

/* 加密内存区域 */
typedef struct sec_encrypted_region {
    u64 region_id;                  /* 区域 ID */
    phys_addr_t phys_base;          /* 物理基址 */
    virt_addr_t virt_base;          /* 虚拟基址 */
    u64 size;                       /* 大小 */
    u32 asid;                       /* 地址空间 ID */
    u32 flags;                      /* 区域标志 */
    bool active;                    /* 是否活跃 */
} sec_encrypted_region_t;

/* 加密区域标志 */
#define SEC_ENC_FLAG_EXCLUSIVE  (1 << 0)   /* 独占访问 */
#define SEC_ENC_FLAG_DEBUG      (1 << 1)   /* 调试模式 */
#define SEC_ENC_FLAG_MIGRATABLE (1 << 2)   /* 可迁移 */

/* ========== 可信执行环境 (TEE) ========== */

/* TEE 类型 */
typedef enum sec_tee_type {
    SEC_TEE_NONE = 0,               /* 无 TEE */
    SEC_TEE_ARM_TRUSTZONE,          /* ARM TrustZone */
    SEC_TEE_INTEL_TDX,              /* Intel TDX */
    SEC_TEE_AMD_PSP,                /* AMD PSP */
    SEC_TEE_RISCV_PMP,              /* RISC-V PMP */
    SEC_TEE_MAX
} sec_tee_type_t;

/* TEE 能力 */
typedef struct sec_tee_caps {
    sec_tee_type_t type;            /* TEE 类型 */
    const char *name;               /* 名称 */
    bool supported;                 /* 是否支持 */
    bool enabled;                   /* 是否启用 */
    
    /* 资源限制 */
    u64 secure_memory_size;         /* 安全世界内存大小 */
    u32 max_trusted_apps;           /* 最大可信应用数 */
    u32 max_sessions;               /* 最大会话数 */
    
    /* 功能标志 */
    bool supports_crypto;           /* 支持加密操作 */
    bool supports_storage;          /* 支持安全存储 */
    bool supports_attestation;      /* 支持认证 */
} sec_tee_caps_t;

/* TEE 会话 */
typedef struct sec_tee_session {
    u64 session_id;                 /* 会话 ID */
    u64 app_id;                     /* 应用 ID */
    u32 state;                      /* 会话状态 */
    u64 created_time;               /* 创建时间 */
    u64 last_active;                /* 最后活动时间 */
} sec_tee_session_t;

/* TEE 会话状态 */
#define SEC_TEE_SESSION_CLOSED   0
#define SEC_TEE_SESSION_OPEN     1
#define SEC_TEE_SESSION_BUSY     2

/* ========== 控制流完整性 (CFI) ========== */

/* CFI 模式 */
typedef enum sec_cfi_mode {
    SEC_CFI_DISABLED = 0,           /* 禁用 */
    SEC_CFI_SHADOW_STACK,           /* 影子栈 */
    SEC_CFI_IBT,                    /* 间接分支跟踪 (Intel IBT) */
    SEC_CFI_PAUTH,                  /* 指针认证 (ARM PA) */
    SEC_CFI_ZICFI,                  /* RISC-V CFI 扩展 */
    SEC_CFI_FULL,                   /* 完整 CFI */
    SEC_CFI_MAX
} sec_cfi_mode_t;

/* CFI 能力 */
typedef struct sec_cfi_caps {
    sec_cfi_mode_t mode;            /* 当前模式 */
    u32 supported_modes;            /* 支持的模式掩码 */
    bool hw_enforced;               /* 硬件强制 */
    
    /* 影子栈属性 */
    u32 shadow_stack_size;          /* 影子栈大小 */
    u32 shadow_stack_count;         /* 影子栈数量 */
    
    /* 统计 */
    u64 violations_detected;        /* 检测到的违规次数 */
    u64 violations_blocked;         /* 阻止的违规次数 */
} sec_cfi_caps_t;

/* CFI 违规信息 */
typedef struct sec_cfi_violation {
    u64 timestamp;                  /* 时间戳 */
    u64 instruction_addr;           /* 指令地址 */
    u64 target_addr;                /* 目标地址 */
    sec_cfi_mode_t mode;            /* CFI 模式 */
    u32 domain_id;                  /* 域 ID */
    bool blocked;                   /* 是否被阻止 */
} sec_cfi_violation_t;

/* ========== 硬件随机数生成器 ========== */

/* RNG 类型 */
typedef enum sec_rng_type {
    SEC_RNG_NONE = 0,               /* 无硬件 RNG */
    SEC_RNG_INTEL_RDRAND,           /* Intel RDRAND/RDSEED */
    SEC_RNG_AMD_TRNG,               /* AMD TRNG */
    SEC_RNG_ARM_TRNG,               /* ARM TRNG */
    SEC_RNG_RISCV_TRNG,             /* RISC-V TRNG */
    SEC_RNG_MAX
} sec_rng_type_t;

/* RNG 能力 */
typedef struct sec_rng_caps {
    sec_rng_type_t type;            /* RNG 类型 */
    const char *name;               /* 名称 */
    bool supported;                 /* 是否支持 */
    
    /* 质量指标 */
    u32 entropy_bits;               /* 每字节熵位数 */
    u32 min_latency_ns;             /* 最小延迟（纳秒） */
    u32 max_latency_ns;             /* 最大延迟（纳秒） */
    
    /* 统计 */
    u64 bytes_generated;            /* 已生成字节数 */
    u64 failures;                   /* 失败次数 */
} sec_rng_caps_t;

/* ========== 密钥管理 ========== */

/* 密钥类型 */
typedef enum sec_key_type {
    SEC_KEY_TYPE_SYMMETRIC = 0,     /* 对称密钥 */
    SEC_KEY_TYPE_ASYMMETRIC_PRIV,   /* 非对称私钥 */
    SEC_KEY_TYPE_ASYMMETRIC_PUB,    /* 非对称公钥 */
    SEC_KEY_TYPE_DERIVED,           /* 派生密钥 */
    SEC_KEY_TYPE_MAX
} sec_key_type_t;

/* 密钥算法 */
typedef enum sec_key_algo {
    SEC_KEY_ALGO_AES_128 = 0,
    SEC_KEY_ALGO_AES_192,
    SEC_KEY_ALGO_AES_256,
    SEC_KEY_ALGO_RSA_2048,
    SEC_KEY_ALGO_RSA_3072,
    SEC_KEY_ALGO_RSA_4096,
    SEC_KEY_ALGO_ECC_P256,
    SEC_KEY_ALGO_ECC_P384,
    SEC_KEY_ALGO_ECC_P521,
    SEC_KEY_ALGO_ED25519,
    SEC_KEY_ALGO_MAX
} sec_key_algo_t;

/* 密钥句柄 */
typedef struct sec_key_handle {
    u64 key_id;                     /* 密钥 ID */
    sec_key_type_t type;            /* 密钥类型 */
    sec_key_algo_t algo;            /* 密钥算法 */
    u32 key_bits;                   /* 密钥位数 */
    u32 flags;                      /* 密钥标志 */
    u64 created_time;               /* 创建时间 */
    u64 expires_time;               /* 过期时间 */
    u32 use_count;                  /* 使用次数 */
    u32 max_use_count;              /* 最大使用次数 */
} sec_key_handle_t;

/* 密钥标志 */
#define SEC_KEY_FLAG_EXPORTABLE    (1 << 0)   /* 可导出 */
#define SEC_KEY_FLAG_WRAPPABLE     (1 << 1)   /* 可包装 */
#define SEC_KEY_FLAG_DERIVE        (1 << 2)   /* 可派生 */
#define SEC_KEY_FLAG_SIGN          (1 << 3)   /* 可签名 */
#define SEC_KEY_FLAG_VERIFY        (1 << 4)   /* 可验证 */
#define SEC_KEY_FLAG_ENCRYPT       (1 << 5)   /* 可加密 */
#define SEC_KEY_FLAG_DECRYPT       (1 << 6)   /* 可解密 */

/* 密钥存储位置 */
typedef enum sec_key_storage {
    SEC_KEY_STORAGE_VOLATILE = 0,   /* 易失性内存 */
    SEC_KEY_STORAGE_SECURE_RAM,     /* 安全 RAM */
    SEC_KEY_STORAGE_HSM,            /* 硬件安全模块 */
    SEC_KEY_STORAGE_TEE,            /* TEE 存储 */
    SEC_KEY_STORAGE_MAX
} sec_key_storage_t;

/* ========== 安全状态 ========== */

typedef struct sec_state {
    /* 内存加密 */
    sec_mem_enc_caps_t mem_enc_caps[SEC_MEM_ENC_MAX];
    u32 active_mem_enc_type;
    u32 encrypted_region_count;
    
    /* TEE */
    sec_tee_caps_t tee_caps;
    u32 tee_session_count;
    
    /* CFI */
    sec_cfi_caps_t cfi_caps;
    
    /* RNG */
    sec_rng_caps_t rng_caps;
    
    /* 密钥管理 */
    u32 key_count;
    sec_key_storage_t default_storage;
} sec_state_t;

/* ========== 架构操作回调 ========== */

typedef struct sec_arch_ops {
    /* 内存加密 */
    bool (*mem_enc_supported)(sec_mem_enc_type_t type);
    bool (*mem_enc_enable)(sec_mem_enc_type_t type);
    void (*mem_enc_disable)(void);
    u64 (*mem_enc_create_region)(phys_addr_t base, u64 size, u32 flags);
    void (*mem_enc_destroy_region)(u64 region_id);
    
    /* TEE */
    bool (*tee_supported)(void);
    u64 (*tee_open_session)(u64 app_id);
    void (*tee_close_session)(u64 session_id);
    hic_status_t (*tee_invoke)(u64 session_id, u32 command, 
                               void *params, u32 param_count);
    
    /* CFI */
    bool (*cfi_supported)(sec_cfi_mode_t mode);
    void (*cfi_enable)(sec_cfi_mode_t mode);
    void (*cfi_disable)(void);
    
    /* RNG */
    bool (*rng_supported)(void);
    bool (*rng_get_bytes)(void *buffer, u32 count);
    u32 (*rng_get_entropy_bits)(void);
    
    /* 密钥管理 */
    u64 (*key_create)(sec_key_type_t type, sec_key_algo_t algo, 
                      const void *key_data, u32 key_size, u32 flags);
    void (*key_destroy)(u64 key_id);
    bool (*key_use)(u64 key_id, void *data, u32 size, bool encrypt);
} sec_arch_ops_t;

/* ========== API 函数声明 ========== */

/* 初始化 */
void sec_init(void);
void sec_shutdown(void);
void sec_register_arch_ops(const sec_arch_ops_t *ops);

/* 状态查询 */
const sec_state_t* sec_get_state(void);
void sec_print_info(void);

/* 内存加密 */
bool sec_mem_enc_supported(sec_mem_enc_type_t type);
const sec_mem_enc_caps_t* sec_mem_enc_get_caps(sec_mem_enc_type_t type);
hic_status_t sec_mem_enc_enable(sec_mem_enc_type_t type);
void sec_mem_enc_disable(void);
u64 sec_mem_enc_create_region(phys_addr_t base, u64 size, u32 flags);
void sec_mem_enc_destroy_region(u64 region_id);
bool sec_mem_enc_is_encrypted(phys_addr_t addr);

/* TEE */
bool sec_tee_supported(void);
const sec_tee_caps_t* sec_tee_get_caps(void);
u64 sec_tee_open_session(u64 app_id);
void sec_tee_close_session(u64 session_id);
hic_status_t sec_tee_invoke(u64 session_id, u32 command,
                           void *params, u32 param_count);

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
bool sec_rng_fill_buffer(void *buffer, u32 size);

/* 密钥管理 */
u64 sec_key_create(sec_key_type_t type, sec_key_algo_t algo,
                   const void *key_data, u32 key_size, u32 flags);
void sec_key_destroy(u64 key_id);
const sec_key_handle_t* sec_key_get_handle(u64 key_id);
bool sec_key_use(u64 key_id, void *data, u32 size, bool encrypt);
u64 sec_key_derive(u64 parent_key_id, const void *context, u32 context_size,
                   sec_key_algo_t target_algo);

/* 安全认证 */
hic_status_t sec_attestation_get_report(void *report, u32 *size,
                                        const void *nonce, u32 nonce_size);
bool sec_attestation_verify_report(const void *report, u32 size);

#endif /* HIC_KERNEL_SECURITY_EXT_H */
