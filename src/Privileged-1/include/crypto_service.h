/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC加密服务接口
 * 
 * 提供完整的密码学操作服务，包括：
 * - AES 加密/解密 (ECB/CBC/CTR/GCM)
 * - SHA-256/384/512 哈希计算
 * - HMAC 消息认证
 * - RSA-PSS/v1.5 签名验证
 * - PBKDF2/HKDF 密钥派生
 */

#ifndef HIC_CRYPTO_SERVICE_H
#define HIC_CRYPTO_SERVICE_H

#include "stdint.h"
#include "stddef.h"

/* ========== 服务端点定义 ========== */

#define CRYPTO_ENDPOINT_BASE        0x6700

#define CRYPTO_ENDPOINT_AES         (CRYPTO_ENDPOINT_BASE + 0)   /* AES加密解密 */
#define CRYPTO_ENDPOINT_SHA         (CRYPTO_ENDPOINT_BASE + 1)   /* SHA哈希 */
#define CRYPTO_ENDPOINT_HMAC        (CRYPTO_ENDPOINT_BASE + 2)   /* HMAC认证 */
#define CRYPTO_ENDPOINT_RSA_PSS     (CRYPTO_ENDPOINT_BASE + 3)   /* RSA-PSS验证 */
#define CRYPTO_ENDPOINT_RSA_V15     (CRYPTO_ENDPOINT_BASE + 4)   /* RSA v1.5验证 */
#define CRYPTO_ENDPOINT_KDF         (CRYPTO_ENDPOINT_BASE + 5)   /* 密钥派生 */

/* ========== 常量定义 ========== */

#define AES_BLOCK_SIZE          16      /* AES 块大小 */
#define AES_MAX_KEY_SIZE        32      /* AES-256 密钥大小 */
#define AES_MAX_ROUND_KEYS      60      /* 256-bit: 14轮 * 4 + 4 */

#define SHA256_DIGEST_SIZE      32      /* SHA-256 输出长度 */
#define SHA256_BLOCK_SIZE       64      /* SHA-256 块大小 */
#define SHA384_DIGEST_SIZE      48      /* SHA-384 输出长度 */
#define SHA384_BLOCK_SIZE       128     /* SHA-384 块大小 */
#define SHA512_DIGEST_SIZE      64      /* SHA-512 输出长度 */
#define SHA512_BLOCK_SIZE       128     /* SHA-512 块大小 */

#define RSA_MAX_KEY_SIZE        512     /* RSA-4096 最大密钥大小 */
#define RSA_MIN_KEY_SIZE        128     /* RSA-1024 最小密钥大小 */

/* ========== 错误码定义 ========== */

#define CRYPTO_SUCCESS              0
#define CRYPTO_ERR_INVALID_PARAM    (-1)
#define CRYPTO_ERR_INVALID_KEY      (-2)
#define CRYPTO_ERR_INVALID_SIG      (-3)
#define CRYPTO_ERR_SIG_MISMATCH     (-4)
#define CRYPTO_ERR_HASH_FAILED      (-5)
#define CRYPTO_ERR_NOT_SUPPORTED    (-6)
#define CRYPTO_ERR_ENCRYPT_FAILED   (-7)
#define CRYPTO_ERR_DECRYPT_FAILED   (-8)

/* ========== AES 模式 ========== */

#define AES_MODE_ECB    0   /* ECB 模式 */
#define AES_MODE_CBC    1   /* CBC 模式 */
#define AES_MODE_CTR    2   /* CTR 模式 */
#define AES_MODE_GCM    3   /* GCM 模式 */

/* ========== SHA 算法标识 ========== */

#define SHA_ALGO_256    0   /* SHA-256 */
#define SHA_ALGO_384    1   /* SHA-384 */
#define SHA_ALGO_512    2   /* SHA-512 */

/* ========== AES 接口 ========== */

/**
 * AES 上下文结构
 */
typedef struct aes_ctx {
    uint32_t    round_keys[AES_MAX_ROUND_KEYS]; /* 轮密钥 */
    uint8_t     iv[AES_BLOCK_SIZE];             /* 初始向量 */
    uint32_t    key_bits;                       /* 密钥位数 (128/192/256) */
    uint32_t    nr;                             /* 轮数 (10/12/14) */
    uint8_t     mode;                           /* 加密模式 */
    uint8_t     encrypt;                        /* 1=加密, 0=解密 */
} aes_ctx_t;

/**
 * AES 请求
 */
