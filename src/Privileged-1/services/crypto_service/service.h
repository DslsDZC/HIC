/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 密码学服务
 * 提供哈希计算和签名验证功能
 */

#ifndef CRYPTO_SERVICE_H
#define CRYPTO_SERVICE_H

#include "../../Core-0/types.h"

/* SHA-384哈希大小 */
#define SHA384_DIGEST_SIZE 48

/* RSA密钥大小 */
#define RSA_2048_BITS 2048
#define RSA_3072_BITS 3072
#define RSA_4096_BITS 4096

/* 大整数最大模数大小（512字节 = 4096位） */
#define PKCS1_MAX_MODULUS_SIZE 512

/* 大整数结构 */
typedef struct pkcs1_bignum {
    u8 data[PKCS1_MAX_MODULUS_SIZE];
    u32 size;
} pkcs1_bignum_t;

/* RSA公钥 */
typedef struct pkcs1_rsa_public_key {
    pkcs1_bignum_t n;  /* 模数 */
    pkcs1_bignum_t e;  /* 公指数 */
    u32 bits;          /* 密钥位数 */
} pkcs1_rsa_public_key_t;

/* PSS参数 */
typedef struct pkcs1_pss_params {
    u32 hash_alg;          /* 哈希算法 */
    u32 salt_length;       /* 盐长度 */
} pkcs1_pss_params_t;

/* ============= 哈希函数 ============= */

/**
 * 计算SHA-384哈希
 * @param data 输入数据
 * @param len 数据长度
 * @param hash 输出哈希（48字节）
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t crypto_sha384(const u8* data, u32 len, u8* hash);

/* ============= RSA签名验证 ============= */

/**
 * 验证RSA签名（PKCS#1 v2.1 RSASSA-PSS）
 * @param message 消息
 * @param message_len 消息长度
 * @param signature 签名
 * @param signature_len 签名长度
 * @param public_key 公钥
 * @param params PSS参数
 * @return 验证成功返回HIC_SUCCESS
 */
hic_status_t crypto_rsa_verify_pss(const u8* message, u32 message_len,
                                    const u8* signature, u32 signature_len,
                                    const pkcs1_rsa_public_key_t* public_key,
                                    const pkcs1_pss_params_t* params);

/**
 * 验证RSA签名（PKCS#1 v1.5）
 * @param message 消息
 * @param message_len 消息长度
 * @param signature 签名
 * @param signature_len 签名长度
 * @param public_key 公钥
 * @param hash_alg 哈希算法
 * @return 验证成功返回HIC_SUCCESS
 */
hic_status_t crypto_rsa_verify_v1_5(const u8* message, u32 message_len,
                                     const u8* signature, u32 signature_len,
                                     const pkcs1_rsa_public_key_t* public_key,
                                     u32 hash_alg);

/* ============= MGF1 ============= */

/**
 * MGF1掩码生成函数
 * @param seed 种子
 * @param seed_len 种子长度
 * @param mask 输出掩码
 * @param mask_len 掩码长度
 * @param mgf_hash_alg 哈希算法
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t crypto_mgf1(const u8* seed, u32 seed_len,
                         u8* mask, u32 mask_len,
                         u32 mgf_hash_alg);

/* ============= 服务API ============= */

/**
 * 初始化密码学服务
 */
hic_status_t crypto_service_init(void);

/**
 * 获取密码学服务信息
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回HIC_SUCCESS
 */
hic_status_t crypto_service_get_info(char* buffer, u32 buffer_size);

#endif /* CRYPTO_SERVICE_H */