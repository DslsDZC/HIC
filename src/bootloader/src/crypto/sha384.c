/**
 * SHA-384 哈希算法实现
 * FIPS 180-4 标准
 */

#include <stdint.h>
#include <string.h>

// SHA-384 初始哈希值
static const uint64_t SHA384_INIT_STATE[8] = {
    0xcbbb9d5dc1059ed8ULL,
    0x629a292a367cd507ULL,
    0x9159015a3070dd17ULL,
    0x152fecd8f70e5939ULL,
    0x67332667ffc00b31ULL,
    0x8eb44a8768581511ULL,
    0xdb0c2e0d64f98fa7ULL,
    0x47b5481dbefa4fa4ULL
};

// SHA-384 常量 K[0..79]
static const uint64_t SHA384_K[80] = {
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

// SHA-384 上下文
typedef struct {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t buffer[128];
} sha384_context_t;

// 右旋转
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

// SHA-384 调度函数
#define SHA384_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA384_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA384_SIGMA0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SHA384_SIGMA1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SHA384_sigma0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define SHA384_sigma1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

/**
 * 初始化SHA-384上下文
 */
void sha384_init(sha384_context_t *ctx)
{
    memcpy(ctx->state, SHA384_INIT_STATE, sizeof(SHA384_INIT_STATE));
    ctx->count[0] = 0;
    ctx->count[1] = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

/**
 * SHA-384 变换函数（处理一个128字节的块）
 */
static void sha384_transform(sha384_context_t *ctx, const uint8_t block[128])
{
    uint64_t W[80];
    uint64_t a, b, c, d, e, f, g, h;
    uint64_t t1, t2;
    int i;
    
    // 准备消息调度
    for (i = 0; i < 16; i++) {
        W[i] = ((uint64_t)block[i * 8] << 56) |
               ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) |
               ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) |
               ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8) |
               ((uint64_t)block[i * 8 + 7]);
    }
    
    for (i = 16; i < 80; i++) {
        W[i] = SHA384_sigma1(W[i - 2]) + W[i - 7] + 
               SHA384_sigma0(W[i - 15]) + W[i - 16];
    }
    
    // 初始化工作变量
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    // 主循环
    for (i = 0; i < 80; i++) {
        t1 = h + SHA384_SIGMA1(e) + SHA384_CH(e, f, g) + SHA384_K[i] + W[i];
        t2 = SHA384_SIGMA0(a) + SHA384_MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // 更新状态
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/**
 * 更新SHA-384哈希
 */
void sha384_update(sha384_context_t *ctx, const uint8_t *data, uint64_t len)
{
    uint64_t i, index, part_len;
    
    // 计算已处理的字节数
    index = (ctx->count[0] >> 3) & 0x7F;
    
    // 更新总位数
    if ((ctx->count[0] += (len << 3)) < (len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += (len >> 29);
    
    part_len = 128 - index;
    
    // 尽可能处理完整的块
    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        sha384_transform(ctx, ctx->buffer);
        
        for (i = part_len; i + 127 < len; i += 128) {
            sha384_transform(ctx, &data[i]);
        }
        
        index = 0;
    } else {
        i = 0;
    }
    
    // 缓冲剩余数据
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

/**
 * 完成SHA-384哈希计算
 */
void sha384_final(sha384_context_t *ctx, uint8_t digest[48])
{
    uint8_t bits[16];
    uint64_t index, pad_len;
    
    // 保存位计数
    bits[0] = (ctx->count[1] >> 56) & 0xFF;
    bits[1] = (ctx->count[1] >> 48) & 0xFF;
    bits[2] = (ctx->count[1] >> 40) & 0xFF;
    bits[3] = (ctx->count[1] >> 32) & 0xFF;
    bits[4] = (ctx->count[1] >> 24) & 0xFF;
    bits[5] = (ctx->count[1] >> 16) & 0xFF;
    bits[6] = (ctx->count[1] >> 8) & 0xFF;
    bits[7] = (ctx->count[1]) & 0xFF;
    bits[8] = (ctx->count[0] >> 56) & 0xFF;
    bits[9] = (ctx->count[0] >> 48) & 0xFF;
    bits[10] = (ctx->count[0] >> 40) & 0xFF;
    bits[11] = (ctx->count[0] >> 32) & 0xFF;
    bits[12] = (ctx->count[0] >> 24) & 0xFF;
    bits[13] = (ctx->count[0] >> 16) & 0xFF;
    bits[14] = (ctx->count[0] >> 8) & 0xFF;
    bits[15] = (ctx->count[0]) & 0xFF;
    
    // 填充
    index = (ctx->count[0] >> 3) & 0x7F;
    pad_len = (index < 112) ? (112 - index) : (240 - index);
    memset(&ctx->buffer[index], 0, pad_len);
    ctx->buffer[index] = 0x80;
    sha384_update(ctx, ctx->buffer, pad_len);
    sha384_update(ctx, bits, 16);
    
    // 输出结果
    for (int i = 0; i < 6; i++) {
        digest[i * 8] = (ctx->state[i] >> 56) & 0xFF;
        digest[i * 8 + 1] = (ctx->state[i] >> 48) & 0xFF;
        digest[i * 8 + 2] = (ctx->state[i] >> 40) & 0xFF;
        digest[i * 8 + 3] = (ctx->state[i] >> 32) & 0xFF;
        digest[i * 8 + 4] = (ctx->state[i] >> 24) & 0xFF;
        digest[i * 8 + 5] = (ctx->state[i] >> 16) & 0xFF;
        digest[i * 8 + 6] = (ctx->state[i] >> 8) & 0xFF;
        digest[i * 8 + 7] = (ctx->state[i]) & 0xFF;
    }
}

/**
 * 计算SHA-384哈希（便捷函数）
 */
void sha384_hash(const uint8_t *data, uint64_t len, uint8_t digest[48])
{
    sha384_context_t ctx;
    sha384_init(&ctx);
    sha384_update(&ctx, data, len);
    sha384_final(&ctx, digest);
}