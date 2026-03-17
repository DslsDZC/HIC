/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC加密服务完整实现
 * 
 * 提供密码学操作服务：
 * - AES-128/192/256 加密/解密 (ECB/CBC/CTR/GCM)
 * - SHA-256/384/512 哈希计算
 * - HMAC 消息认证
 * - RSA-PSS/v1.5 签名验证
 * - PBKDF2/HKDF 密钥派生
 */

#include "service.h"
#include "crypto_service.h"
#include "string.h"

/* ========== 辅助函数 ========== */

void secure_memzero(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len--) {
        *p++ = 0;
    }
}

int constant_time_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0 ? 1 : 0;
}

/* ========== AES 实现 ========== */

static const uint8_t aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t aes_inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint8_t rcon[11] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };

#define ROTWORD(x) (((x) << 8) | ((x) >> 24))
#define SUBWORD(x) ((uint32_t)aes_sbox[(x) >> 24] << 24 | \
                    (uint32_t)aes_sbox[((x) >> 16) & 0xff] << 16 | \
                    (uint32_t)aes_sbox[((x) >> 8) & 0xff] << 8 | \
                    (uint32_t)aes_sbox[(x) & 0xff])

static void aes_key_expansion(const uint8_t *key, uint32_t key_bits, uint32_t *round_keys)
{
    uint32_t nk = key_bits / 32;
    uint32_t nr = (key_bits == 128) ? 10 : (key_bits == 192) ? 12 : 14;
    uint32_t nb = 4;
    
    for (uint32_t i = 0; i < nk; i++) {
        round_keys[i] = ((uint32_t)key[4*i] << 24) |
                        ((uint32_t)key[4*i+1] << 16) |
                        ((uint32_t)key[4*i+2] << 8) |
                        ((uint32_t)key[4*i+3]);
    }
    
    for (uint32_t i = nk; i < nb * (nr + 1); i++) {
        uint32_t temp = round_keys[i - 1];
        
        if (i % nk == 0) {
            temp = SUBWORD(ROTWORD(temp)) ^ ((uint32_t)rcon[i / nk] << 24);
        } else if (nk > 6 && i % nk == 4) {
            temp = SUBWORD(temp);
        }
        
        round_keys[i] = round_keys[i - nk] ^ temp;
    }
}

static void aes_sub_bytes(uint8_t state[16])
{
    for (int i = 0; i < 16; i++) {
        state[i] = aes_sbox[state[i]];
    }
}

static void aes_inv_sub_bytes(uint8_t state[16])
{
    for (int i = 0; i < 16; i++) {
        state[i] = aes_inv_sbox[state[i]];
    }
}

static void aes_shift_rows(uint8_t state[16])
{
    uint8_t temp;
    temp = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = temp;
    temp = state[2]; state[2] = state[10]; state[10] = temp;
    temp = state[6]; state[6] = state[14]; state[14] = temp;
    temp = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = temp;
}

static void aes_inv_shift_rows(uint8_t state[16])
{
    uint8_t temp;
    temp = state[13]; state[13] = state[9]; state[9] = state[5]; state[5] = state[1]; state[1] = temp;
    temp = state[2]; state[2] = state[10]; state[10] = temp;
    temp = state[6]; state[6] = state[14]; state[14] = temp;
    temp = state[3]; state[3] = state[7]; state[7] = state[11]; state[11] = state[15]; state[15] = temp;
}