typedef struct aes_request {
    const uint8_t    *key;              /* 密钥 */
    uint32_t          key_bits;         /* 密钥位数 (128/192/256) */
    const uint8_t    *iv;               /* 初始向量 (CBC/CTR/GCM) */
    uint8_t           mode;             /* 加密模式 */
    uint8_t           encrypt;          /* 1=加密, 0=解密 */
    const uint8_t    *input;            /* 输入数据 */
    size_t            input_len;        /* 输入长度 */
    /* GCM 专用 */
    const uint8_t    *aad;              /* 附加认证数据 */
    size_t            aad_len;          /* AAD 长度 */
} aes_request_t;

/**
 * AES 响应
 */
typedef struct aes_response {
    int32_t           status;           /* 状态码 */
    uint8_t          *output;           /* 输出数据 */
    size_t            output_len;       /* 输出长度 */
    uint8_t           tag[16];          /* GCM 认证标签 */
} aes_response_t;

/* ========== SHA 接口 ========== */

/**
 * SHA 上下文结构 (通用)
 */
typedef struct sha_ctx {
    uint8_t     state[64];              /* 状态变量 (最大64字节) */
    uint64_t    total_bytes;            /* 已处理字节数 */
    uint8_t     buffer[128];            /* 输入缓冲区 */
    uint32_t    buffer_len;             /* 缓冲区长度 */
    uint8_t     algo;                   /* 算法标识 */
    uint8_t     digest_size;            /* 摘要大小 */
    uint8_t     block_size;             /* 块大小 */
    uint8_t     finalized;              /* 是否已最终化 */
} sha_ctx_t;

/**
 * SHA-384 上下文 (兼容旧接口)
 */
typedef struct sha384_ctx {
    uint64_t    state[8];               /* 状态变量 */
    uint64_t    count[2];               /* 位计数 */
    uint8_t     buffer[SHA384_BLOCK_SIZE]; /* 输入缓冲区 */
    uint32_t    buffer_len;             /* 缓冲区长度 */
} sha384_ctx_t;

/**
 * SHA 请求
 */
typedef struct sha_request {
    const uint8_t    *data;             /* 输入数据 */
    size_t            data_len;         /* 数据长度 */
    uint8_t           algo;             /* 算法: SHA_ALGO_* */
    uint8_t           finalize;         /* 是否完成 */
} sha_request_t;

/**
 * SHA 响应
 */
typedef struct sha_response {
    int32_t           status;           /* 状态码 */
    uint8_t           digest[SHA512_DIGEST_SIZE]; /* 哈希输出 */
} sha_response_t;

/* ========== HMAC 接口 ========== */

/**
 * HMAC 上下文
 */
typedef struct hmac_ctx {
    sha_ctx_t   sha_ctx;                /* SHA 上下文 */
    uint8_t     key[128];               /* 密钥 (最大块大小) */
    uint32_t    key_len;                /* 密钥长度 */
    uint8_t     algo;                   /* SHA 算法 */
} hmac_ctx_t;

/**
 * HMAC 请求
 */
typedef struct hmac_request {
    const uint8_t    *key;              /* 密钥 */
    size_t            key_len;          /* 密钥长度 */
    const uint8_t    *data;             /* 输入数据 */
    size_t            data_len;         /* 数据长度 */
    uint8_t           algo;             /* SHA 算法 */
} hmac_request_t;

/**
 * HMAC 响应
 */
typedef struct hmac_response {
    int32_t           status;           /* 状态码 */
    uint8_t           mac[SHA512_DIGEST_SIZE]; /* MAC 输出 */
} hmac_response_t;

/* ========== RSA 接口 ========== */

/**
 * RSA 公钥结构
 */
typedef struct rsa_public_key {
    uint32_t    key_size;               /* 密钥大小（字节） */
    uint32_t    exponent;               /* 公钥指数 */
    uint8_t     modulus[RSA_MAX_KEY_SIZE]; /* 模数 n */
} rsa_public_key_t;

/**
 * RSA-PSS 签名验证请求
 */
typedef struct rsa_pss_verify_request {
    const uint8_t        *message;      /* 原始消息 */
    size_t                message_len;  /* 消息长度 */
    const uint8_t        *signature;    /* 签名值 */
    size_t                sig_len;      /* 签名长度 */
    const rsa_public_key_t *public_key; /* 公钥 */
    uint32_t              salt_len;     /* 盐值长度 */
} rsa_pss_verify_request_t;

