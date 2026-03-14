/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 固化验证服务
 * 
 * 设计原则：
 * - 极小化：<500 行代码
 * - 固化：不依赖动态更新
 * - 信任锚点：作为静态信任链的起点
 * 
 * 信任链：
 * 内核信任根 → verifier → module_manager → 其他模块
 */

#ifndef HIC_VERIFIER_SERVICE_H
#define HIC_VERIFIER_SERVICE_H

#include <stdint.h>
#include <stddef.h>

/* 验证状态 */
typedef enum {
    VERIFY_OK = 0,
    VERIFY_ERR_INVALID_PARAM,
    VERIFY_ERR_HASH_MISMATCH,
    VERIFY_ERR_SIGNATURE_INVALID,
    VERIFY_ERR_CERT_CHAIN_INVALID,
    VERIFY_ERR_NO_TRUSTED_ROOT,
    VERIFY_ERR_NOT_SUPPORTED,
} verify_status_t;

/* 签名算法类型 */
typedef enum {
    SIGN_ALG_SHA384_HASH = 0,    /* 仅哈希校验（开发阶段） */
    SIGN_ALG_RSA3072_PSS = 1,    /* RSA-3072 PSS */
    SIGN_ALG_ED25519 = 2,        /* Ed25519 */
} sign_algorithm_t;

/* 证书结构（简化） */
typedef struct {
    uint8_t  issuer[32];         /* 颁发者标识 */
    uint8_t  subject[32];        /* 主题标识 */
    uint8_t  public_key[384];    /* 公钥（最大 RSA-3072） */
    uint32_t public_key_len;     /* 公钥长度 */
    uint32_t valid_from;         /* 有效期起始 */
    uint32_t valid_until;        /* 有效期结束 */
    uint8_t  signature[512];     /* 签名 */
    uint32_t signature_len;      /* 签名长度 */
    sign_algorithm_t sign_alg;   /* 签名算法 */
} verifier_cert_t;

/* 模块签名头 */
typedef struct {
    uint32_t magic;              /* 魔数: 0x48495347 ("HISG") */
    uint32_t version;            /* 版本 */
    uint32_t cert_count;         /* 证书数量 */
    uint32_t sign_alg;           /* 签名算法 */
    uint8_t  cert_hash[48];      /* 证书链哈希 (SHA-384) */
    uint8_t  module_hash[48];    /* 模块哈希 (SHA-384) */
    uint8_t  signature[512];     /* 签名 */
    uint32_t signature_len;      /* 签名长度 */
} module_sign_header_t;

/* 验证结果 */
typedef struct {
    verify_status_t status;
    uint8_t computed_hash[48];   /* 计算得到的哈希 */
    const char *error_msg;       /* 错误信息 */
} verify_result_t;

/* ==================== 服务接口 ==================== */

/**
 * @brief 初始化验证服务
 */
int verifier_init(void);

/**
 * @brief 启动验证服务
 */
int verifier_start(void);

/**
 * @brief 停止验证服务
 */
int verifier_stop(void);

/**
 * @brief 清理验证服务
 */
int verifier_cleanup(void);

/* ==================== 验证接口 ==================== */

/**
 * @brief 验证模块签名
 * 
 * @param module_data 模块数据
 * @param module_size 模块大小
 * @param sign_header 签名头
 * @param result 验证结果
 * @return 状态码
 */
verify_status_t verifier_verify_module(
    const void *module_data,
    size_t module_size,
    const module_sign_header_t *sign_header,
    verify_result_t *result
);

/**
 * @brief 验证证书链
 * 
 * @param certs 证书数组
 * @param cert_count 证书数量
 * @return 状态码
 */
verify_status_t verifier_verify_cert_chain(
    const verifier_cert_t *certs,
    uint32_t cert_count
);

/**
 * @brief 计算哈希
 * 
 * @param data 数据
 * @param size 大小
 * @param hash 输出哈希（48字节）
 */
void verifier_compute_hash(const void *data, size_t size, uint8_t hash[48]);

/**
 * @brief 获取信任根公钥
 * 
 * @param pubkey 输出公钥
 * @param max_len 最大长度
 * @return 实际长度
 */
size_t verifier_get_trusted_root(uint8_t *pubkey, size_t max_len);

#endif /* HIC_VERIFIER_SERVICE_H */