static uint8_t gmul(uint8_t a, uint8_t b)
{
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

static void aes_mix_columns(uint8_t state[16])
{
    for (int i = 0; i < 4; i++) {
        uint8_t a[4], b[4];
        for (int j = 0; j < 4; j++) {
            a[j] = state[i * 4 + j];
            b[j] = gmul(a[j], 2);
        }
        state[i * 4 + 0] = b[0] ^ a[1] ^ b[1] ^ a[2] ^ a[3];
        state[i * 4 + 1] = a[0] ^ b[1] ^ a[2] ^ b[2] ^ a[3];
        state[i * 4 + 2] = a[0] ^ a[1] ^ b[2] ^ a[3] ^ b[3];
        state[i * 4 + 3] = a[0] ^ b[0] ^ a[1] ^ a[2] ^ b[3];
    }
}

static void aes_inv_mix_columns(uint8_t state[16])
{
    for (int i = 0; i < 4; i++) {
        uint8_t a[4];
        for (int j = 0; j < 4; j++) a[j] = state[i * 4 + j];
        state[i * 4 + 0] = gmul(a[0], 0x0e) ^ gmul(a[1], 0x0b) ^ gmul(a[2], 0x0d) ^ gmul(a[3], 0x09);
        state[i * 4 + 1] = gmul(a[0], 0x09) ^ gmul(a[1], 0x0e) ^ gmul(a[2], 0x0b) ^ gmul(a[3], 0x0d);
        state[i * 4 + 2] = gmul(a[0], 0x0d) ^ gmul(a[1], 0x09) ^ gmul(a[2], 0x0e) ^ gmul(a[3], 0x0b);
        state[i * 4 + 3] = gmul(a[0], 0x0b) ^ gmul(a[1], 0x0d) ^ gmul(a[2], 0x09) ^ gmul(a[3], 0x0e);
    }
}

static void aes_add_round_key(uint8_t state[16], const uint32_t *round_key)
{
    for (int i = 0; i < 4; i++) {
        uint32_t k = round_key[i];
        state[i * 4 + 0] ^= (k >> 24) & 0xff;
        state[i * 4 + 1] ^= (k >> 16) & 0xff;
        state[i * 4 + 2] ^= (k >> 8) & 0xff;
        state[i * 4 + 3] ^= k & 0xff;
    }
}

static void aes_encrypt_block(const uint32_t *round_keys, uint32_t nr, const uint8_t input[16], uint8_t output[16])
{
    uint8_t state[16];
    memcpy(state, input, 16);
    
    aes_add_round_key(state, round_keys);
    
    for (uint32_t round = 1; round < nr; round++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, round_keys + round * 4);
    }
    
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, round_keys + nr * 4);
    
    memcpy(output, state, 16);
}

static void aes_decrypt_block(const uint32_t *round_keys, uint32_t nr, const uint8_t input[16], uint8_t output[16])
{
    uint8_t state[16];
    memcpy(state, input, 16);
    
    aes_add_round_key(state, round_keys + nr * 4);
    
    for (int round = nr - 1; round >= 1; round--) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(state, round_keys + round * 4);
        aes_inv_mix_columns(state);
    }
    
    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(state, round_keys);
    
    memcpy(output, state, 16);
}

int aes_init(aes_ctx_t *ctx, const uint8_t *key, uint32_t key_bits, uint8_t mode, uint8_t encrypt)
{
    if (!ctx || !key) return CRYPTO_ERR_INVALID_PARAM;
    if (key_bits != 128 && key_bits != 192 && key_bits != 256) return CRYPTO_ERR_INVALID_KEY;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_bits = key_bits;
    ctx->nr = (key_bits == 128) ? 10 : (key_bits == 192) ? 12 : 14;
    ctx->mode = mode;
    ctx->encrypt = encrypt;
    
    aes_key_expansion(key, key_bits, ctx->round_keys);
    return CRYPTO_SUCCESS;
}

int aes_process(aes_ctx_t *ctx, const uint8_t *input, uint8_t *output, size_t len)
{
    if (!ctx || !input || !output) return CRYPTO_ERR_INVALID_PARAM;
    
    uint8_t iv[16];
    memcpy(iv, ctx->iv, 16);
    
    for (size_t i = 0; i < len; i += 16) {
        uint8_t block[16] = {0};
        size_t block_len = (len - i < 16) ? (len - i) : 16;
        memcpy(block, input + i, block_len);
        
        switch (ctx->mode) {
            case AES_MODE_ECB:
                if (ctx->encrypt) {
                    aes_encrypt_block(ctx->round_keys, ctx->nr, block, output + i);
                } else {
                    aes_decrypt_block(ctx->round_keys, ctx->nr, block, output + i);
                }
                break;
                
            case AES_MODE_CBC:
                if (ctx->encrypt) {
                    for (int j = 0; j < 16; j++) block[j] ^= iv[j];
                    aes_encrypt_block(ctx->round_keys, ctx->nr, block, output + i);
                    memcpy(iv, output + i, 16);
                } else {
                    aes_decrypt_block(ctx->round_keys, ctx->nr, block, output + i);
                    for (int j = 0; j < 16; j++) output[i + j] ^= iv[j];
                    memcpy(iv, input + i, 16);
                }
                break;
                
            case AES_MODE_CTR:
                aes_encrypt_block(ctx->round_keys, ctx->nr, iv, block);
                for (size_t j = 0; j < block_len; j++) {
                    output[i + j] = input[i + j] ^ block[j];
                }
                for (int j = 15; j >= 0; j--) {
                    if (++iv[j] != 0) break;
                }
                break;
                
            default:
                return CRYPTO_ERR_NOT_SUPPORTED;
        }
    }
    
    return CRYPTO_SUCCESS;
}

