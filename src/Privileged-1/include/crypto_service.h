/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC加密服务接口
 * 
 * 提供密码学操作服务，包括：
 * - SHA-384 哈希计算
 * - RSA-PSS 签名验证
 * - RSA v1.5 签名验证
 * - MGF1 掩码生成
 */

#ifndef HIC_CRYPTO_SERVICE_H
#define HIC_CRYPTO_SERVICE_H

#include "stdint.h"
#include "stddef.h"

/* ========== 服务端点定义 ========== */

#define CRYPTO_ENDPOINT_BASE        0x6700

#define CRYPTO_ENDPOINT_SHA384      (CRYPTO_ENDPOINT_BASE + 0)   /* SHA-384哈希 */
#define CRYPTO_ENDPOINT_RSA_PSS     (CRYPTO_ENDPOINT_BASE + 1)   /* RSA-PSS验证 */
#define CRYPTO_ENDPOINT_RSA_V15     (CRYPTO_ENDPOINT_BASE + 2)   /* RSA v1.5验证 */
#define CRYPTO_ENDPOINT_MGF1        (CRYPTO_ENDPOINT_BASE + 3)   /* MGF1掩码生成 */

/* ========== 常量定义 ========== */

#define SHA384_DIGEST_SIZE      48      /* SHA-384 输出长度 (384位 = 48字节) */
#define SHA384_BLOCK_SIZE       128     /* SHA-384 块大小 */
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

/* ========== SHA-384 接口 ========== */

/**
 * SHA-384 上下文结构
 */
typedef struct sha384_ctx {
    uint64_t    state[8];               /* 状态变量 */
    uint64_t    count[2];               /* 位计数 */
    uint8_t     buffer[SHA384_BLOCK_SIZE]; /* 输入缓冲区 */
    uint32_t    buffer_len;             /* 缓冲区长度 */
} sha384_ctx_t;

/**
 * SHA-384 请求
 */
typedef struct sha384_request {
    const uint8_t    *data;             /* 输入数据 */
    size_t            data_len;         /* 数据长度 */
    uint8_t           finalize;         /* 是否完成（输出最终哈希） */
} sha384_request_t;

/**
 * SHA-384 响应
 */
typedef struct sha384_response {
    int32_t           status;           /* 状态码 */
    uint8_t           digest[SHA384_DIGEST_SIZE]; /* 哈希输出 */
} sha384_response_t;

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
    uint8_t               hash_algo;    /* 哈希算法: 0=SHA-256, 1=SHA-384, 2=SHA-512 */
} rsa_v15_verify_request_t;

/**
 * RSA v1.5 签名验证响应
 */
typedef struct rsa_v15_verify_response {
    int32_t               status;       /* 状态码 */
} rsa_v15_verify_response_t;

/* ========== MGF1 接口 ========== */

/**
 * MGF1 掩码生成请求
 */
typedef struct mgf1_request {
    const uint8_t    *seed;             /* 种子值 */
    size_t            seed_len;         /* 种子长度 */
    size_t            mask_len;         /* 期望掩码长度 */
    uint8_t           hash_algo;        /* 哈希算法: 0=SHA-256, 1=SHA-384 */
} mgf1_request_t;

/**
 * MGF1 掩码生成响应
 */
typedef struct mgf1_response {
    int32_t           status;           /* 状态码 */
    uint8_t          *mask;             /* 输出掩码 */
    size_t            mask_len;         /* 实际掩码长度 */
} mgf1_response_t;

/* ========== 公开函数接口 ========== */

/**
 * 初始化 SHA-384 上下文
 */
void sha384_init(sha384_ctx_t *ctx);

/**
 * 更新 SHA-384 哈希
 */
void sha384_update(sha384_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * 完成 SHA-384 哈希
 */
void sha384_final(sha384_ctx_t *ctx, uint8_t digest[SHA384_DIGEST_SIZE]);

/**
 * 单次计算 SHA-384 哈希
 */
int sha384_hash(const uint8_t *data, size_t len, uint8_t digest[SHA384_DIGEST_SIZE]);

/**
 * RSA-PSS 签名验证
 */
int rsa_verify_pss(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint32_t salt_len);

/**
 * RSA v1.5 签名验证
 */
int rsa_verify_v15(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint8_t hash_algo);

/**
 * MGF1 掩码生成函数
 */
int mgf1_generate(const uint8_t *seed, size_t seed_len,
                  uint8_t *mask, size_t mask_len,
                  uint8_t hash_algo);

/**
 * 常量时间内存比较（防止时序攻击）
 */
int constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len);

#endif /* HIC_CRYPTO_SERVICE_H */
