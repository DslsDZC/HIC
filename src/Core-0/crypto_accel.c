/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 密码学加速抽象层 - 内核桩实现
 * 
 * Core-0 层只提供抽象接口和硬件回调注册
 * 完整的软件实现在 Privileged-1/crypto_service 中
 */

#include "crypto_accel.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "lib/console.h"

/* 架构操作回调 */
static crypto_arch_ops_t g_crypto_ops = {0};

/* 硬件能力 */
static crypto_accel_caps_t g_crypto_caps = {
    .supported = false,
    .enabled = false,
    .supported_algos = 0,
    .aes_latency_cycles = 0,
    .sha256_latency_cycles = 0,
    .rsa_latency_cycles = 0,
    .throughput_mbps = 0,
    .supports_aes_gcm = false,
    .supports_aes_xts = false,
    .supports_sha_ext = false,
    .supports_ecc = false,
};

/* 性能统计 */
static u64 g_total_operations = 0;
static u64 g_total_bytes = 0;

/* ========== 初始化 ========== */

void crypto_accel_init(void)
{
    console_puts("[CRYPTO] Initializing crypto abstraction layer\n");
    
    /* 探测硬件能力 */
    if (g_crypto_ops.get_caps) {
        g_crypto_ops.get_caps(&g_crypto_caps);
    }
    
    console_puts("[CRYPTO] Crypto layer initialized\n");
    if (g_crypto_caps.supported) {
        console_puts("[CRYPTO]   Hardware acceleration: available\n");
    } else {
        console_puts("[CRYPTO]   Hardware acceleration: unavailable\n");
        console_puts("[CRYPTO]   Using Privileged-1 crypto_service\n");
    }
}

void crypto_accel_shutdown(void)
{
    console_puts("[CRYPTO] Shutting down crypto abstraction layer\n");
    
    g_crypto_caps.enabled = false;
    g_total_operations = 0;
    g_total_bytes = 0;
}

void crypto_accel_register_ops(const crypto_arch_ops_t *ops)
{
    if (ops) {
        memcopy(&g_crypto_ops, ops, sizeof(crypto_arch_ops_t));
        console_puts("[CRYPTO] Architecture ops registered\n");
    }
}

/* ========== 能力查询 ========== */

const crypto_accel_caps_t* crypto_accel_get_caps(void)
{
    return &g_crypto_caps;
}

bool crypto_accel_algo_supported(crypto_algo_t algo)
{
    if (algo >= CRYPTO_ALGO_MAX) {
        return false;
    }
    return (g_crypto_caps.supported_algos & (1 << algo)) != 0;
}

bool crypto_accel_enabled(void)
{
    return g_crypto_caps.enabled;
}

/* ========== AES 操作桩函数 ========== */

bool crypto_aes_encrypt(const void *key, u32 key_bits,
                        const void *iv, u32 mode,
                        const void *input, void *output, u32 size)
{
    /* 尝试硬件加速 */
    if (g_crypto_caps.enabled && g_crypto_ops.aes_encrypt) {
        crypto_aes_ctx_t ctx;
        memzero(&ctx, sizeof(ctx));
        memcopy(ctx.key, key, key_bits / 8);
        if (iv) {
            memcopy(ctx.iv, iv, 16);
        }
        ctx.key_size = key_bits;
        ctx.mode = mode;
        ctx.encrypt = true;
        
        bool result = g_crypto_ops.aes_encrypt(&ctx, input, output, size);
        g_total_operations++;
        g_total_bytes += size;
        return result;
    }
    
    /* 无硬件加速，需要调用 crypto_service */
    console_puts("[CRYPTO] AES encrypt: no hardware accel, use crypto_service\n");
    return false;
}

bool crypto_aes_decrypt(const void *key, u32 key_bits,
                        const void *iv, u32 mode,
                        const void *input, void *output, u32 size)
{
    /* 尝试硬件加速 */
    if (g_crypto_caps.enabled && g_crypto_ops.aes_decrypt) {
        crypto_aes_ctx_t ctx;
        memzero(&ctx, sizeof(ctx));
        memcopy(ctx.key, key, key_bits / 8);
        if (iv) {
            memcopy(ctx.iv, iv, 16);
        }
        ctx.key_size = key_bits;
        ctx.mode = mode;
        ctx.encrypt = false;
        
        bool result = g_crypto_ops.aes_decrypt(&ctx, input, output, size);
        g_total_operations++;
        g_total_bytes += size;
        return result;
    }
    
    console_puts("[CRYPTO] AES decrypt: no hardware accel, use crypto_service\n");
    return false;
}

bool crypto_aes_gcm_encrypt(const void *key, u32 key_bits,
                            const void *iv,
                            const void *aad, u32 aad_size,
                            const void *input, void *output, u32 size,
                            void *tag)
{
    /* GCM 需要硬件或服务层支持 */
    (void)key;
    (void)key_bits;
    (void)iv;
    (void)aad;
    (void)aad_size;
    (void)input;
    (void)output;
    (void)size;
    (void)tag;
    
    console_puts("[CRYPTO] AES-GCM encrypt: use crypto_service\n");
    return false;
}

bool crypto_aes_gcm_decrypt(const void *key, u32 key_bits,
                            const void *iv,
                            const void *aad, u32 aad_size,
                            const void *input, void *output, u32 size,
                            const void *tag)
{
    (void)key;
    (void)key_bits;
    (void)iv;
    (void)aad;
    (void)aad_size;
    (void)input;
    (void)output;
    (void)size;
    (void)tag;
    
    console_puts("[CRYPTO] AES-GCM decrypt: use crypto_service\n");
    return false;
}

bool crypto_aes_gcm_verify(const void *expected_tag, const void *actual_tag)
{
    return crypto_constant_time_compare(expected_tag, actual_tag, 16);
}

