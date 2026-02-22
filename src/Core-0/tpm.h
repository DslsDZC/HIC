/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC TPM集成
 * 提供硬件信任根和安全启动支持
 */

#ifndef HIC_KERNEL_TPM_H
#define HIC_KERNEL_TPM_H

#include "types.h"

/* TPM命令 */
typedef enum {
    TPM_CMD_STARTUP,
    TPM_CMD_GET_RANDOM,
    TPM_CMD_PCR_EXTEND,
    TPM_CMD_PCR_READ,
    TPM_CMD_SEAL,
    TPM_CMD_UNSEAL,
    TPM_CMD_SIGN,
    TPM_CMD_VERIFY,
} tpm_command_t;

/* TPM响应 */
typedef struct {
    u8 success;
    u8 data[256];
    u32 data_len;
} tpm_response_t;

/* TPM接口 */

/**
 * 初始化TPM
 */
int tpm_init(void);

/**
 * 执行TPM命令
 */
int tpm_execute_command(tpm_command_t cmd, const u8* input, u32 input_len,
                         tpm_response_t* output);

/**
 * 获取随机数
 */
int tpm_get_random(u8* buffer, u32 length);

/**
 * 扩展PCR寄存器
 */
int tpm_pcr_extend(u32 pcr_index, const u8* data, u32 length);

/**
 * 读取PCR寄存器
 */
int tpm_pcr_read(u32 pcr_index, u8* value, u32* length);

/**
 * 密封数据到TPM
 */
int tpm_seal(const u8* data, u32 length, u8* sealed, u32* sealed_len);

/**
 * 从TPM解密封数据
 */
int tpm_unseal(const u8* sealed, u32 sealed_len, u8* data, u32* length);

#endif /* HIC_KERNEL_TPM_H */