/* GCM 简化实现 */
static void gcm_ghash(const uint8_t *h, const uint8_t *data, size_t len, uint8_t *hash)
{
    uint8_t y[16] = {0};
    uint8_t h_local[16];
    memcpy(h_local, h, 16);
    
    for (size_t i = 0; i < len; i += 16) {
        for (int j = 0; j < 16 && i + j < len; j++) {
            y[j] ^= data[i + j];
        }
        
        uint8_t temp[16] = {0};
        for (int bit = 0; bit < 128; bit++) {
            if (y[bit / 8] & (0x80 >> (bit % 8))) {
                for (int k = 0; k < 16; k++) temp[k] ^= h_local[k];
            }
            
            uint8_t carry = h_local[15] & 1;
            for (int k = 15; k > 0; k--) {
                h_local[k] = (h_local[k] >> 1) | (h_local[k-1] << 7);
            }
            h_local[0] >>= 1;
            if (carry) h_local[0] ^= 0xe1;
        }
        memcpy(y, temp, 16);
    }
    
    memcpy(hash, y, 16);
}

int aes_gcm_encrypt(const uint8_t *key, uint32_t key_bits, const uint8_t *iv,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *input, uint8_t *output, size_t len,
                    uint8_t *tag)
{
    aes_ctx_t ctx;
    if (aes_init(&ctx, key, key_bits, AES_MODE_ECB, 1) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_INVALID_KEY;
    }
    
    uint8_t h[16] = {0};
    aes_encrypt_block(ctx.round_keys, ctx.nr, h, h);
    
    uint8_t j0[16];
    memcpy(j0, iv, 16);
    j0[15] ^= 1;
    
    uint8_t ctr[16];
    memcpy(ctr, j0, 16);
    ctr[15]++;
    
    /* CTR 加密 */
    for (size_t i = 0; i < len; i += 16) {
        uint8_t block[16];
        aes_encrypt_block(ctx.round_keys, ctx.nr, ctr, block);
        size_t block_len = (len - i < 16) ? (len - i) : 16;
        for (size_t j = 0; j < block_len; j++) {
            output[i + j] = input[i + j] ^ block[j];
        }
        for (int j = 15; j >= 0; j--) {
            if (++ctr[j] != 0) break;
        }
    }
    
    /* GHASH */
    uint8_t ghash_input[256];
    size_t ghash_len = 0;
    
    if (aad && aad_len > 0) {
        size_t aad_blocks = (aad_len + 15) / 16 * 16;
        memset(ghash_input, 0, aad_blocks);
        memcpy(ghash_input, aad, aad_len);
        ghash_len += aad_blocks;
    }
    
    size_t ct_blocks = (len + 15) / 16 * 16;
    memset(ghash_input + ghash_len, 0, ct_blocks);
    memcpy(ghash_input + ghash_len, output, len);
    ghash_len += ct_blocks;
    
    for (int i = 0; i < 8; i++) {
        ghash_input[ghash_len + i] = (aad_len * 8) >> (56 - i * 8);
        ghash_input[ghash_len + 8 + i] = (len * 8) >> (56 - i * 8);
    }
    ghash_len += 16;
    
    uint8_t ghash[16];
    gcm_ghash(h, ghash_input, ghash_len, ghash);
    
    uint8_t s[16];
    aes_encrypt_block(ctx.round_keys, ctx.nr, j0, s);
    for (int i = 0; i < 16; i++) {
        tag[i] = ghash[i] ^ s[i];
    }
    
    secure_memzero(&ctx, sizeof(ctx));
    return CRYPTO_SUCCESS;
}

