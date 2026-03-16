/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 密码学加速抽象层 (Cryptography Acceleration Abstraction Layer)
 * 
 * 提供跨平台的密码学原语抽象接口：
 * 1. 统一密码学原语：抽象硬件加速的 AES、SHA、 RSA 等操作
 * 2. 随机数生成：跨平台的硬件 RNG 接口
 * 3. 密钥管理：安全的密钥存储与使用抽象
 */

#ifndef HIC_KERNEL_CRYPTO_ACCEL_H
#define HIC_KERNEL_CRYPTO_ACCEL_H

#include "types.h"

#include "security_ext.h"

/* ========== 算法标识 ========== */

/* 对称算法 */
typedef enum crypto_algo {
    CRYPTO_ALGO_AES = 0,        /* AES */
    CRYPTO_ALGO_SHA256,          /* SHA-256 */
    CRYPTO_ALGO_SHA384,          /* SHA-384 */
    CRYPTO_ALGO_SHA512,          /* SHA-512 */
    CRYPTO_ALGO_SHA3_256,        /* SHA3-256 */
    CRYPTO_ALGO_MD5,            /* MD5 */
    CRYPTO_ALGO_RSA,            /* RSA */
    CRYPTO_ALGO_ECDSA,           /* ECDSA */
    CRYPTO_ALGO_ED25519,        /* Ed25519 */
    CRYPTO_ALGO_CHACHA20,        /* ChaCha20 */
    CRYPTO_ALGO_POLY1305,        /* Poly1305 */
    CRYPTO_ALGO_MAX
} crypto_algo_t;

/* 操作模式 */
typedef enum crypto_op {
    CRYPTO_OP_ENCRYPT = 0,      /* 加密 */
    CRYPTO_OP_DECRYPT,          /* 解密 */
    CRYPTO_OP_SIGN,              /* 签名 */
    CRYPTO_OP_VERIFY,             /* 验证 */
    CRYPTO_OP_HASH,              /* 哈希 */
    CRYPTO_OP_KEYGEN,            /* 密钥生成 */
    CRYPTO_OP_KEYDERIVE,         /* 密钥派生 */
    CRYPTO_OP_MAX
} crypto_op_t;

/* ========== 硬件能力 ========== */

/* 加速能力信息 */
typedef struct crypto_accel_caps {
    bool supported;                 /* 是否支持 */
    bool enabled;                  /* 是否启用 */
    
    /* 支持的算法 */
    u32 supported_algos;            /* 支持的算法位掩码 */
    
    /* 性能参数 */
    u32 aes_latency_cycles;        /* AES 延迟（周期数） */
    u32 sha256_latency_cycles;      /* SHA-256 延迟 */
    u32 rsa_latency_cycles;         /* RSA 延迟 */
    u64 throughput_mbps;            /* 吞吐量 (Mbps) */
    
    /* 特殊能力 */
    bool supports_aes_gcm;          /* 支持 AES-GCM */
    bool supports_aes_xts;          /* 支持 AES-XTS */
    bool supports_sha_ext;          /* 支持 SHA 扩展 */
    bool supports_nist_pcurves;     /* 支持 NIST P-curves */
    bool supports_ecc;              /* 支持 ECC 加速 */
} crypto_accel_caps_t;

/* ========== 操作上下文 ========== */

/* 操作上下文 */
typedef struct crypto_op_ctx {
    crypto_algo_t algo;            /* 算法 */
    crypto_op_t op;                /* 操作类型 */
    const void *input;             /* 输入数据 */
    u32 input_size;                /* 输入大小 */
    void *output;                  /* 输出数据 */
    u32 output_size;               /* 输出大小 */
    u64 flags;                     /* 操作标志 */
    u64 result;                    /* 操作结果 */
} crypto_op_ctx_t;

/* 操作标志 */
#define CRYPTO_FLAG_HW_ACCEL    (1 << 0)   /* 强制硬件加速 */
#define CRYPTO_FLAG_ASYNC        (1 << 1)   /* 异步操作 */
#define CRYPTO_FLAG_KEY_PRESENT  (1 << 2)   /* 密钥已提供 */
#define CRYPTO_FLAG_IV_PRESENT    (1 << 3)   /* IV 已提供 */

/* ========== AES 操作 ========== */

/* AES 上下文 */
typedef struct crypto_aes_ctx {
    u8 key[32];                     /* 密钥 */
    u8 iv[16];                      /* 初始向量 */
    u32 key_size;                   /* 密钥大小（位） */
    u32 mode;                       /* 加密模式 */
    bool encrypt;                   /* 加密/解密 */
} crypto_aes_ctx_t;

/* AES 模式 */
#define CRYPTO_AES_MODE_ECB    0   /* ECB */
#define CRYPTO_AES_MODE_CBC    1   /* CBC */
#define CRYPTO_AES_MODE_CTR    2   /* CTR */
#define CRYPTO_AES_MODE_GCM    3   /* GCM */
#define CRYPTO_AES_MODE_XTS    4   /* XTS */

