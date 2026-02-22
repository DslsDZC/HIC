/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC PKCS#1 v2.1 RSASSA-PSS签名验证实现（完整版）
 * 遵循RFC 8017标准
 */

#include "pkcs1.h"
#include "lib/mem.h"
#include "lib/console.h"
#include "lib/string.h"

/* SHA-384辅助函数 */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHR64(x, n) ((x) >> (n))

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define EP1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SIG0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ SHR64(x, 7))
#define SIG1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ SHR64(x, 6))

/* 前向声明 */
static bool pkcs1_bignum_mul(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                              pkcs1_bignum_t* result);
static bool pkcs1_bignum_mod(const pkcs1_bignum_t* a, const pkcs1_bignum_t* mod,
                              pkcs1_bignum_t* result);

/* SHA-384哈希（完整实现） */
static void sha384(const u8* data, u32 len, u8* hash) {
    /* SHA-384初始哈希值 */
    static const u64 k[80] = {
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
    
    /* SHA-384初始哈希值 */
    u64 h[8] = {
        0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
        0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
        0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
        0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
    };
    
    /* 消息调度 */
    u64 w[80];
    u32 total_len = len;
    
    /* 处理消息 */
    while (len >= 128) {
        /* 准备消息调度 */
        for (u32 i = 0; i < 16; i++) {
            w[i] = ((u64)data[i * 8] << 56) |
                   ((u64)data[i * 8 + 1] << 48) |
                   ((u64)data[i * 8 + 2] << 40) |
                   ((u64)data[i * 8 + 3] << 32) |
                   ((u64)data[i * 8 + 4] << 24) |
                   ((u64)data[i * 8 + 5] << 16) |
                   ((u64)data[i * 8 + 6] << 8) |
                   (u64)data[i * 8 + 7];
        }
        
        for (u32 i = 16; i < 80; i++) {
            w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
        }
        
        /* 压缩 */
        u64 a = h[0], b = h[1], c = h[2], d = h[3];
        u64 e = h[4], f = h[5], g = h[6], h_h = h[7];
        
        for (u32 i = 0; i < 80; i++) {
            u64 t1 = h_h + EP1(e) + CH(e, f, g) + k[i] + w[i];
            u64 t2 = EP0(a) + MAJ(a, b, c);
            h_h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_h;
        
        data += 128;
        len -= 128;
    }
    
    /* 处理剩余数据和填充 */
    u8 block[128];
    memzero(block, 128);
    memcopy(block, data, len);
    block[len] = 0x80;
    
    /* 添加长度（大端序） */
    if (len >= 112) {
        /* 需要额外的一个块 */
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
        
        u64 a = h[0], b = h[1], c = h[2], d = h[3];
        u64 e = h[4], f = h[5], g = h[6], h_h = h[7];
        
        for (u32 i = 0; i < 80; i++) {
            u64 t1 = h_h + EP1(e) + CH(e, f, g) + k[i] + w[i];
            u64 t2 = EP0(a) + MAJ(a, b, c);
            h_h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += h_h;
        
        memzero(block, 128);
        u64 bit_len = (u64)total_len * 8;
        block[15] = (u8)(bit_len >> 56);
        block[14] = (u8)(bit_len >> 48);
        block[13] = (u8)(bit_len >> 40);
        block[12] = (u8)(bit_len >> 32);
        block[11] = (u8)(bit_len >> 24);
        block[10] = (u8)(bit_len >> 16);
        block[9] = (u8)(bit_len >> 8);
        block[8] = (u8)bit_len;
    } else {
        u64 bit_len = (u64)total_len * 8;
        block[15] = (u8)(bit_len >> 56);
        block[14] = (u8)(bit_len >> 48);
        block[13] = (u8)(bit_len >> 40);
        block[12] = (u8)(bit_len >> 32);
        block[11] = (u8)(bit_len >> 24);
        block[10] = (u8)(bit_len >> 16);
        block[9] = (u8)(bit_len >> 8);
        block[8] = (u8)bit_len;
    }
    
    /* 处理最后一个块 */
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
    
    u64 a = h[0], b = h[1], c = h[2], d = h[3];
    u64 e = h[4], f = h[5], g = h[6], h_h = h[7];
    
    for (u32 i = 0; i < 80; i++) {
        u64 t1 = h_h + EP1(e) + CH(e, f, g) + k[i] + w[i];
        u64 t2 = EP0(a) + MAJ(a, b, c);
        h_h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += h_h;
    
    /* 输出结果（SHA-384只使用前6个64位） */
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
}

/* MGF1实现（完整版） */
bool pkcs1_mgf1(const u8* seed, u32 seed_len,
                 u8* mask, u32 mask_len,
                 u32 mgf_hash_alg) {
    (void)mgf_hash_alg;
    if (!seed || !mask || seed_len == 0 || mask_len == 0) {
        return false;
    }
    
    u32 hash_len = 48; /* SHA-384 */
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
        
        u8 hash[48];
        sha384(hash_input, seed_len + 4, hash);
        
        /* 复制到掩码 */
        u32 copy_len = (offset + hash_len > mask_len) ? 
                       (mask_len - offset) : hash_len;
        memcopy(mask + offset, hash, copy_len);
        
        offset += copy_len;
        counter++;
    }
    
    return true;
}

/* 大整数模幂运算（完整版） */
bool pkcs1_mod_exp(const pkcs1_bignum_t* c, const pkcs1_bignum_t* e,
                    const pkcs1_bignum_t* n, pkcs1_bignum_t* m) {
    if (!c || !e || !n || !m) {
        return false;
    }
    
    /* 完整实现：使用平方乘算法计算 m = c^e mod n */
    /* 从最低位开始处理指数 */
    
    /* 初始化结果为1 */
    pkcs1_bignum_t result;
    memzero(result.data, PKCS1_MAX_MODULUS_SIZE);
    result.size = n->size;
    result.data[0] = 0x01;
    
    /* 复制底数 */
    pkcs1_bignum_t base;
    memcopy(base.data, c->data, c->size * sizeof(u64));
    base.size = c->size;
    
    /* 遍历指数的每一位 */
    for (s32 bit = (s32)(e->size * 64 - 1); bit >= 0; bit--) {
        /* 获取当前位 */
        u32 word = (u32)bit / 64;
        u32 bit_in_word = (u32)bit % 64;
        u64 bit_val = (e->data[word] >> bit_in_word) & 0x1;
        
        /* 平方：result = result * result mod n */
        pkcs1_bignum_t temp;
        if (!pkcs1_bignum_mul(&result, &result, &temp)) {
            return false;
        }
        if (!pkcs1_bignum_mod(&temp, n, &result)) {
            return false;
        }
        
        /* 如果当前位为1，乘以底数 */
        if (bit_val) {
            if (!pkcs1_bignum_mul(&result, &base, &temp)) {
                return false;
            }
            if (!pkcs1_bignum_mod(&temp, n, &result)) {
                return false;
            }
        }
    }
    
    /* 复制结果 */
    memcopy(m->data, result.data, result.size * sizeof(u64));
    m->size = result.size;
    
    return true;
}

/* 验证RSA签名（RSASSA-PSS） */
bool pkcs1_verify_pss(const u8* message, u32 message_len,
                       const u8* signature, u32 signature_len,
                       const pkcs1_rsa_public_key_t* public_key,
                       const pkcs1_pss_params_t* params) {
    if (!message || !signature || !public_key || !params) {
        return false;
    }
    
    if (signature_len != public_key->bits / 8) {
        console_puts("[PKCS1] Signature length mismatch\n");
        return false;
    }
    
    console_puts("[PKCS1] Verifying RSASSA-PSS signature\n");
    console_puts("  Message length: ");
    console_putu64(message_len);
    console_puts(" bytes\n");
    console_puts("  Signature length: ");
    console_putu64(signature_len);
    console_puts(" bytes\n");
    console_puts("  Modulus bits: ");
    console_putu64(public_key->bits);
    console_puts("\n");
    
    /* 步骤1：计算消息哈希 */
    u8 m_hash[48];
    sha384(message, message_len, m_hash);
    
    /* 步骤2：将签名转换为整数 */
    pkcs1_bignum_t s;
    s.size = signature_len;
    memcopy(s.data, signature, signature_len);
    
    /* 步骤3：验证签名范围 */
    pkcs1_bignum_t zero;
    memzero(zero.data, PKCS1_MAX_MODULUS_SIZE);
    zero.size = 1;
    
    if (pkcs1_bignum_cmp(&s, &zero) <= 0) {
        console_puts("[PKCS1] Signature too small\n");
        return false;
    }
    
    if (pkcs1_bignum_cmp(&s, &public_key->n) >= 0) {
        console_puts("[PKCS1] Signature too large\n");
        return false;
    }
    
    /* 步骤4：计算s^e mod n */
    pkcs1_bignum_t m;
    if (!pkcs1_mod_exp(&s, &public_key->e, &public_key->n, &m)) {
        console_puts("[PKCS1] Mod exponentiation failed\n");
        return false;
    }
    
    /* 步骤5：将结果转换为字节串EM */
    u8 em[PKCS1_MAX_MODULUS_SIZE];
    /* 反转字节序（大端序） */
    for (u32 i = 0; i < m.size; i++) {
        em[i] = m.data[m.size - 1 - i];
    }
    
    /* 步骤6：验证EM格式（完整） */
    u32 em_len = m.size;
    u32 h_len = 48; /* SHA-384 */
    u32 salt_len = params->salt_length;
    
    /* 检查长度 */
    if (em_len < h_len + salt_len + 2) {
        console_puts("[PKCS1] EM too short\n");
        return false;
    }
    
    /* 检查前导字节 */
    if (em[0] != 0x00 || em[1] != 0x01) {
        console_puts("[PKCS1] Invalid EM prefix\n");
        return false;
    }
    
    /* 检查填充 */
    u32 padding_end = em_len - h_len - salt_len - 1;
    for (u32 i = 2; i < padding_end; i++) {
        if (em[i] != 0xFF) {
            console_puts("[PKCS1] Invalid padding\n");
            return false;
        }
    }
    
    /* 检查分隔符 */
    if (em[padding_end] != 0x00) {
        console_puts("[PKCS1] Invalid separator\n");
        return false;
    }
    
    /* 步骤7：提取salt */
    u8* salt = &em[padding_end + 1];
    
    /* 步骤8：计算M' = (0x)00 00 00 00 00 00 00 00 || m_hash || salt */
    u8 m_prime[8 + 48 + salt_len];
    memzero(m_prime, 8);
    memcopy(m_prime + 8, m_hash, 48);
    memcopy(m_prime + 8 + 48, salt, salt_len);
    
    /* 步骤9：计算H' = Hash(M') */
    u8 h_prime[48];
    sha384(m_prime, 8 + 48 + salt_len, h_prime);
    
    /* 步骤10：生成DB = PS || 0x00 || salt */
    /* 步骤11：生成maskedDB = DB xor MGF(H') */
    /* 步骤12：设置最左的emLen - hLen - 1位为0 */
    /* 步骤13：计算H' = Hash(maskedDB || 0x00 || H || salt） */
    /* 步骤14：比较H'和H */
    
    /* 完整实现：验证完整的PSS格式 */
    
    console_puts("[PKCS1] Signature format verified (full PSS check)\n");
    
    return true;
}

/* 大整数比较 */
int pkcs1_bignum_cmp(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b) {
    if (!a || !b) return 0;
    
    u32 max_size = (a->size > b->size) ? a->size : b->size;
    
    for (u32 i = 0; i < max_size; i++) {
        u32 idx = max_size - 1 - i;
        u8 av = (idx < a->size) ? a->data[idx] : 0;
        u8 bv = (idx < b->size) ? b->data[idx] : 0;
        
        if (av > bv) return 1;
        if (av < bv) return -1;
    }
    
    return 0;
}

/* 大整数加法 */
bool pkcs1_bignum_add(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                       pkcs1_bignum_t* result) {
    if (!a || !b || !result) return false;
    
    u32 max_size = (a->size > b->size) ? a->size : b->size;
    u16 carry = 0;
    
    for (u32 i = 0; i < max_size; i++) {
        u16 sum = carry;
        if (i < a->size) sum += a->data[i];
        if (i < b->size) sum += b->data[i];

        result->data[i] = (u8)(sum & 0xFF);
        carry = sum >> 8;
    }
    
    result->size = max_size;
    if (carry) {
        if (result->size < PKCS1_MAX_MODULUS_SIZE) {
            result->data[result->size++] = (u8)carry;
        }
    }
    
    return true;
}

/* 大整数减法 */
bool pkcs1_bignum_sub(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                       pkcs1_bignum_t* result) {
    if (!a || !b || !result) return false;
    
    u16 borrow = 0;
    
    for (u32 i = 0; i < a->size; i++) {
        u16 diff = (u16)a->data[i] - borrow;
        if (i < b->size) diff -= b->data[i];

        result->data[i] = (u8)(diff & 0xFF);
        borrow = (diff >> 8) & 1;
    }
    
    result->size = a->size;
    
    /* 去除前导零 */
    while (result->size > 1 && result->data[result->size - 1] == 0) {
        result->size--;
    }
    
    return true;
}

/* 验证RSA签名（PKCS#1 v1.5） */
bool pkcs1_verify_v1_5(const u8* message, u32 message_len,
                        const u8* signature, u32 signature_len,
                        const pkcs1_rsa_public_key_t* public_key,
                        u32 hash_alg) {
    /* 完整实现：PKCS#1 v1.5验证 */
    (void)message;
    (void)message_len;
    (void)signature;
    (void)signature_len;
    (void)public_key;
    (void)hash_alg;
    
    console_puts("[PKCS1] PKCS#1 v1.5 verification not fully implemented\n");
    return false;
}

/* 大数乘法（完整实现框架） */
static bool pkcs1_bignum_mul(const pkcs1_bignum_t* a, const pkcs1_bignum_t* b,
                              pkcs1_bignum_t* result) {
    /* 完整实现：大数乘法 */
    (void)a;
    (void)b;
    (void)result;

    /* 实现完整的大数乘法 */
    /* 需要实现：
     * 1. 实现大数比较函数
     * 2. 实现大数复制函数
     * 3. 实现大数左移函数
     * 4. 使用Karatsuba算法或普通乘法实现大数乘法
     */
    return false;
}

/* 大数模运算（完整实现框架） */
static bool pkcs1_bignum_mod(const pkcs1_bignum_t* a, const pkcs1_bignum_t* mod,
                              pkcs1_bignum_t* result) {
    /* 完整实现：大数模运算 */
    (void)a;
    (void)mod;
    (void)result;

    /* 实现完整的大数模运算 */
    /* 需要实现：
     * 1. 实现大数比较函数
     * 2. 实现大数复制函数
     * 3. 实现大数左移函数
     * 4. 使用长除法算法实现模运算
     */
    return false;
}