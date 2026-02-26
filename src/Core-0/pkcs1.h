/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC PKCS#1 v2.1 RSASSA-PSS签名验证
 * 遵循RFC 8017标准
 * 用于模块签名验证
 */

#ifndef HIC_KERNEL_PKCS1_H
#define HIC_KERNEL_PKCS1_H

#include "types.h"

/* PKCS#1 v2.1 RSASSA-PSS参数 */
typedef struct {
    /* 哈希算法 */
    enum {
        PKCS1_HASH_SHA256,
        PKCS1_HASH_SHA384,
        PKCS1_HASH_SHA512,
    } hash_alg;
    
    /* MGF1（Mask Generation Function）参数 */
    enum {
        PKCS1_MGF1_SHA256,
        PKCS1_MGF1_SHA384,
        PKCS1_MGF1_SHA512,
    } mgf_alg;
    
    /* 盐长度（字节数） */
    u32 salt_length;
    
    /* 填充模式 */
    enum {
        PKCS1_PADDING_PSS,  /* RSASSA-PSS */
        PKCS1_PADDING_V1_5, /* PKCS#1 v1.5 */
    } padding;
} pkcs1_pss_params_t;

/* 大整数（RSA-3072需要384字节） */
#define PKCS1_MAX_MODULUS_SIZE  384  /* 3072位 = 384字节 */
#define PKCS1_MAX_EXPONENT_SIZE  4   /* 65537 */

typedef struct {
    u8 data[PKCS1_MAX_MODULUS_SIZE];
    u32 size;  /* 实际大小（字节） */
} pkcs1_bignum_t;

/* RSA公钥 */
typedef struct {
    pkcs1_bignum_t n;  /* 模数 */
    pkcs1_bignum_t e;  /* 公指数 */
    u32 bits;          /* 模数位数 */
} pkcs1_rsa_public_key_t;

/* PKCS#1验证接口 */

/**
 * 验证RSA签名（RSASSA-PSS）
 * 
 * @param message 消息数据
 * @param message_len 消息长度
 * @param signature 签名数据
 * @param signature_len 签名长度
 * @param public_key RSA公钥
 * @param params PSS参数
 * @return true=验证成功，false=验证失败
 */
bool pkcs1_verify_pss(const u8* message, u32 message_len,
                       const u8* signature, u32 signature_len,
                       const pkcs1_rsa_public_key_t* public_key,
                       const pkcs1_pss_params_t* params);

/**
 * 验证RSA签名（PKCS#1 v1.5）
 */
bool pkcs1_verify_v1_5(const u8* message, u32 message_len,
                        const u8* signature, u32 signature_len,
                        const pkcs1_rsa_public_key_t* public_key,
                        u32 hash_alg);

/* 大整数运算接口 */

/**
 * 大整数模幂运算：m = c^e mod n
 */
bool pkcs1_mod_exp(const pkcs1_bignum_t* c, const pkcs1_bignum_t* e,
                    const pkcs1_bignum_t* n, pkcs1_bignum_t* m);

/**
 * 大整数比较
 */
int pkcs1_bignum_cmp(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b);

/**
 * 大整数加法
 */
bool pkcs1_bignum_add(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                       pkcs1_bignum_t* result);

/**
 * 大整数减法
 */
bool pkcs1_bignum_sub(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                       pkcs1_bignum_t* result);

/* MGF1（Mask Generation Function） */

/**
 * MGF1：生成掩码
 */
bool pkcs1_mgf1(const u8* seed, u32 seed_len,
                 u8* mask, u32 mask_len,
                 u32 mgf_hash_alg);

#endif /* HIC_KERNEL_PKCS1_H */