/* ========== SHA 操作桩函数 ========== */

bool crypto_sha_init(crypto_sha_ctx_t *ctx, crypto_algo_t algo)
{
    if (!ctx) {
        return false;
    }
    
    memzero(ctx, sizeof(*ctx));
    ctx->algo = algo;
    ctx->finalized = false;
    
    /* 尝试硬件加速 */
    if (g_crypto_caps.enabled && g_crypto_ops.sha_init) {
        return g_crypto_ops.sha_init(ctx, algo);
    }
    
    /* 设置默认块大小 */
    switch (algo) {
        case CRYPTO_ALGO_SHA256:
            ctx->block_size = 64;
            ctx->state_size = 32;
            break;
        case CRYPTO_ALGO_SHA384:
            ctx->block_size = 128;
            ctx->state_size = 48;
            break;
        case CRYPTO_ALGO_SHA512:
            ctx->block_size = 128;
            ctx->state_size = 64;
            break;
        default:
            return false;
    }
    
    /* 无硬件加速，需要调用 crypto_service */
    console_puts("[CRYPTO] SHA init: no hardware accel, use crypto_service\n");
    return false;
}

bool crypto_sha_update(crypto_sha_ctx_t *ctx, const void *data, u32 size)
{
    if (!ctx || !data || ctx->finalized) {
        return false;
    }
    
    ctx->total_bytes += size;
    
    /* 尝试硬件加速 */
    if (g_crypto_caps.enabled && g_crypto_ops.sha_update) {
        return g_crypto_ops.sha_update(ctx, data, size);
    }
    
    console_puts("[CRYPTO] SHA update: use crypto_service\n");
    return false;
}

bool crypto_sha_final(crypto_sha_ctx_t *ctx, void *digest)
{
    if (!ctx || !digest || ctx->finalized) {
        return false;
    }
    
    ctx->finalized = true;
    
    /* 尝试硬件加速 */
    if (g_crypto_caps.enabled && g_crypto_ops.sha_final) {
        return g_crypto_ops.sha_final(ctx, digest);
    }
    
    console_puts("[CRYPTO] SHA final: use crypto_service\n");
    return false;
}

bool crypto_sha_hash(crypto_algo_t algo, const void *data, u32 size, void *digest)
{
    crypto_sha_ctx_t ctx;
    
    if (!crypto_sha_init(&ctx, algo)) {
        return false;
    }
    
    if (!crypto_sha_update(&ctx, data, size)) {
        return false;
    }
    
    return crypto_sha_final(&ctx, digest);
}

/* ========== HMAC 操作桩函数 ========== */

bool crypto_hmac_init(crypto_sha_ctx_t *ctx, crypto_algo_t algo,
                      const void *key, u32 key_size)
{
    (void)ctx;
    (void)algo;
    (void)key;
    (void)key_size;
    
    console_puts("[CRYPTO] HMAC init: use crypto_service\n");
    return false;
}

bool crypto_hmac_update(crypto_sha_ctx_t *ctx, const void *data, u32 size)
{
    return crypto_sha_update(ctx, data, size);
}

bool crypto_hmac_final(crypto_sha_ctx_t *ctx, void *mac)
{
    (void)ctx;
    (void)mac;
    
    console_puts("[CRYPTO] HMAC final: use crypto_service\n");
    return false;
}

/* ========== 密钥派生桩函数 ========== */

bool crypto_pbkdf2(const void *password, u32 password_len,
                  const void *salt, u32 salt_len,
                  void *key, u32 key_size,
                  u32 iterations)
{
    if (g_crypto_ops.kdf) {
        return g_crypto_ops.kdf(CRYPTO_ALGO_SHA256,
                               password, password_len,
                               salt, salt_len,
                               key, key_size,
                               iterations);
    }
    
    console_puts("[CRYPTO] PBKDF2: use crypto_service\n");
    return false;
}

bool crypto_hkdf(const void *ikm, u32 ikm_len,
                const void *salt, u32 salt_len,
                const void *info, u32 info_len,
                void *okm, u32 okm_len)
{
    (void)ikm;
    (void)ikm_len;
    (void)salt;
    (void)salt_len;
    (void)info;
    (void)info_len;
    (void)okm;
    (void)okm_len;
    
    console_puts("[CRYPTO] HKDF: use crypto_service\n");
    return false;
}

/* ========== 安全辅助函数 ========== */

bool crypto_constant_time_compare(const void *a, const void *b, u32 size)
{
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    u8 result = 0;
    
    for (u32 i = 0; i < size; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result == 0;
}

void crypto_secure_zero(void *ptr, u32 size)
{
    volatile u8 *p = (volatile u8 *)ptr;
    while (size--) {
        *p++ = 0;
    }
}

/* ========== 性能统计 ========== */

void crypto_accel_get_stats(u64 *operations, u64 *bytes_processed)
{
    if (operations) {
        *operations = g_total_operations;
    }
    if (bytes_processed) {
        *bytes_processed = g_total_bytes;
    }
}

void crypto_accel_print_info(void)
{
    console_puts("\n[CRYPTO] ===== Crypto Abstraction Layer =====\n");
    
    console_puts("[CRYPTO] Hardware acceleration: ");
    console_puts(g_crypto_caps.supported ? "available" : "unavailable");
    console_puts("\n");
    
    console_puts("[CRYPTO] Full implementation: Privileged-1/crypto_service\n");
    
    console_puts("[CRYPTO] Operations: ");
    console_putu64(g_total_operations);
    console_puts("\n");
    
    console_puts("[CRYPTO] Bytes processed: ");
    console_putu64(g_total_bytes);
    console_puts("\n");
    
    console_puts("[CRYPTO] =====================================\n\n");
}
