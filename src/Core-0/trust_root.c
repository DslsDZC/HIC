/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 信任根管理
 * 
 * 信任根公钥存储在只读数据段，不可修改
 * 用于验证服务的静态信任链起点
 */

#include "types.h"

/* ==================== 信任根公钥（只读数据段） ==================== */

/*
 * 开发阶段信任根公钥（SHA-384 哈希）
 * 生产环境应替换为真实的 RSA/Ed25519 公钥
 * 
 * 存储在 .rodata 段，受 MMU 写保护
 */
__attribute__((section(".rodata")))
static const u8 g_trusted_root_public_key[384] = {
    /* RSA-3072 公钥占位符（384字节） */
    /* 实际部署时应使用构建系统生成的真实公钥 */
    
    /* 开发阶段：使用 SHA-384 哈希作为信任根标识 */
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    
    /* 其余部分填充零 */
    /* [48..383] = 0 */
};

/* 信任根公钥长度 */
#define TRUSTED_ROOT_KEY_LEN 384

/* 信任根公钥类型标识 */
#define TRUSTED_ROOT_TYPE_RSA3072  1
#define TRUSTED_ROOT_TYPE_ED25519  2
#define TRUSTED_ROOT_TYPE_HASH     3  /* 开发阶段 */

static const u32 g_trusted_root_type = TRUSTED_ROOT_TYPE_HASH;

/* ==================== 公开接口 ==================== */

/**
 * @brief 获取信任根公钥
 * 
 * @param pubkey 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 实际复制的字节数
 * 
 * 注意：此函数应仅对持有特定能力的服务开放
 */
size_t get_trusted_root_key(u8 *pubkey, size_t max_len)
{
    if (!pubkey || max_len == 0) {
        return 0;
    }
    
    /* 只复制请求的长度 */
    size_t copy_len = (max_len < TRUSTED_ROOT_KEY_LEN) ? max_len : TRUSTED_ROOT_KEY_LEN;
    
    for (size_t i = 0; i < copy_len; i++) {
        pubkey[i] = g_trusted_root_public_key[i];
    }
    
    return copy_len;
}

/**
 * @brief 获取信任根类型
 * 
 * @return 类型标识
 */
u32 get_trusted_root_type(void)
{
    return g_trusted_root_type;
}

/**
 * @brief 获取信任根公钥长度
 * 
 * @return 长度（字节）
 */
size_t get_trusted_root_key_len(void)
{
    return TRUSTED_ROOT_KEY_LEN;
}