/**
 * RSA-PSS 签名验证响应
 */
typedef struct rsa_pss_verify_response {
    int32_t               status;       /* 状态码 */
} rsa_pss_verify_response_t;

/**
 * RSA v1.5 签名验证请求
 */
typedef struct rsa_v15_verify_request {
    const uint8_t        *message;      /* 原始消息 */
    size_t                message_len;  /* 消息长度 */
    const uint8_t        *signature;    /* 签名值 */
    size_t                sig_len;      /* 签名长度 */
    const rsa_public_key_t *public_key; /* 公钥 */
    uint8_t               hash_algo;    /* 哈希算法 */
} rsa_v15_verify_request_t;

/**
 * RSA v1.5 签名验证响应
 */
typedef struct rsa_v15_verify_response {
    int32_t               status;       /* 状态码 */
} rsa_v15_verify_response_t;

/* ========== 密钥派生接口 ========== */

/**
 * KDF 请求
 */
typedef struct kdf_request {
    uint8_t           algo;             /* 算法: 0=PBKDF2, 1=HKDF */
    const uint8_t    *password;         /* 密码/输入密钥材料 */
    size_t            password_len;     /* 密码长度 */
    const uint8_t    *salt;             /* 盐值 */
    size_t            salt_len;         /* 盐值长度 */
    const uint8_t    *info;             /* HKDF info (可选) */
    size_t            info_len;         /* info 长度 */
    uint32_t          iterations;       /* PBKDF2 迭代次数 */
    size_t            key_len;          /* 输出密钥长度 */
} kdf_request_t;

/**
 * KDF 响应
 */
typedef struct kdf_response {
    int32_t           status;           /* 状态码 */
    uint8_t           key[64];          /* 输出密钥 */
    size_t            key_len;          /* 实际密钥长度 */
} kdf_response_t;

/* ========== 公开函数接口 ========== */

/* AES */
int aes_init(aes_ctx_t *ctx, const uint8_t *key, uint32_t key_bits, 
             uint8_t mode, uint8_t encrypt);
int aes_process(aes_ctx_t *ctx, const uint8_t *input, uint8_t *output, size_t len);
int aes_gcm_encrypt(const uint8_t *key, uint32_t key_bits, const uint8_t *iv,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *input, uint8_t *output, size_t len,
                    uint8_t *tag);
int aes_gcm_decrypt(const uint8_t *key, uint32_t key_bits, const uint8_t *iv,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *input, uint8_t *output, size_t len,
                    const uint8_t *tag);

/* SHA */
int sha_init(sha_ctx_t *ctx, uint8_t algo);
int sha_update(sha_ctx_t *ctx, const uint8_t *data, size_t len);
int sha_final(sha_ctx_t *ctx, uint8_t *digest);
int sha_hash(uint8_t algo, const uint8_t *data, size_t len, uint8_t *digest);

/* SHA-384 兼容接口 */
void sha384_init(sha384_ctx_t *ctx);
void sha384_update(sha384_ctx_t *ctx, const uint8_t *data, size_t len);
void sha384_final(sha384_ctx_t *ctx, uint8_t digest[SHA384_DIGEST_SIZE]);
int sha384_hash(const uint8_t *data, size_t len, uint8_t digest[SHA384_DIGEST_SIZE]);

/* HMAC */
int hmac_init(hmac_ctx_t *ctx, uint8_t algo, const uint8_t *key, size_t key_len);
int hmac_update(hmac_ctx_t *ctx, const uint8_t *data, size_t len);
int hmac_final(hmac_ctx_t *ctx, uint8_t *mac);
int hmac_hash(uint8_t algo, const uint8_t *key, size_t key_len,
              const uint8_t *data, size_t data_len, uint8_t *mac);

/* RSA */
int rsa_verify_pss(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint32_t salt_len);
int rsa_verify_v15(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint8_t hash_algo);

/* KDF */
int pbkdf2_derive(const uint8_t *password, size_t password_len,
                  const uint8_t *salt, size_t salt_len,
                  uint8_t *key, size_t key_len, uint32_t iterations);
int hkdf_derive(const uint8_t *ikm, size_t ikm_len,
                const uint8_t *salt, size_t salt_len,
                const uint8_t *info, size_t info_len,
                uint8_t *okm, size_t okm_len);

/* 辅助函数 */
int constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len);
void secure_memzero(void *ptr, size_t len);

#endif /* HIC_CRYPTO_SERVICE_H */