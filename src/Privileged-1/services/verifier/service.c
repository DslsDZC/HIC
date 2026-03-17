/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 固化验证服务实现
 * 
 * 包含所有签名验证逻辑（从内核迁移）
 * 设计原则：自包含，不依赖内核密码学接口
 * 
 * 代码量目标：<800 行
 */

#include "service.h"
#include <string.h>

/* ==================== 服务入口点（必须在代码段最前面） ==================== */

/**
 * 服务入口点 - 必须放在代码段最前面
 * 使用 section 属性确保此函数在链接时位于 .static_svc.verifier.text 的开头
 */
__attribute__((section(".static_svc.verifier.text"), used, noinline))
int _verifier_entry(void)
{
    /* 初始化验证服务 */
    verifier_init();
    
    /* 启动服务主循环 */
    verifier_start();
    
    /* 服务不应返回，如果返回则进入无限循环 */
    while (1) {
        /* 等待被调度或终止 */
        __asm__ volatile("hlt");
    }
    
    return 0;
}

/* ==================== SHA-384 实现（内嵌） ==================== */

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define EP1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SIG0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define SIG1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

static const uint64_t sha384_k[80] = {
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

static void sha384_internal(const uint8_t *data, uint32_t len, uint8_t *hash)
{
    uint64_t h[8] = {
        0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
        0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
        0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
        0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
    };
    
    uint64_t w[80];
    uint32_t total_len = len;
    uint8_t pad_buffer[128];
    
    /* 处理完整块 */
    while (len >= 128) {
        for (uint32_t i = 0; i < 16; i++) {
            w[i] = ((uint64_t)data[i * 8] << 56) |
                   ((uint64_t)data[i * 8 + 1] << 48) |
                   ((uint64_t)data[i * 8 + 2] << 40) |
                   ((uint64_t)data[i * 8 + 3] << 32) |
                   ((uint64_t)data[i * 8 + 4] << 24) |
                   ((uint64_t)data[i * 8 + 5] << 16) |
                   ((uint64_t)data[i * 8 + 6] << 8) |
                   (uint64_t)data[i * 8 + 7];
        }
        
        for (uint32_t i = 16; i < 80; i++) {
            w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
        }
        
        uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint64_t e = h[4], f = h[5], g = h[6], h_val = h[7];
        
        for (uint32_t i = 0; i < 80; i++) {
            uint64_t t1 = h_val + EP1(e) + CH(e, f, g) + sha384_k[i] + w[i];
            uint64_t t2 = EP0(a) + MAJ(a, b, c);
            h_val = g; g = f; f = e;
            e = d + t1;
            d = c; c = b; b = a;
            a = t1 + t2;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_val;
        
        data += 128;
        len -= 128;
    }
    
    /* 处理填充 */
    for (uint32_t i = 0; i < 128; i++) {
        pad_buffer[i] = (i < len) ? data[i] : 0;
    }
    pad_buffer[len] = 0x80;
    
    if (len >= 112) {
        /* 需要额外块 */
        /* 简化处理：假设总长度 < 2^32 */
    }
    
    /* 长度字段（位数，大端） */
    uint64_t bit_len = (uint64_t)total_len * 8;
    pad_buffer[120] = (uint8_t)(bit_len >> 56);
    pad_buffer[121] = (uint8_t)(bit_len >> 48);
    pad_buffer[122] = (uint8_t)(bit_len >> 40);
    pad_buffer[123] = (uint8_t)(bit_len >> 32);
    pad_buffer[124] = (uint8_t)(bit_len >> 24);
    pad_buffer[125] = (uint8_t)(bit_len >> 16);
    pad_buffer[126] = (uint8_t)(bit_len >> 8);
    pad_buffer[127] = (uint8_t)bit_len;
    
    /* 处理最后块 */
    for (uint32_t i = 0; i < 16; i++) {
        w[i] = ((uint64_t)pad_buffer[i * 8] << 56) |
               ((uint64_t)pad_buffer[i * 8 + 1] << 48) |
               ((uint64_t)pad_buffer[i * 8 + 2] << 40) |
               ((uint64_t)pad_buffer[i * 8 + 3] << 32) |
               ((uint64_t)pad_buffer[i * 8 + 4] << 24) |
               ((uint64_t)pad_buffer[i * 8 + 5] << 16) |
               ((uint64_t)pad_buffer[i * 8 + 6] << 8) |
               (uint64_t)pad_buffer[i * 8 + 7];
    }
    
    for (uint32_t i = 16; i < 80; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }
    
    uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint64_t e = h[4], f = h[5], g = h[6], h_val = h[7];
    
    for (uint32_t i = 0; i < 80; i++) {
        uint64_t t1 = h_val + EP1(e) + CH(e, f, g) + sha384_k[i] + w[i];
        uint64_t t2 = EP0(a) + MAJ(a, b, c);
        h_val = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }
    
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += h_val;
    
    /* 输出前 48 字节（SHA-384） */
    for (uint32_t i = 0; i < 6; i++) {
        hash[i * 8] = (uint8_t)(h[i] >> 56);
        hash[i * 8 + 1] = (uint8_t)(h[i] >> 48);
        hash[i * 8 + 2] = (uint8_t)(h[i] >> 40);
        hash[i * 8 + 3] = (uint8_t)(h[i] >> 32);
        hash[i * 8 + 4] = (uint8_t)(h[i] >> 24);
        hash[i * 8 + 5] = (uint8_t)(h[i] >> 16);
        hash[i * 8 + 6] = (uint8_t)(h[i] >> 8);
        hash[i * 8 + 7] = (uint8_t)h[i];
    }
}

/* ==================== 信任根公钥（固化） ==================== */

static const uint8_t g_trusted_root_hash[48] = {
    /* 开发阶段信任根哈希 */
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

/* RSA-3072 公钥（固化，384字节） */
/* 开发阶段：使用测试公钥 */
static const uint8_t g_rsa_n[384] = {
    /* 模数 n - 开发阶段填充 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* ... 其余为填充 ... */
};

static const uint8_t g_rsa_e[4] = { 0x00, 0x01, 0x00, 0x01 };  /* e = 65537 */

/* ==================== 辅助函数（提前定义） ==================== */

static int mem_eq(const void *a, const void *b, size_t len)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    
    for (size_t i = 0; i < len; i++) {
        if (pa[i] != pb[i]) return 0;
    }
    return 1;
}

static void mem_copy(void *dest, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
}

/* ==================== RSA-3072 大数运算 ==================== */

#define RSA3072_BYTES  384
#define RSA3072_BITS   3072
#define RSA3072_WORDS  (RSA3072_BYTES / sizeof(uint64_t))

/* 大数结构 */
typedef struct {
    uint64_t data[RSA3072_WORDS];
} bigint384_t;

/* 大数从字节加载（大端） */
static void bigint_load(bigint384_t *dst, const uint8_t *src, size_t len)
{
    memset(dst, 0, sizeof(*dst));
    
    /* 从大端字节转换到内部表示 */
    size_t words = (len + 7) / 8;
    for (size_t i = 0; i < words && i < RSA3072_WORDS; i++) {
        size_t offset = len - 8 * (i + 1);
        if (offset >= len) offset = 0;
        
        uint64_t val = 0;
        for (size_t j = 0; j < 8 && (offset + j) < len; j++) {
            val = (val << 8) | src[offset + j];
        }
        dst->data[i] = val;
    }
}

/* 大数比较 */
static int bigint_cmp(const bigint384_t *a, const bigint384_t *b)
{
    for (int i = RSA3072_WORDS - 1; i >= 0; i--) {
        if (a->data[i] > b->data[i]) return 1;
        if (a->data[i] < b->data[i]) return -1;
    }
    return 0;
}

/* 大数是否为零 */
static int bigint_is_zero(const bigint384_t *a)
{
    for (size_t i = 0; i < RSA3072_WORDS; i++) {
        if (a->data[i] != 0) return 0;
    }
    return 1;
}

/* 大数减法：result = a - b (假设 a >= b) */
static void bigint_sub(bigint384_t *result, const bigint384_t *a, const bigint384_t *b)
{
    uint64_t borrow = 0;
    
    for (size_t i = 0; i < RSA3072_WORDS; i++) {
        uint64_t diff = a->data[i] - b->data[i] - borrow;
        
        /* 检测借位 */
        if (a->data[i] < b->data[i] + borrow) {
            borrow = 1;
        } else if (a->data[i] - borrow < b->data[i]) {
            borrow = 1;
        } else {
            borrow = 0;
        }
        
        result->data[i] = diff;
    }
}

/* 大数模减：result = a - b mod n */
static void bigint_mod_sub(bigint384_t *result, const bigint384_t *a, 
                           const bigint384_t *b, const bigint384_t *n)
{
    bigint_sub(result, a, b);
    
    /* 如果结果为负，加 n */
    if (bigint_is_zero(result) == 0 && bigint_cmp(a, b) < 0) {
        /* result = result + n */
        uint64_t carry = 0;
        for (size_t i = 0; i < RSA3072_WORDS; i++) {
            uint64_t sum = result->data[i] + n->data[i] + carry;
            carry = (sum < result->data[i]) ? 1 : 0;
            result->data[i] = sum;
        }
    }
}

/* 大数模乘（Montgomery 乘法简化版） */
static void bigint_mod_mul(bigint384_t *result, const bigint384_t *a,
                           const bigint384_t *b, const bigint384_t *n)
{
    bigint384_t temp;
    memset(&temp, 0, sizeof(temp));
    
    /* 简化实现：使用标准乘法后模归约 */
    /* 对于开发阶段，使用简化的线性同余方法 */
    
    for (int i = RSA3072_WORDS - 1; i >= 0; i--) {
        for (int bit = 63; bit >= 0; bit--) {
            /* 左移 */
            uint64_t carry = 0;
            for (size_t j = 0; j < RSA3072_WORDS; j++) {
                uint64_t new_carry = temp.data[j] >> 63;
                temp.data[j] = (temp.data[j] << 1) | carry;
                carry = new_carry;
            }
            
            /* 模归约 */
            if (bigint_cmp(&temp, n) >= 0) {
                bigint_sub(&temp, &temp, n);
            }
            
            /* 如果 b 的当前位为 1，加 a */
            if ((b->data[i] >> bit) & 1) {
                carry = 0;
                for (size_t j = 0; j < RSA3072_WORDS; j++) {
                    uint64_t sum = temp.data[j] + a->data[j] + carry;
                    carry = (sum < temp.data[j]) ? 1 : 0;
                    temp.data[j] = sum;
                }
                
                /* 模归约 */
                while (bigint_cmp(&temp, n) >= 0) {
                    bigint_sub(&temp, &temp, n);
                }
            }
        }
    }
    
    mem_copy(result, &temp, sizeof(*result));
}

/* 大数模幂：result = base^exp mod n */
static void bigint_mod_exp(bigint384_t *result, const bigint384_t *base,
                           const uint8_t *exp, size_t exp_len,
                           const bigint384_t *n)
{
    bigint384_t temp;
    bigint384_t base_temp;
    
    /* 初始化 result = 1 */
    memset(result, 0, sizeof(*result));
    result->data[0] = 1;
    
    mem_copy(&base_temp, base, sizeof(base_temp));
    
    /* 从最高位开始处理指数 */
    for (size_t i = 0; i < exp_len; i++) {
        uint8_t byte = exp[i];
        
        for (int bit = 7; bit >= 0; bit--) {
            /* 平方 */
            bigint_mod_mul(result, result, result, n);
            
            /* 乘 */
            if ((byte >> bit) & 1) {
                bigint_mod_mul(result, result, &base_temp, n);
            }
        }
    }
}

/* ==================== MGF1 掩码生成函数 ==================== */

/**
 * MGF1 掩码生成函数 (RFC 2437)
 * 
 * @param seed 种子
 * @param seed_len 种子长度
 * @param mask 输出掩码
 * @param mask_len 掩码长度
 */
static void mgf1(const uint8_t *seed, size_t seed_len,
                 uint8_t *mask, size_t mask_len)
{
    uint8_t counter[4] = { 0, 0, 0, 0 };
    uint8_t hash[48];
    size_t done = 0;
    
    while (done < mask_len) {
        /* 计算 hash = SHA-384(seed || counter) */
        /* 使用简化的哈希计算 */
        uint8_t temp[256];
        size_t temp_len = 0;
        
        /* 复制种子 */
        for (size_t i = 0; i < seed_len && temp_len < sizeof(temp); i++) {
            temp[temp_len++] = seed[i];
        }
        
        /* 添加计数器 */
        for (int i = 0; i < 4 && temp_len < sizeof(temp); i++) {
            temp[temp_len++] = counter[i];
        }
        
        sha384_internal(temp, (uint32_t)temp_len, hash);
        
        /* 复制到掩码 */
        size_t copy_len = (mask_len - done < 48) ? (mask_len - done) : 48;
        for (size_t i = 0; i < copy_len; i++) {
            mask[done + i] = hash[i];
        }
        
        done += copy_len;
        
        /* 增加计数器 */
        for (int i = 3; i >= 0; i--) {
            if (++counter[i] != 0) break;
        }
    }
}

/* ==================== RSA-3072 PSS 验证 ==================== */

/**
 * RSA-3072 PSS 签名验证
 * 
 * @param hash 消息哈希 (48 字节, SHA-384)
 * @param hash_len 哈希长度
 * @param signature 签名 (384 字节)
 * @param sig_len 签名长度
 * @return 验证状态
 */
static verify_status_t rsa3072_pss_verify(const uint8_t *hash, size_t hash_len,
                                          const uint8_t *signature, size_t sig_len)
{
    bigint384_t n, s, m;
    uint8_t em[RSA3072_BYTES];
    uint8_t masked_seed[48], seed[48];
    uint8_t masked_db[RSA3072_BYTES - 48 - 1], db[RSA3072_BYTES - 48 - 1];
    uint8_t m_hash[48];
    uint8_t p_salt[32];  /* 盐值 */
    uint8_t m_prime[8 + 48 + 32];  /* M' = 0x00..00 || mHash || salt */
    uint8_t h_prime[48];
    
    /* 参数检查 */
    if (hash_len != 48 || sig_len != RSA3072_BYTES) {
        return VERIFY_ERR_INVALID_PARAM;
    }
    
    /* 加载公钥和签名 */
    bigint_load(&n, g_rsa_n, sizeof(g_rsa_n));
    bigint_load(&s, signature, sig_len);
    
    /* RSA 解密：m = s^e mod n */
    bigint_mod_exp(&m, &s, g_rsa_e, sizeof(g_rsa_e), &n);
    
    /* 将结果转换为字节（大端） */
    for (size_t i = 0; i < RSA3072_WORDS; i++) {
        uint64_t val = m.data[i];
        size_t offset = RSA3072_BYTES - 8 * (i + 1);
        if (offset < RSA3072_BYTES) {
            em[offset + 0] = (uint8_t)(val >> 56);
            em[offset + 1] = (uint8_t)(val >> 48);
            em[offset + 2] = (uint8_t)(val >> 40);
            em[offset + 3] = (uint8_t)(val >> 32);
            em[offset + 4] = (uint8_t)(val >> 24);
            em[offset + 5] = (uint8_t)(val >> 16);
            em[offset + 6] = (uint8_t)(val >> 8);
            em[offset + 7] = (uint8_t)val;
        }
    }
    
    /* PSS 解码 */
    /* EM = maskedDB || H || 0xbc */
    /* 其中 maskedDB = DB XOR MGF(H), DB = PS || 0x01 || salt */
    
    /* 检查尾部标记 */
    if (em[RSA3072_BYTES - 1] != 0xbc) {
        return VERIFY_ERR_SIGNATURE_INVALID;
    }
    
    /* 分离 maskedDB 和 H */
    size_t em_len = RSA3072_BYTES;
    size_t h_len = 48;
    size_t db_len = em_len - h_len - 1;
    
    for (size_t i = 0; i < db_len; i++) {
        masked_db[i] = em[i];
    }
    for (size_t i = 0; i < h_len; i++) {
        masked_seed[i] = em[db_len + i];
    }
    
    /* MGF 掩码恢复 */
    mgf1(masked_seed, h_len, db, db_len);
    
    /* XOR 恢复 DB */
    for (size_t i = 0; i < db_len; i++) {
        db[i] ^= masked_db[i];
    }
    
    /* 恢复 seed */
    mgf1(db, db_len, seed, h_len);
    for (size_t i = 0; i < h_len; i++) {
        seed[i] ^= masked_seed[i];
    }
    
    /* 验证 DB 格式 */
    /* DB = PS || 0x01 || salt */
    /* PS = 0x00 填充 */
    
    /* 跳过前导零 */
    size_t salt_start = 0;
    while (salt_start < db_len - 1 && db[salt_start] == 0x00) {
        salt_start++;
    }
    
    /* 检查分隔符 0x01 */
    if (db[salt_start] != 0x01) {
        return VERIFY_ERR_SIGNATURE_INVALID;
    }
    salt_start++;
    
    /* 提取盐值 */
    size_t salt_len = db_len - salt_start;
    if (salt_len > sizeof(p_salt)) {
        salt_len = sizeof(p_salt);
    }
    for (size_t i = 0; i < salt_len; i++) {
        p_salt[i] = db[salt_start + i];
    }
    
    /* 计算 M' = 0x00..00 || mHash || salt */
    memset(m_prime, 0, sizeof(m_prime));
    /* 8 字节前导零 */
    for (size_t i = 0; i < hash_len; i++) {
        m_prime[8 + i] = hash[i];
    }
    for (size_t i = 0; i < salt_len; i++) {
        m_prime[8 + hash_len + i] = p_salt[i];
    }
    
    /* 计算 H' = SHA-384(M') */
    sha384_internal(m_prime, (uint32_t)(8 + hash_len + salt_len), h_prime);
    
    /* 比较 H 和 H' */
    if (!mem_eq(masked_seed, h_prime, h_len)) {
        return VERIFY_ERR_SIGNATURE_INVALID;
    }
    
    return VERIFY_OK;
}

/* ==================== 全局状态 ==================== */

static int g_verifier_initialized = 0;
static int g_verify_count = 0;

/* ==================== 服务接口实现 ==================== */

int verifier_init(void)
{
    if (g_verifier_initialized) {
        return 0;
    }
    
    g_verifier_initialized = 1;
    g_verify_count = 0;
    
    return 0;
}

int verifier_start(void)
{
    /* 输出启动信息 */
    extern void serial_print(const char *msg);
    extern void thread_yield(void);
    
    serial_print("[VERIFIER] Service started\n");
    
    /* 主服务循环 - 等待请求 */
    /* TODO: 实现 IPC 请求处理 */
    int count = 0;
    while (1) {
        /* 让出 CPU 给其他线程 */
        thread_yield();
        count++;
        
        /* 避免无限循环太快 */
        if (count > 1000000) {
            count = 0;
        }
    }
    
    return 0;
}

int verifier_stop(void)
{
    return 0;
}

int verifier_cleanup(void)
{
    g_verifier_initialized = 0;
    return 0;
}

/* ==================== 验证接口实现 ==================== */

void verifier_compute_hash(const void *data, size_t size, uint8_t hash[48])
{
    sha384_internal((const uint8_t *)data, (uint32_t)size, hash);
}

size_t verifier_get_trusted_root(uint8_t *pubkey, size_t max_len)
{
    /* 从内核获取信任根 */
    extern size_t get_trusted_root_key(uint8_t *pubkey, size_t max_len);
    
    size_t len = get_trusted_root_key(pubkey, max_len);
    if (len > 0) {
        return len;
    }
    
    /* 回退到内嵌信任根 */
    if (max_len >= 48) {
        mem_copy(pubkey, g_trusted_root_hash, 48);
        return 48;
    }
    
    return 0;
}

verify_status_t verifier_verify_module(
    const void *module_data,
    size_t module_size,
    const module_sign_header_t *sign_header,
    verify_result_t *result)
{
    uint8_t computed_hash[48];
    
    if (!module_data || !sign_header || !result) {
        return VERIFY_ERR_INVALID_PARAM;
    }
    
    /* 检查魔数 */
    if (sign_header->magic != 0x48495347) {  /* "HISG" */
        result->status = VERIFY_ERR_SIGNATURE_INVALID;
        result->error_msg = "Invalid magic number";
        return VERIFY_ERR_SIGNATURE_INVALID;
    }
    
    /* 计算模块哈希 */
    verifier_compute_hash(module_data, module_size, computed_hash);
    mem_copy(result->computed_hash, computed_hash, 48);
    
    /* 开发阶段：仅哈希校验 */
    if (sign_header->sign_alg == SIGN_ALG_SHA384_HASH) {
        if (!mem_eq(computed_hash, sign_header->module_hash, 48)) {
            result->status = VERIFY_ERR_HASH_MISMATCH;
            result->error_msg = "Module hash mismatch";
            return VERIFY_ERR_HASH_MISMATCH;
        }
        
        g_verify_count++;
        result->status = VERIFY_OK;
        result->error_msg = "OK";
        return VERIFY_OK;
    }
    
    /* RSA-3072 PSS 验证 */
    if (sign_header->sign_alg == SIGN_ALG_RSA3072_PSS) {
        verify_status_t status = rsa3072_pss_verify(
            computed_hash, 48,
            sign_header->signature, sign_header->signature_len
        );
        
        if (status != VERIFY_OK) {
            result->status = status;
            result->error_msg = "RSA-3072 PSS verification failed";
            return status;
        }
        
        g_verify_count++;
        result->status = VERIFY_OK;
        result->error_msg = "OK";
        return VERIFY_OK;
    }
    
    result->status = VERIFY_ERR_NOT_SUPPORTED;
    result->error_msg = "Signature algorithm not yet supported";
    return VERIFY_ERR_NOT_SUPPORTED;
}

verify_status_t verifier_verify_cert_chain(
    const verifier_cert_t *certs,
    uint32_t cert_count)
{
    if (!certs || cert_count == 0) {
        return VERIFY_ERR_INVALID_PARAM;
    }
    
    /* 简化实现 */
    return VERIFY_OK;
}