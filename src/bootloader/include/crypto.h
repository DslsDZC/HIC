#ifndef HIK_BOOTLOADER_CRYPTO_H
#define HIK_BOOTLOADER_CRYPTO_H

#include <stdint.h>

// SHA-384 上下文
#define SHA384_DIGEST_SIZE 48
#define SHA384_BLOCK_SIZE 128

typedef struct {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t buffer[SHA384_BLOCK_SIZE];
} sha384_context_t;

// SHA-384 函数
void sha384_init(sha384_context_t *ctx);
void sha384_update(sha384_context_t *ctx, const uint8_t *data, uint64_t len);
void sha384_final(sha384_context_t *ctx, uint8_t digest[SHA384_DIGEST_SIZE]);
void sha384_hash(const uint8_t *data, uint64_t len, uint8_t digest[SHA384_DIGEST_SIZE]);

// RSA-3072 签名验证
#define RSA_3072_MODULUS_SIZE 384  // 3072 bits = 384 bytes
#define RSA_3072_EXPONENT_SIZE 4   // e = 65537

typedef struct {
    uint8_t modulus[RSA_3072_MODULUS_SIZE];
    uint8_t exponent[RSA_3072_EXPONENT_SIZE];
} rsa_3072_public_key_t;

typedef struct {
    uint8_t signature[RSA_3072_MODULUS_SIZE];
} rsa_3072_signature_t;

// RSA PKCS#1 v2.1 签名验证
int rsa_3072_verify_pss(const rsa_3072_public_key_t *pubkey,
                        const uint8_t *hash, uint32_t hash_len,
                        const uint8_t *signature);

// Ed25519 签名验证
#define ED25519_PUBLIC_KEY_SIZE 32
#define ED25519_SIGNATURE_SIZE 64

typedef struct {
    uint8_t public_key[ED25519_PUBLIC_KEY_SIZE];
} ed25519_public_key_t;

typedef struct {
    uint8_t signature[ED25519_SIGNATURE_SIZE];
} ed25519_signature_t;

// Ed25519 签名验证
int ed25519_verify(const ed25519_public_key_t *pubkey,
                   const uint8_t *message, uint64_t message_len,
                   const ed25519_signature_t *signature);

#endif // HIK_BOOTLOADER_CRYPTO_H