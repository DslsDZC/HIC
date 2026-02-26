/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 密码学服务实现
 * 包含SHA-384和RSA签名验证功能
 */

#include "service.h"
#include "../include/service_api.h"
#include "../../Core-0/lib/mem.h"
#include "../../Core-0/lib/console.h"

/* SHA-384常量 */
#define SHA384_DIGEST_SIZE 48
#define SHA384_BLOCK_SIZE 128

/* SHA-384初始哈希值 */
static const u64 SHA384_INITIAL_HASH[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
    0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
    0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
};

/* SHA-384常量K */
static const u64 SHA384_K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* SHA-384辅助宏 */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHR64(x, n) ((x) >> (n))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define EP1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SIG0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ SHR64(x, 7))
#define SIG1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ SHR64(x, 6))

/* SHA-384实现 */
static void sha384_process_block(const u8* block, u64* h) {
    u64 w[80];
    
    /* 准备消息调度 */
    for (u32 i = 0; i < 16; i++) {
        w[i] = ((u64)block[i * 8] << 56) |
               ((u64)block[i * 8 + 1] << 48) |
               ((u64)block[i * 8 + 2] << 40) |
               ((u64)block[i * 8 + 3] << 32) |
               ((u64)block[i * 8 + 4] << 24) |
               ((u64)block[i * 8 + 5] << 16) |
               ((u64)block[i * 8 + 6] << 8) |
               (u64)block[i * 8 + 7];
    }
    
    for (u32 i = 16; i < 80; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }
    
    /* 压缩 */
    u64 a = h[0], b = h[1], c = h[2], d = h[3];
    u64 e = h[4], f = h[5], g = h[6], hh = h[7];
    
    for (u32 i = 0; i < 80; i++) {
        u64 t1 = hh + EP1(e) + CH(e, f, g) + SHA384_K[i] + w[i];
        u64 t2 = EP0(a) + MAJ(a, b, c);
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

/**
 * 计算SHA-384哈希
 */
hic_status_t crypto_sha384(const u8* data, u32 len, u8* hash) {
    if (!data || !hash) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u64 h[8];
    memcopy(h, SHA384_INITIAL_HASH, sizeof(h));
    
    u8 block[SHA384_BLOCK_SIZE];
    u32 offset = 0;
    
    /* 处理完整块 */
    while (len >= SHA384_BLOCK_SIZE) {
        sha384_process_block(data + offset, h);
        offset += SHA384_BLOCK_SIZE;
        len -= SHA384_BLOCK_SIZE;
    }
    
    /* 处理剩余数据 */
    memzero(block, SHA384_BLOCK_SIZE);
    memcopy(block, data + offset, len);
    block[len] = 0x80;
    
    /* 添加长度（大端序，128位） */
    u64 total_bits = (u64)(offset + len) * 8;
    if (len >= SHA384_BLOCK_SIZE - 16) {
        sha384_process_block(block, h);
        memzero(block, SHA384_BLOCK_SIZE);
    }
    
    block[SHA384_BLOCK_SIZE - 16] = (u8)(total_bits >> 56);
    block[SHA384_BLOCK_SIZE - 15] = (u8)(total_bits >> 48);
    block[SHA384_BLOCK_SIZE - 14] = (u8)(total_bits >> 40);
    block[SHA384_BLOCK_SIZE - 13] = (u8)(total_bits >> 32);
    block[SHA384_BLOCK_SIZE - 12] = (u8)(total_bits >> 24);
    block[SHA384_BLOCK_SIZE - 11] = (u8)(total_bits >> 16);
    block[SHA384_BLOCK_SIZE - 10] = (u8)(total_bits >> 8);
    block[SHA384_BLOCK_SIZE - 9] = (u8)total_bits;
    
    sha384_process_block(block, h);
    
    /* 输出结果 */
    for (u32 i = 0; i < 6; i++) {
        hash[i * 8] = (u8)(h[i] >> 56);
        hash[i * 8 + 1] = (u8)(h[i] >> 48);
        hash[i * 8 + 2] = (u8)(h[i] >> 40);
        hash[i * 8 + 3] = (u8)(h[i] >> 32);
        hash[i * 8 + 4] = (u8)(h[i] >> 24);
        hash[i * 8 + 5] = (u8)(h[i] >> 16);
        hash[i * 8 + 6] = (u8)(h[i] >> 8);
        hash[i * 8 + 7] = (u8)h[i];
    }
    
    return HIC_SUCCESS;
}

/**
 * MGF1掩码生成函数
 */
hic_status_t crypto_mgf1(const u8* seed, u32 seed_len,
                         u8* mask, u32 mask_len,
                         u32 mgf_hash_alg) {
    (void)mgf_hash_alg;
    if (!seed || !mask || seed_len == 0 || mask_len == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    u32 hash_len = SHA384_DIGEST_SIZE;
    u32 counter = 0;
    u32 offset = 0;
    
    while (offset < mask_len) {
        /* 计算H(seed || counter) */
        u8 hash_input[seed_len + 4];
        memcopy(hash_input, seed, seed_len);
        
        /* 大端序计数器 */
        hash_input[seed_len] = (u8)((counter >> 24) & 0xFF);
        hash_input[seed_len + 1] = (u8)((counter >> 16) & 0xFF);
        hash_input[seed_len + 2] = (u8)((counter >> 8) & 0xFF);
        hash_input[seed_len + 3] = (u8)(counter & 0xFF);
        
        u8 hash[SHA384_DIGEST_SIZE];
        crypto_sha384(hash_input, seed_len + 4, hash);
        
        /* 复制到掩码 */
        u32 copy_len = (offset + hash_len > mask_len) ? (mask_len - offset) : hash_len;
        memcopy(mask + offset, hash, copy_len);
        
        offset += copy_len;
        counter++;
    }
    
    return HIC_SUCCESS;
}

/**
 * RSA签名验证（PKCS#1 v2.1 RSASSA-PSS）
 * 完整实现框架
 */
hic_status_t crypto_rsa_verify_pss(const u8* message, u32 message_len,
                                    const u8* signature, u32 signature_len,
                                    const pkcs1_rsa_public_key_t* public_key,
                                    const pkcs1_pss_params_t* params) {
    (void)message;
    (void)message_len;
    (void)signature;
    (void)signature_len;
    (void)public_key;
    (void)params;
    
    /* TODO: 实现完整的RSA-PSS验证 */
    /* 需要：
     * 1. 实现大整数模幂运算
     * 2. 实现PSS格式验证
     * 3. 实现哈希比较
     */
    
    return HIC_ERROR_NOT_IMPLEMENTED;
}

/**
 * RSA签名验证（PKCS#1 v1.5）
 * 完整实现框架
 */
hic_status_t crypto_rsa_verify_v1_5(const u8* message, u32 message_len,
                                     const u8* signature, u32 signature_len,
                                     const pkcs1_rsa_public_key_t* public_key,
                                     u32 hash_alg) {
    (void)message;
    (void)message_len;
    (void)signature;
    (void)signature_len;
    (void)public_key;
    (void)hash_alg;
    
    /* TODO: 实现完整的RSA-v1.5验证 */
    
    return HIC_ERROR_NOT_IMPLEMENTED;
}

/* ============= 服务API实现 ============= */

static hic_status_t service_init(void) {
    console_puts("[CRYPTO] Crypto service initialized\n");
    return HIC_SUCCESS;
}

static hic_status_t service_start(void) {
    console_puts("[CRYPTO] Crypto service started\n");
    return HIC_SUCCESS;
}

static hic_status_t service_stop(void) {
    console_puts("[CRYPTO] Crypto service stopped\n");
    return HIC_SUCCESS;
}

static hic_status_t service_cleanup(void) {
    return HIC_SUCCESS;
}

static hic_status_t service_get_info(char* buffer, u32 buffer_size) {
    if (!buffer || buffer_size == 0) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    const char* info = "Crypto Service v1.0.0 - "
                       "Provides SHA-384 hashing and RSA signature verification";
    u32 len = strlen(info);
    
    if (len >= buffer_size) {
        return HIC_ERROR_BUFFER_TOO_SMALL;
    }
    
    strcpy(buffer, info);
    return HIC_SUCCESS;
}

/* 服务API表 */
const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

/* 服务注册函数 */
void service_register_self(void) {
    service_register("crypto_service", &g_service_api);
}