int aes_gcm_decrypt(const uint8_t *key, uint32_t key_bits, const uint8_t *iv,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *input, uint8_t *output, size_t len,
                    const uint8_t *tag)
{
    uint8_t expected_tag[16];
    if (aes_gcm_encrypt(key, key_bits, iv, aad, aad_len, input, output, len, expected_tag) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_DECRYPT_FAILED;
    }
    
    if (!constant_time_compare(tag, expected_tag, 16)) {
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    return CRYPTO_SUCCESS;
}

/* ========== SHA 实现 ========== */

static const uint32_t sha256_init[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint64_t sha384_init_state[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
    0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
    0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
};

static const uint64_t sha512_init[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0_32(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1_32(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0_32(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG1_32(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))
#define SIGMA0_64(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SIGMA1_64(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0_64(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define sigma1_64(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) | ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1_32(w[i - 2]) + w[i - 7] + SIG0_32(w[i - 15]) + w[i - 16];
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + EP1_32(e) + CH(e, f, g) + sha256_k[i] + w[i];
        uint32_t t2 = EP0_32(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_transform(uint64_t state[8], const uint8_t block[128])
{
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h;
    
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint64_t)block[i * 8] << 56) | ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) | ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) | ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8) | ((uint64_t)block[i * 8 + 7]);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = sigma1_64(w[i - 2]) + w[i - 7] + sigma0_64(w[i - 15]) + w[i - 16];
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    for (int i = 0; i < 80; i++) {
        uint64_t t1 = h + SIGMA1_64(e) + CH(e, f, g) + sha512_k[i] + w[i];
        uint64_t t2 = SIGMA0_64(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

int sha_init(sha_ctx_t *ctx, uint8_t algo)
{
    if (!ctx) return CRYPTO_ERR_INVALID_PARAM;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->algo = algo;
    ctx->finalized = 0;
    
    switch (algo) {
        case SHA_ALGO_256:
            memcpy(ctx->state, sha256_init, 32);
            ctx->digest_size = SHA256_DIGEST_SIZE;
            ctx->block_size = SHA256_BLOCK_SIZE;
            break;
        case SHA_ALGO_384:
            memcpy(ctx->state, sha384_init, 64);
            ctx->digest_size = SHA384_DIGEST_SIZE;
            ctx->block_size = SHA384_BLOCK_SIZE;
            break;
        case SHA_ALGO_512:
            memcpy(ctx->state, sha512_init, 64);
            ctx->digest_size = SHA512_DIGEST_SIZE;
            ctx->block_size = SHA512_BLOCK_SIZE;
            break;
        default:
            return CRYPTO_ERR_NOT_SUPPORTED;
    }
    
    return CRYPTO_SUCCESS;
}

int sha_update(sha_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || ctx->finalized) return CRYPTO_ERR_INVALID_PARAM;
    
    ctx->total_bytes += len;
    uint32_t block_size = ctx->block_size;
    
    /* 处理缓冲区 */
    while (len > 0) {
        uint32_t buffer_used = ctx->total_bytes % block_size;
        if (buffer_used == 0 && len >= block_size) {
            if (ctx->algo == SHA_ALGO_256) {
                sha256_transform((uint32_t *)ctx->state, data);
            } else {
                sha512_transform((uint64_t *)ctx->state, data);
            }
            data += block_size;
            len -= block_size;
        } else {
            uint32_t needed = block_size - buffer_used;
            uint32_t copy = (len < needed) ? len : needed;
            memcpy(ctx->buffer + buffer_used, data, copy);
            data += copy;
            len -= copy;
            
            if (buffer_used + copy == block_size) {
                if (ctx->algo == SHA_ALGO_256) {
                    sha256_transform((uint32_t *)ctx->state, ctx->buffer);
                } else {
                    sha512_transform((uint64_t *)ctx->state, ctx->buffer);
                }
            }
        }
    }
    
    return CRYPTO_SUCCESS;
}

int sha_final(sha_ctx_t *ctx, uint8_t *digest)
{
    if (!ctx || !digest || ctx->finalized) return CRYPTO_ERR_INVALID_PARAM;
    
    ctx->finalized = 1;
    uint32_t block_size = ctx->block_size;
    uint64_t total_bits = ctx->total_bytes * 8;
    uint32_t buffer_used = ctx->total_bytes % block_size;
    
    ctx->buffer[buffer_used++] = 0x80;
    
    if (buffer_used > block_size - 8) {
        memset(ctx->buffer + buffer_used, 0, block_size - buffer_used);
        if (ctx->algo == SHA_ALGO_256) {
            sha256_transform((uint32_t *)ctx->state, ctx->buffer);
        } else {
            sha512_transform((uint64_t *)ctx->state, ctx->buffer);
        }
        buffer_used = 0;
    }
    
    memset(ctx->buffer + buffer_used, 0, block_size - buffer_used);
    
    if (ctx->algo == SHA_ALGO_256) {
        for (int i = 0; i < 8; i++) {
            ctx->buffer[block_size - 8 + i] = (total_bits >> (56 - i * 8)) & 0xff;
        }
        sha256_transform((uint32_t *)ctx->state, ctx->buffer);
        
        uint32_t *state = (uint32_t *)ctx->state;
        for (int i = 0; i < 8; i++) {
            digest[i * 4] = (state[i] >> 24) & 0xff;
            digest[i * 4 + 1] = (state[i] >> 16) & 0xff;
            digest[i * 4 + 2] = (state[i] >> 8) & 0xff;
            digest[i * 4 + 3] = state[i] & 0xff;
        }
    } else {
        for (int i = 0; i < 8; i++) {
            ctx->buffer[block_size - 8 + i] = 0;
        }
        for (int i = 0; i < 8; i++) {
            ctx->buffer[block_size - 16 + i] = (total_bits >> (56 - i * 8)) & 0xff;
        }
        sha512_transform((uint64_t *)ctx->state, ctx->buffer);
        
        uint64_t *state = (uint64_t *)ctx->state;
        int out_words = (ctx->algo == SHA_ALGO_384) ? 6 : 8;
        for (int i = 0; i < out_words; i++) {
            digest[i * 8] = (state[i] >> 56) & 0xff;
            digest[i * 8 + 1] = (state[i] >> 48) & 0xff;
            digest[i * 8 + 2] = (state[i] >> 40) & 0xff;
            digest[i * 8 + 3] = (state[i] >> 32) & 0xff;
            digest[i * 8 + 4] = (state[i] >> 24) & 0xff;
            digest[i * 8 + 5] = (state[i] >> 16) & 0xff;
            digest[i * 8 + 6] = (state[i] >> 8) & 0xff;
            digest[i * 8 + 7] = state[i] & 0xff;
        }
    }
    
    return CRYPTO_SUCCESS;
}

int sha_hash(uint8_t algo, const uint8_t *data, size_t len, uint8_t *digest)
{
    sha_ctx_t ctx;
    if (sha_init(&ctx, algo) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    if (sha_update(&ctx, data, len) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    return sha_final(&ctx, digest);
}

/* SHA-384 兼容接口 */
void sha384_init(sha384_ctx_t *ctx)
{
    for (int i = 0; i < 8; i++) ctx->state[i] = sha384_init_state[i];
    ctx->count[0] = ctx->count[1] = 0;
    ctx->buffer_len = 0;
}

void sha384_update(sha384_ctx_t *ctx, const uint8_t *data, size_t len)
{
    uint64_t bit_count = (uint64_t)len << 3;
    ctx->count[1] += bit_count;
    if (ctx->count[1] < bit_count) ctx->count[0]++;
    ctx->count[0] += (uint64_t)len >> 61;
    
    size_t buffer_used = ctx->buffer_len;
    if (buffer_used > 0) {
        size_t buffer_free = SHA384_BLOCK_SIZE - buffer_used;
        if (len >= buffer_free) {
            memcpy(ctx->buffer + buffer_used, data, buffer_free);
            sha512_transform(ctx->state, ctx->buffer);
            data += buffer_free;
            len -= buffer_free;
            ctx->buffer_len = 0;
        } else {
            memcpy(ctx->buffer + buffer_used, data, len);
            ctx->buffer_len += len;
            return;
        }
    }
    
    while (len >= SHA384_BLOCK_SIZE) {
        sha512_transform(ctx->state, data);
        data += SHA384_BLOCK_SIZE;
        len -= SHA384_BLOCK_SIZE;
    }
    
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

void sha384_final(sha384_ctx_t *ctx, uint8_t digest[SHA384_DIGEST_SIZE])
{
    size_t buffer_used = ctx->buffer_len;
    ctx->buffer[buffer_used++] = 0x80;
    
    if (buffer_used > 112) {
        memset(ctx->buffer + buffer_used, 0, SHA384_BLOCK_SIZE - buffer_used);
        sha512_transform(ctx->state, ctx->buffer);
        buffer_used = 0;
    }
    
    memset(ctx->buffer + buffer_used, 0, 112 - buffer_used);
    
    for (int i = 0; i < 8; i++) {
        ctx->buffer[112 + i] = (ctx->count[0] >> (56 - i * 8)) & 0xff;
        ctx->buffer[120 + i] = (ctx->count[1] >> (56 - i * 8)) & 0xff;
    }
    sha512_transform(ctx->state, ctx->buffer);
    
    for (int i = 0; i < 6; i++) {
        digest[i * 8] = (ctx->state[i] >> 56) & 0xff;
        digest[i * 8 + 1] = (ctx->state[i] >> 48) & 0xff;
        digest[i * 8 + 2] = (ctx->state[i] >> 40) & 0xff;
        digest[i * 8 + 3] = (ctx->state[i] >> 32) & 0xff;
        digest[i * 8 + 4] = (ctx->state[i] >> 24) & 0xff;
        digest[i * 8 + 5] = (ctx->state[i] >> 16) & 0xff;
        digest[i * 8 + 6] = (ctx->state[i] >> 8) & 0xff;
        digest[i * 8 + 7] = ctx->state[i] & 0xff;
    }
    
    secure_memzero(ctx, sizeof(*ctx));
}

int sha384_hash(const uint8_t *data, size_t len, uint8_t digest[SHA384_DIGEST_SIZE])
{
    sha384_ctx_t ctx;
    sha384_init(&ctx);
    sha384_update(&ctx, data, len);
    sha384_final(&ctx, digest);
    return CRYPTO_SUCCESS;
}

/* ========== HMAC 实现 ========== */

int hmac_init(hmac_ctx_t *ctx, uint8_t algo, const uint8_t *key, size_t key_len)
{
    if (!ctx || !key) return CRYPTO_ERR_INVALID_PARAM;
    
    uint32_t block_size;
    switch (algo) {
        case SHA_ALGO_256: block_size = SHA256_BLOCK_SIZE; break;
        case SHA_ALGO_384:
        case SHA_ALGO_512: block_size = SHA512_BLOCK_SIZE; break;
        default: return CRYPTO_ERR_NOT_SUPPORTED;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->algo = algo;
    
    if (sha_init(&ctx->sha_ctx, algo) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    
    uint8_t k_ipad[128];
    memset(k_ipad, 0, block_size);
    
    if (key_len > block_size) {
        sha_hash(algo, key, key_len, k_ipad);
        ctx->key_len = (algo == SHA_ALGO_256) ? 32 : 64;
    } else {
        memcpy(k_ipad, key, key_len);
        ctx->key_len = key_len;
    }
    memcpy(ctx->key, k_ipad, block_size);
    
    for (uint32_t i = 0; i < block_size; i++) {
        k_ipad[i] ^= 0x36;
    }
    
    return sha_update(&ctx->sha_ctx, k_ipad, block_size);
}

int hmac_update(hmac_ctx_t *ctx, const uint8_t *data, size_t len)
{
    return sha_update(&ctx->sha_ctx, data, len);
}

int hmac_final(hmac_ctx_t *ctx, uint8_t *mac)
{
    if (!ctx || !mac) return CRYPTO_ERR_INVALID_PARAM;
    
    uint8_t inner_hash[64];
    if (sha_final(&ctx->sha_ctx, inner_hash) != CRYPTO_SUCCESS) {
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    uint32_t block_size = ctx->sha_ctx.block_size;
    uint8_t k_opad[128];
    memset(k_opad, 0, block_size);
    memcpy(k_opad, ctx->key, block_size);
    
    for (uint32_t i = 0; i < block_size; i++) {
        k_opad[i] ^= 0x5c;
    }
    
    if (sha_init(&ctx->sha_ctx, ctx->algo) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    if (sha_update(&ctx->sha_ctx, k_opad, block_size) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    if (sha_update(&ctx->sha_ctx, inner_hash, ctx->sha_ctx.digest_size) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    
    return sha_final(&ctx->sha_ctx, mac);
}

int hmac_hash(uint8_t algo, const uint8_t *key, size_t key_len,
              const uint8_t *data, size_t data_len, uint8_t *mac)
{
    hmac_ctx_t ctx;
    if (hmac_init(&ctx, algo, key, key_len) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    if (hmac_update(&ctx, data, data_len) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    return hmac_final(&ctx, mac);
}

/* ========== RSA 实现 (保留原有) ========== */

static int be_int_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

static void be_int_subtract(uint8_t *result, const uint8_t *a, const uint8_t *b, size_t len)
{
    int borrow = 0;
    for (int i = len - 1; i >= 0; i--) {
        int diff = a[i] - b[i] - borrow;
        if (diff < 0) { diff += 256; borrow = 1; }
        else { borrow = 0; }
        result[i] = (uint8_t)diff;
    }
}

static void be_int_modmul(uint8_t *result, const uint8_t *a, const uint8_t *b,
                          const uint8_t *n, size_t len)
{
    uint8_t temp[2 * RSA_MAX_KEY_SIZE];
    memset(temp, 0, 2 * len);
    
    for (size_t i = 0; i < len; i++) {
        int carry = 0;
        for (size_t j = 0; j < len; j++) {
            size_t idx = len - 1 - i - j + len;
            if (idx < 2 * len) {
                int product = temp[idx] + a[len - 1 - i] * b[len - 1 - j] + carry;
                temp[idx] = product & 0xFF;
                carry = product >> 8;
            }
        }
    }
    
    while (be_int_compare(temp, n, len) >= 0) {
        be_int_subtract(temp, temp, n, 2 * len);
    }
    
    memcpy(result, temp + len, len);
    secure_memzero(temp, sizeof(temp));
}

static int rsa_modexp(uint8_t *result, const uint8_t *base, uint32_t exp,
                      const uint8_t *n, size_t key_size)
{
    if (key_size == 0 || key_size > RSA_MAX_KEY_SIZE) return CRYPTO_ERR_INVALID_PARAM;
    
    uint8_t temp[RSA_MAX_KEY_SIZE];
    uint8_t acc[RSA_MAX_KEY_SIZE];
    
    memset(acc, 0, key_size);
    acc[key_size - 1] = 1;
    memcpy(temp, base, key_size);
    
    while (exp > 0) {
        if (exp & 1) be_int_modmul(acc, acc, temp, n, key_size);
        be_int_modmul(temp, temp, temp, n, key_size);
        exp >>= 1;
    }
    
    memcpy(result, acc, key_size);
    secure_memzero(temp, sizeof(temp));
    secure_memzero(acc, sizeof(acc));
    return CRYPTO_SUCCESS;
}

int rsa_verify_v15(const uint8_t *message, size_t message_len,
                   const uint8_t *signature, size_t sig_len,
                   const rsa_public_key_t *public_key,
                   uint8_t hash_algo)
{
    uint8_t decrypted[RSA_MAX_KEY_SIZE];
    uint8_t digest[SHA384_DIGEST_SIZE];
    
    if (!message || !signature || !public_key) return CRYPTO_ERR_INVALID_PARAM;
    if (sig_len != public_key->key_size) return CRYPTO_ERR_INVALID_SIG;
    
    (void)hash_algo;
    
    if (sha384_hash(message, message_len, digest) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    
    if (rsa_modexp(decrypted, signature, public_key->exponent,
                   public_key->modulus, public_key->key_size) != CRYPTO_SUCCESS) {
        secure_memzero(digest, sizeof(digest));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    if (decrypted[0] != 0x00 || decrypted[1] != 0x01) {
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    size_t i = 2;
    while (i < public_key->key_size && decrypted[i] == 0xFF) i++;
    
    if (i >= public_key->key_size || decrypted[i] != 0x00) {
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    i++;
    
    size_t expected_hash_offset = i + 19;
    int match = constant_time_compare(decrypted + expected_hash_offset, digest, SHA384_DIGEST_SIZE);
    
    secure_memzero(decrypted, sizeof(decrypted));
    secure_memzero(digest, sizeof(digest));
    return match ? CRYPTO_SUCCESS : CRYPTO_ERR_SIG_MISMATCH;
}

int mgf1_generate(const uint8_t *seed, size_t seed_len,
                  uint8_t *mask, size_t mask_len,
                  uint8_t hash_algo)
{
    uint8_t counter[4];
    uint8_t digest[SHA384_DIGEST_SIZE];
    size_t done = 0;
    
    (void)hash_algo;
    if (!seed || !mask || mask_len == 0) return CRYPTO_ERR_INVALID_PARAM;
    
    for (uint32_t i = 0; done < mask_len; i++) {
        counter[0] = (i >> 24) & 0xff;
        counter[1] = (i >> 16) & 0xff;
        counter[2] = (i >> 8) & 0xff;
        counter[3] = i & 0xff;
        
        sha384_ctx_t ctx;
        sha384_init(&ctx);
        sha384_update(&ctx, seed, seed_len);
        sha384_update(&ctx, counter, 4);
        sha384_final(&ctx, digest);
        
        size_t copy_len = (mask_len - done < SHA384_DIGEST_SIZE) ? (mask_len - done) : SHA384_DIGEST_SIZE;
        memcpy(mask + done, digest, copy_len);
        done += copy_len;
    }
    
    secure_memzero(digest, sizeof(digest));
    return CRYPTO_SUCCESS;
}

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
    size_t em_len, hash_len = SHA384_DIGEST_SIZE;
    
    if (!message || !signature || !public_key) return CRYPTO_ERR_INVALID_PARAM;
    if (sig_len != public_key->key_size) return CRYPTO_ERR_INVALID_SIG;
    
    em_len = public_key->key_size;
    
    if (sha384_hash(message, message_len, m_hash) != CRYPTO_SUCCESS) return CRYPTO_ERR_HASH_FAILED;
    
    if (rsa_modexp(decrypted, signature, public_key->exponent,
                   public_key->modulus, public_key->key_size) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    if (em_len < hash_len + salt_len + 2) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_INVALID_SIG;
    }
    
    if (decrypted[em_len - 1] != 0xBC) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    for (int i = 0; i < 8; i++) m_prime[i] = 0x00;
    memcpy(m_prime + 8, m_hash, hash_len);
    
    size_t salt_offset = em_len - hash_len - salt_len - 1;
    memcpy(m_prime + 8 + hash_len, decrypted + salt_offset, salt_len);
    
    if (sha384_hash(m_prime, 8 + hash_len + salt_len, h_prime) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    size_t h_offset = em_len - hash_len - 1;
    if (!constant_time_compare(decrypted + h_offset, h_prime, hash_len)) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        secure_memzero(h_prime, sizeof(h_prime));
        return CRYPTO_ERR_SIG_MISMATCH;
    }
    
    if (mgf1_generate(decrypted + h_offset, hash_len, db_mask, h_offset, 0) != CRYPTO_SUCCESS) {
        secure_memzero(m_hash, sizeof(m_hash));
        secure_memzero(decrypted, sizeof(decrypted));
        secure_memzero(m_prime, sizeof(m_prime));
        secure_memzero(h_prime, sizeof(h_prime));
        return CRYPTO_ERR_HASH_FAILED;
    }
    
    for (size_t i = 0; i < h_offset; i++) db[i] = decrypted[i] ^ db_mask[i];
    
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
    
    secure_memzero(m_hash, sizeof(m_hash));
    secure_memzero(decrypted, sizeof(decrypted));
    secure_memzero(m_prime, sizeof(m_prime));
    secure_memzero(h_prime, sizeof(h_prime));
    secure_memzero(db_mask, sizeof(db_mask));
    secure_memzero(db, sizeof(db));
    
    return CRYPTO_SUCCESS;
}

/* ========== 密钥派生 ========== */

int pbkdf2_derive(const uint8_t *password, size_t password_len,
                  const uint8_t *salt, size_t salt_len,
                  uint8_t *key, size_t key_len, uint32_t iterations)
{
    uint8_t *key_out = key;
    size_t derived = 0;
    uint32_t block_num = 1;
    
    while (derived < key_len) {
        uint8_t u[32], f[32];
        uint8_t salt_block[128];
        
        size_t sb_len = salt_len;
        memcpy(salt_block, salt, salt_len);
        salt_block[sb_len++] = (block_num >> 24) & 0xff;
        salt_block[sb_len++] = (block_num >> 16) & 0xff;
        salt_block[sb_len++] = (block_num >> 8) & 0xff;
        salt_block[sb_len++] = block_num & 0xff;
        
        hmac_hash(SHA_ALGO_256, password, password_len, salt_block, sb_len, u);
        memcpy(f, u, 32);
        
        for (uint32_t i = 1; i < iterations; i++) {
            hmac_hash(SHA_ALGO_256, password, password_len, u, 32, u);
            for (int j = 0; j < 32; j++) f[j] ^= u[j];
        }
        
        size_t copy = (key_len - derived < 32) ? (key_len - derived) : 32;
        memcpy(key_out + derived, f, copy);
        derived += copy;
        block_num++;
    }
    
    return CRYPTO_SUCCESS;
}

int hkdf_derive(const uint8_t *ikm, size_t ikm_len,
                const uint8_t *salt, size_t salt_len,
                const uint8_t *info, size_t info_len,
                uint8_t *okm, size_t okm_len)
{
    uint8_t prk[32];
    uint8_t t[32];
    uint8_t counter = 1;
    size_t generated = 0;
    
    if (salt && salt_len > 0) {
        hmac_hash(SHA_ALGO_256, salt, salt_len, ikm, ikm_len, prk);
    } else {
        uint8_t zero_salt[32] = {0};
        hmac_hash(SHA_ALGO_256, zero_salt, 32, ikm, ikm_len, prk);
    }
    
    while (generated < okm_len) {
        uint8_t input[256];
        size_t input_len = 0;
        
        if (generated > 0) {
            memcpy(input, t, 32);
            input_len = 32;
        }
        
        if (info && info_len > 0) {
            memcpy(input + input_len, info, info_len);
            input_len += info_len;
        }
        
        input[input_len++] = counter;
        
        hmac_hash(SHA_ALGO_256, prk, 32, input, input_len, t);
        
        size_t copy = (okm_len - generated < 32) ? (okm_len - generated) : 32;
        memcpy(okm + generated, t, copy);
        generated += copy;
        counter++;
    }
    
    return CRYPTO_SUCCESS;
}

/* ========== 服务接口实现 ========== */

hic_status_t service_init(void) { return HIC_SUCCESS; }
hic_status_t service_start(void) { return HIC_SUCCESS; }
hic_status_t service_stop(void) { return HIC_SUCCESS; }
hic_status_t service_cleanup(void) { return HIC_SUCCESS; }

hic_status_t service_get_info(char* buffer, u32 size)
{
    if (buffer && size > 0) {
        const char *info = "Crypto Service v2.0.0\n"
                          "Endpoints: aes(0x6700), sha(0x6701), hmac(0x6702), rsa_pss(0x6703), rsa_v15(0x6704), kdf(0x6705)";
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
