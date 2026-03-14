/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC加密服务实现
 * 
 * 提供密码学操作服务：
 * - SHA-384 哈希计算
 * - RSA-PSS 签名验证
 * - RSA v1.5 签名验证
 * - MGF1 掩码生成
 */

#include "service.h"
#include "crypto_service.h"

/* ========== 辅助函数 ========== */

/**
 * 内存清零（安全清除敏感数据）
 */
static void secure_memzero(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

/**
 * 常量时间内存比较
 */
int constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0 ? 1 : 0;
}

/* ========== SHA-384 实现 ========== */

/* SHA-384 初始哈希值 */
static const uint64_t sha384_init_state[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
    0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
    0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
};

/* SHA-384 轮常量 (K) */
static const uint64_t sha384_k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
    0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
    0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
    0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
    0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
    0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
    0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
    0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
    0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
    0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
    0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* 循环右移 */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

/* SHA-384 辅助函数 */
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x)    (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SIGMA1(x)    (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0(x)    (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define sigma1(x)    (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

/**
 * 从字节数组加载64位大端整数
 */
static uint64_t load_be64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  | ((uint64_t)p[7]);
}

/**
 * 存储64位大端整数到字节数组
 */
static void store_be64(uint8_t *p, uint64_t x)
{
    p[0] = (uint8_t)(x >> 56);
    p[1] = (uint8_t)(x >> 48);
    p[2] = (uint8_t)(x >> 40);
    p[3] = (uint8_t)(x >> 32);
    p[4] = (uint8_t)(x >> 24);
    p[5] = (uint8_t)(x >> 16);
    p[6] = (uint8_t)(x >> 8);
    p[7] = (uint8_t)(x);
}

/**
 * SHA-384 块处理
 */
static void sha384_transform(sha384_ctx_t *ctx, const uint8_t block[SHA384_BLOCK_SIZE])
{
    uint64_t W[80];
    uint64_t a, b, c, d, e, f, g, h;
    uint64_t T1, T2;
    
    /* 准备消息调度 */
    for (int i = 0; i < 16; i++) {
        W[i] = load_be64(block + i * 8);
    }
    for (int i = 16; i < 80; i++) {
        W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
    }
    
    /* 初始化工作变量 */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    /* 80轮处理 */
    for (int i = 0; i < 80; i++) {
        T1 = h + SIGMA1(e) + CH(e, f, g) + sha384_k[i] + W[i];
        T2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    /* 更新状态 */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
    
    /* 清除敏感数据 */
    secure_memzero(W, sizeof(W));
}

/**
 * 初始化 SHA-384 上下文
 */
void sha384_init(sha384_ctx_t *ctx)
{
    for (int i = 0; i < 8; i++) {
        ctx->state[i] = sha384_init_state[i];
    }
    ctx->count[0] = 0;
    ctx->count[1] = 0;
    ctx->buffer_len = 0;
}

/**
 * 更新 SHA-384 哈希
 */
void sha384_update(sha384_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t buffer_used = ctx->buffer_len;
    size_t buffer_free;
    size_t i;
    
    /* 更新位计数 */
    uint64_t bit_count = (uint64_t)len << 3;
    ctx->count[1] += bit_count;
    if (ctx->count[1] < bit_count) {
        ctx->count[0]++;
    }
    ctx->count[0] += (uint64_t)len >> 61;
    
    /* 处理缓冲区中的数据 */
    if (buffer_used > 0) {
        buffer_free = SHA384_BLOCK_SIZE - buffer_used;
        
        if (len >= buffer_free) {
            /* 填满缓冲区并处理 */
            for (i = 0; i < buffer_free; i++) {
                ctx->buffer[buffer_used + i] = data[i];
            }
            sha384_transform(ctx, ctx->buffer);
            data += buffer_free;
            len -= buffer_free;
            ctx->buffer_len = 0;
        } else {
            /* 将数据添加到缓冲区 */
            for (i = 0; i < len; i++) {
                ctx->buffer[buffer_used + i] = data[i];
            }
            ctx->buffer_len += len;
            return;
        }
    }
    
    /* 处理完整的块 */
    while (len >= SHA384_BLOCK_SIZE) {
        sha384_transform(ctx, data);
        data += SHA384_BLOCK_SIZE;
        len -= SHA384_BLOCK_SIZE;
    }
    
    /* 缓冲剩余数据 */
    if (len > 0) {
        for (i = 0; i < len; i++) {
            ctx->buffer[i] = data[i];
        }
        ctx->buffer_len = len;
    }
}

/**
 * 完成 SHA-384 哈希
 */
void sha384_final(sha384_ctx_t *ctx, uint8_t digest[SHA384_DIGEST_SIZE])
{
    uint64_t bit_count[2];
    size_t buffer_used = ctx->buffer_len;
    
    /* 保存位计数 */
    bit_count[0] = ctx->count[0];
    bit_count[1] = ctx->count[1];
    
    /* 添加填充位 (1后跟0) */
    ctx->buffer[buffer_used++] = 0x80;
    
    /* 如果缓冲区空间不足，填充并处理 */
    if (buffer_used > 112) {
        while (buffer_used < SHA384_BLOCK_SIZE) {
            ctx->buffer[buffer_used++] = 0x00;
        }
        sha384_transform(ctx, ctx->buffer);
        buffer_used = 0;
    }
    
    /* 填充到112字节 */
    while (buffer_used < 112) {
        ctx->buffer[buffer_used++] = 0x00;
    }
    
    /* 添加位计数（大端） */
    store_be64(ctx->buffer + 112, bit_count[0]);
    store_be64(ctx->buffer + 120, bit_count[1]);
    sha384_transform(ctx, ctx->buffer);
    
    /* 输出哈希值（前48字节） */
    for (int i = 0; i < 6; i++) {
        store_be64(digest + i * 8, ctx->state[i]);
    }
    
    /* 清除敏感数据 */
    secure_memzero(ctx, sizeof(*ctx));
}

/**
 * 单次计算 SHA-384 哈希
 */
int sha384_hash(const uint8_t *data, size_t len, uint8_t digest[SHA384_DIGEST_SIZE])
{
    sha384_ctx_t ctx;
    
    sha384_init(&ctx);
    sha384_update(&ctx, data, len);
    sha384_final(&ctx, digest);
    
    return CRYPTO_SUCCESS;
}

/* ========== RSA 辅助函数 ========== */

/**
 * 比较大端整数
 */
static int be_int_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

/**
 * 大端整数减法: result = a - b (a >= b)
 */
static void be_int_subtract(uint8_t *result, const uint8_t *a, const uint8_t *b, size_t len)
{
    int borrow = 0;
    for (int i = len - 1; i >= 0; i--) {
        int diff = a[i] - b[i] - borrow;
        if (diff < 0) {
            diff += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = (uint8_t)diff;
    }
}

/**
 * 大端整数模乘: result = (a * b) mod n
 * 简化实现：使用标准模乘算法
 */
static void be_int_modmul(uint8_t *result, const uint8_t *a, const uint8_t *b,
                          const uint8_t *n, size_t len)
{
    uint8_t temp[2 * RSA_MAX_KEY_SIZE];
    uint8_t quotient[2 * RSA_MAX_KEY_SIZE];
    
    /* 初始化 */
    for (size_t i = 0; i < 2 * len; i++) {
        temp[i] = 0;
    }
    
    /* 乘法 */
    for (size_t i = 0; i < len; i++) {
        int carry = 0;
        for (size_t j = 0; j < len; j++) {
            size_t idx = len - 1 - i - j + len;
            if (idx < 2 * len) {
                int product = temp[idx] + a[len - 1 - i] * b[len - 1 - j] + carry;
                temp[idx] = (uint8_t)(product & 0xFF);
                carry = product >> 8;
            }
        }
    }
    
    /* 模运算（简化：重复减法） */
    while (be_int_compare(temp, n, len) >= 0) {
        be_int_subtract(temp, temp, n, 2 * len);
    }
    
    /* 复制结果 */
    for (size_t i = 0; i < len; i++) {
        result[i] = temp[len + i];
    }
    
    secure_memzero(temp, sizeof(temp));
    secure_memzero(quotient, sizeof(quotient));
}

/**
 * RSA 模幂运算: result = base^exp mod n
 */
static int rsa_modexp(uint8_t *result, const uint8_t *base, uint32_t exp,
                      const uint8_t *n, size_t key_size)
{
    if (key_size == 0 || key_size > RSA_MAX_KEY_SIZE) {
        return CRYPTO_ERR_INVALID_PARAM;
    }
    
    uint8_t temp[RSA_MAX_KEY_SIZE];
    uint8_t acc[RSA_MAX_KEY_SIZE];
    
    /* 初始化 accumulator = 1 */
    for (size_t i = 0; i < key_size; i++) {
        acc[i] = 0;
    }
    acc[key_size - 1] = 1;
    
    /* 复制 base 到 temp */
    for (size_t i = 0; i < key_size; i++) {
        temp[i] = base[i];
    }
    
    /* 模幂运算（平方-乘法算法） */
    while (exp > 0) {
        if (exp & 1) {
            be_int_modmul(acc, acc, temp, n, key_size);
        }
        be_int_modmul(temp, temp, temp, n, key_size);
        exp >>= 1;
    }
    
    /* 复制结果 */
    for (size_t i = 0; i < key_size; i++) {
        result[i] = acc[i];
    }
    
    secure_memzero(temp, sizeof(temp));
    secure_memzero(acc, sizeof(acc));
    
    return CRYPTO_SUCCESS;
}

/**
 * PKCS#1 v1.5 编码的签名验证
 */
int rsa_verify_v15(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint8_t hash_algo)
{
    uint8_t decrypted[RSA_MAX_KEY_SIZE];
    uint8_t digest[SHA384_DIGEST_SIZE];
    size_t digest_len;
    
    if (!message || !signature || !public_key) {
        return CRYPTO_ERR_INVALID_PARAM;
    }
    
    if (sig_len != public_key->key_size) {
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    /* 计算消息哈希 */
    if (hash_algo == 0) {
        /* SHA-256 - 简化为使用 SHA-384 */
        digest_len = SHA384_DIGEST_SIZE;
    } else {
        digest_len = SHA384_DIGEST_SIZE;
    }
    (void)digest_len; /* 暂时避免未使用警告 */
    
    if (sha384_hash(message, message_len, digest) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    /* RSA 解密（签名验证） */
    if (rsa_modexp(decrypted, signature, public_key->exponent,
                   public_key->modulus, public_key->key_size) != CRYPTO_SUCCESS) {
        secure_memzero(digest, sizeof(digest));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    /* 检查 PKCS#1 v1.5 填充格式 */
    /* 格式: 0x00 0x01 [0xFF...] 0x00 [DigestInfo] [Digest] */
    if (decrypted[0] != 0x00 || decrypted[1] != 0x01) {
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    /* 查找 0x00 分隔符 */
    size_t i = 2;
    while (i < public_key->key_size && decrypted[i] == 0xFF) {
        i++;
    }
    
    if (i >= public_key->key_size || decrypted[i] != 0x00) {
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    i++;
    
    /* 构造预期的编码格式（简化：直接比较哈希值） */
    size_t expected_hash_offset = i + 19; /* 跳过 DigestInfo 头 */
    
    if (expected_hash_offset + digest_len > public_key->key_size) {
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    /* 常量时间比较哈希值 */
    int match = constant_time_compare(decrypted + expected_hash_offset, 
                                       digest, digest_len);
    
    secure_memzero(decrypted, sizeof(decrypted));
    secure_memzero(digest, sizeof(digest));
    
    return match ? CRYPTO_SUCCESS : CRYPTO_ERR_SIG_MISMATCH;
}

/* ========== MGF1 实现 ========== */

/**
 * MGF1 掩码生成函数
 */
int mgf1_generate(const uint8_t *seed, size_t seed_len,
                  uint8_t *mask, size_t mask_len,
                  uint8_t hash_algo)
{
    uint8_t counter[4];
    uint8_t digest[SHA384_DIGEST_SIZE];
    size_t hash_len = SHA384_DIGEST_SIZE;
    size_t done = 0;
    
    (void)hash_algo; /* 简化：仅使用 SHA-384 */
    
    if (!seed || !mask || mask_len == 0) {
        return CRYPTO_ERR_INVALID_PARAM;
    }
    
    /* MGF1 循环 */
    for (uint32_t i = 0; done < mask_len; i++) {
        /* 构造计数器（大端） */
        counter[0] = (uint8_t)(i >> 24);
        counter[1] = (uint8_t)(i >> 16);
        counter[2] = (uint8_t)(i >> 8);
        counter[3] = (uint8_t)(i);
        
        /* 计算 Hash(seed || counter) */
        sha384_ctx_t ctx;
        sha384_init(&ctx);
        sha384_update(&ctx, seed, seed_len);
        sha384_update(&ctx, counter, 4);
        sha384_final(&ctx, digest);
        
        /* 复制到掩码 */
        size_t copy_len = (mask_len - done < hash_len) ? (mask_len - done) : hash_len;
        for (size_t j = 0; j < copy_len; j++) {
            mask[done + j] = digest[j];
        }
        done += copy_len;
    }
    
    secure_memzero(digest, sizeof(digest));
    
    return CRYPTO_SUCCESS;
}

/**
 * RSA-PSS 签名验证
 */
int rsa_verify_pss(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint32_t salt_len)
{
    uint8_t decrypted[RSA_MAX_KEY_SIZE];
    uint8_t m_hash[SHA384_DIGEST_SIZE];
    uint8_t m_prime[8 + SHA384_DIGEST_SIZE + RSA_MAX_KEY_SIZE];
    uint8_t h_prime[SHA384_DIGEST_SIZE];
    uint8_t db_mask[RSA_MAX_KEY_SIZE];
    uint8_t db[RSA_MAX_KEY_SIZE];
    size_t em_len;
    size_t hash_len = SHA384_DIGEST_SIZE;
    
    if (!message || !signature || !public_key) {
        return CRYPTO_ERR_INVALID_PARAM;
    }
    
    if (sig_len != public_key->key_size) {
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    em_len = public_key->key_size;
    
    /* 计算消息哈希 */
    if (sha384_hash(message, message_len, m_hash) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    /* RSA 解密 */
    if (rsa_modexp(decrypted, signature, public_key->exponent,
                   public_key->modulus, public_key->key_size) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    /* 检查 EM 长度 */
    if (em_len < hash_len + salt_len + 2) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    /* 检查最右字节 */
    if (decrypted[em_len - 1] != 0xBC) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    /* 生成 M' = 0x00*8 || mHash || salt */
    for (int i = 0; i < 8; i++) {
        m_prime[i] = 0x00;
    }
    for (size_t i = 0; i < hash_len; i++) {
        m_prime[8 + i] = m_hash[i];
    }
    /* salt 从 EM 中提取 */
    size_t salt_offset = em_len - hash_len - salt_len - 1;
    for (size_t i = 0; i < salt_len; i++) {
        m_prime[8 + hash_len + i] = decrypted[salt_offset + i];
    }
    
    /* 计算 H' = Hash(M') */
    if (sha384_hash(m_prime, 8 + hash_len + salt_len, h_prime) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    /* 比较 H 和 H' */
    size_t h_offset = em_len - hash_len - 1;
    if (!constant_time_compare(decrypted + h_offset, h_prime, hash_len)) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        secure_memzero(h_prime, sizeof(h_prime));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    /* 生成 dbMask = MGF(H) */
    if (mgf1_generate(decrypted + h_offset, hash_len, db_mask, h_offset, 0) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        secure_memzero(h_prime, sizeof(h_prime));
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    /* DB = maskedDB XOR dbMask */
    for (size_t i = 0; i < h_offset; i++) {
        db[i] = decrypted[i] ^ db_mask[i];
    }
    
    /* 检查 DB 格式: PS || 0x01 || salt */
    /* PS 是 0x00 填充 */
    size_t ps_len = em_len - hash_len - salt_len - 2;
    for (size_t i = 0; i < ps_len; i++) {
        if (db[i] != 0x00) {
            secure_memzero(m_hash, sizeof(m_hash));
            secure_memzero(decrypted, sizeof(decrypted));
            secure_memzero(m_prime, sizeof(m_prime));
            secure_memzero(h_prime, sizeof(h_prime));
            secure_memzero(db, sizeof(db));
            return CRYPTO_ERR_SIG_MISMATCH;
        }
    }
    
    if (db[ps_len] != 0x01) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        secure_memzero(h_prime, sizeof(h_prime));
        secure_memzero(db, sizeof(db));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    /* 清除敏感数据 */
    secure_memzero(m_hash, sizeof(m_hash));
    secure_memzero(decrypted, sizeof(decrypted));
    secure_memzero(m_prime, sizeof(m_prime));
    secure_memzero(h_prime, sizeof(h_prime));
    secure_memzero(db_mask, sizeof(db_mask));
    secure_memzero(db, sizeof(db));
    
    return CRYPTO_SUCCESS;
}

/* ========== 服务接口实现 ========== */

hic_status_t service_init(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_start(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_stop(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_cleanup(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_get_info(char* buffer, u32 size)
{
    if (buffer && size > 0) {
        const char *info = "Crypto Service v1.0.0\n"
                          "Endpoints: sha384(0x6700), rsa_pss(0x6701), rsa_v15(0x6702), mgf1(0x6703)";
        int len = 0;
        while (info[len] && len < (int)(size - 1)) {
            buffer[len] = info[len];
            len++;
        }
        buffer[len] = '\0';
    }
    return HIC_SUCCESS;
}

const service_api_t g_service_api = {
    .init = service_init,
    .start = service_start,
    .stop = service_stop,
    .cleanup = service_cleanup,
    .get_info = service_get_info,
};

void service_register_self(void)
{
    service_register("crypto_service", &g_service_api);
}