/* ========== SHA 操作 ========== */

/* SHA 上下文 */
typedef struct crypto_sha_ctx {
    crypto_algo_t algo;            /* SHA 算法 */
    u8 state[256];                  /* 算法状态 */
    u32 state_size;                 /* 状态大小 */
    u64 total_bytes;                /* 已处理字节数 */
    bool finalized;                 /* 是否已最终化 */
} crypto_sha_ctx_t;

/* ========== RSA 操作 ========== */

/* RSA 密钥 */
typedef struct crypto_rsa_key {
    u8 *modulus;                    /* 模数 n */
    u8 *exponent;                   /* 指数 e/d */
    u8 *private_exponent;           /* 私钥指数 d (仅私钥) */
    u32 key_bits;                   /* 密钥位数 */
    bool is_private;                /* 是否私钥 */
} crypto_rsa_key_t;

/* ========== 架构回调 ========== */

typedef struct crypto_arch_ops {
    /* 能力查询 */
    void (*get_caps)(crypto_accel_caps_t *caps);
    
    /* 对称加密 */
    bool (*aes_encrypt)(const crypto_aes_ctx_t *ctx,
                        const void *input, void *output, u32 size);
    bool (*aes_decrypt)(const crypto_aes_ctx_t *ctx,
                        const void *input, void *output, u32 size);
    
    /* 哈希 */
    bool (*sha_init)(crypto_sha_ctx_t *ctx, crypto_algo_t algo);
    bool (*sha_update)(crypto_sha_ctx_t *ctx, const void *data, u32 size);
    bool (*sha_final)(crypto_sha_ctx_t *ctx, void *digest);
    
    /* 非对称加密 */
    bool (*rsa_encrypt)(const crypto_rsa_key_t *key,
                        const void *input, void *output, u32 size);
    bool (*rsa_decrypt)(const crypto_rsa_key_t *key,
                        const void *input, void *output, u32 size);
    
    /* 養钥生成 */
    bool (*keygen)(crypto_algo_t algo, void *key, u32 key_size);
    bool (*kdf)(crypto_algo_t algo,
                const void *password, u32 password_len,
                const void *salt, u32 salt_len,
                void *key, u32 key_size,
                u32 iterations);
} crypto_arch_ops_t;

/* ========== API 函数声明 ========== */

/* 初始化 */
void crypto_accel_init(void);
void crypto_accel_shutdown(void);
void crypto_accel_register_ops(const crypto_arch_ops_t *ops);

/* 能力查询 */
const crypto_accel_caps_t* crypto_accel_get_caps(void);
bool crypto_accel_algo_supported(crypto_algo_t algo);
bool crypto_accel_enabled(void);

/* AES 操作 */
bool crypto_aes_encrypt(const void *key, u32 key_bits,
                        const void *iv, u32 mode,
                        const void *input, void *output, u32 size);
bool crypto_aes_decrypt(const void *key, u32 key_bits,
                        const void *iv, u32 mode,
                        const void *input, void *output, u32 size);
bool crypto_aes_gcm_encrypt(const void *key, u32 key_bits,
                            const void *iv,
                            const void *aad, u32 aad_size,
                            const void *input, void *output, u32 size,
                            void *tag);
bool crypto_aes_gcm_decrypt(const void *key, u32 key_bits,
                            const void *iv,
                            const void *aad, u32 aad_size,
                            const void *input, void *output, u32 size,
                            const void *tag);

bool crypto_aes_gcm_verify(const void *expected_tag, const void *actual_tag);

/* SHA 操作 */
bool crypto_sha_init(crypto_sha_ctx_t *ctx, crypto_algo_t algo);
bool crypto_sha_update(crypto_sha_ctx_t *ctx, const void *data, u32 size);
bool crypto_sha_final(crypto_sha_ctx_t *ctx, void *digest);
bool crypto_sha_hash(crypto_algo_t algo, const void *data, u32 size, void *digest);

/* HMAC 操作 */
bool crypto_hmac_init(crypto_sha_ctx_t *ctx, crypto_algo_t algo,
                      const void *key, u32 key_size);
bool crypto_hmac_update(crypto_sha_ctx_t *ctx, const void *data, u32 size);
bool crypto_hmac_final(crypto_sha_ctx_t *ctx, void *mac);

/* 密钥派生 */
bool crypto_pbkdf2(const void *password, u32 password_len,
                  const void *salt, u32 salt_len,
                  void *key, u32 key_size,
                  u32 iterations);
bool crypto_hkdf(const void *ikm, u32 ikm_len,
                const void *salt, u32 salt_len,
                const void *info, u32 info_len,
                void *okm, u32 okm_len);

/* 避免时序攻击 */
bool crypto_constant_time_compare(const void *a, const void *b, u32 size);
void crypto_secure_zero(void *ptr, u32 size);

/* 性能统计 */
void crypto_accel_get_stats(u64 *operations, u64 *bytes_processed);
void crypto_accel_print_info(void);

#endif /* HIC_KERNEL_CRYPTO_ACCEL_H */
