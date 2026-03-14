/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC 信任根管理接口
 * 
 * 信任根公钥存储在只读数据段，用于验证服务的静态信任链
 */

#ifndef HIC_TRUST_ROOT_H
#define HIC_TRUST_ROOT_H

#include "types.h"

/* 信任根类型 */
#define TRUSTED_ROOT_TYPE_RSA3072  1
#define TRUSTED_ROOT_TYPE_ED25519  2
#define TRUSTED_ROOT_TYPE_HASH     3  /* 开发阶段：SHA-384 哈希 */

/**
 * @brief 获取信任根公钥
 * 
 * @param pubkey 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 实际复制的字节数
 * 
 * 注意：此函数应仅对持有 CAP_VERIFY_ROOT 能力的服务开放
 */
size_t get_trusted_root_key(u8 *pubkey, size_t max_len);

/**
 * @brief 获取信任根类型
 * 
 * @return 类型标识（TRUSTED_ROOT_TYPE_*）
 */
u32 get_trusted_root_type(void);

/**
 * @brief 获取信任根公钥长度
 * 
 * @return 长度（字节）
 */
size_t get_trusted_root_key_len(void);

#endif /* HIC_TRUST_ROOT_H */
