/**
 * HIC内核映像验证
 */

#include <stdint.h>
#include <string.h>
#include "kernel_image.h"
#include "crypto.h"

/**
 * 验证HIC内核映像
 */
hic_verify_result_t hic_image_verify(void *image_data, uint64_t size)
{
    hic_image_header_t *header;
    hic_signature_t *sig;
    uint8_t *image_body;
    uint64_t body_size;
    uint8_t computed_hash[48];
    
    // 检查映像大小
    if (size < sizeof(hic_image_header_t)) {
        return HIC_VERIFY_INVALID_MAGIC;
    }
    
    header = (hic_image_header_t *)image_data;
    
    // 验证魔数
    if (memcmp(header->magic, HIC_IMG_MAGIC, 8) != 0) {
        return HIC_VERIFY_INVALID_MAGIC;
    }
    
    // 验证架构
    if (header->arch_id != HIC_ARCH_X86_64) {
        return HIC_VERIFY_WRONG_ARCH;
    }
    
    // 验证版本（简单检查）
    if (header->version < 0x0100) {
        return HIC_VERIFY_UNSUPPORTED_VERSION;
    }
    
    // 验证大小
    if (header->image_size != size) {
        return HIC_VERIFY_LOAD_ERROR;
    }
    
    // 如果没有签名，则只验证结构完整性
    if (header->signature_size == 0) {
        return HIC_VERIFY_SUCCESS;
    }
    
    // 计算映像体哈希（不包括签名部分）
    body_size = header->signature_offset;
    image_body = (uint8_t *)image_data;
    sha384_hash(image_body, body_size, computed_hash);
    
    /* 获取签名 */
    sig = (hic_signature_t *)(image_body + header->signature_offset);
    
    /* 验证签名 */
    /* 完整实现：从安全存储加载公钥并验证签名 */
    hic_public_key_t* pub_key = get_trusted_public_key(sig->key_id);
    
    if (pub_key == NULL) {
        log_error("未找到公钥: %08x\n", sig->key_id);
        return HIC_VERIFY_INVALID_SIGNATURE;
    }
    
    switch (sig->algorithm) {
        case HIC_SIG_ALGO_RSA_3072_SHA384: {
            /* RSA-3072 + SHA-384 验证 */
            /* 使用完整的大数运算库实现 */
            bool verify_result = rsa_verify_sha384(
                pub_key->key.rsa.modulus,
                pub_key->key.rsa.exponent,
                sig->signature,
                sig->signature_size,
                computed_hash,
                48  /* SHA-384 = 48字节 */
            );
            
            if (verify_result) {
                log_info("RSA-3072签名验证成功\n");
                return HIC_VERIFY_SUCCESS;
            } else {
                log_error("RSA-3072签名验证失败\n");
                return HIC_VERIFY_INVALID_SIGNATURE;
            }
        }
            
        case HIC_SIG_ALGO_ED25519_SHA512: {
            /* Ed25519 验证 */
            /* 使用Ed25519曲线运算实现 */
            bool verify_result = ed25519_verify(
                pub_key->key.ed25519.public_key,
                computed_hash,
                48,  /* SHA-384 = 48字节 */
                sig->signature,
                sig->signature_size
            );
            
            if (verify_result) {
                log_info("Ed25519签名验证成功\n");
                return HIC_VERIFY_SUCCESS;
            } else {
                log_error("Ed25519签名验证失败\n");
                return HIC_VERIFY_INVALID_SIGNATURE;
            }
        }
            
        default:
            log_error("不支持的签名算法: %u\n", sig->algorithm);
            return HIC_VERIFY_INVALID_SIGNATURE;
    }
}

/**
 * 加载HIC内核映像
 */
hic_verify_result_t hic_image_load(hic_image_loader_t *loader, void *target_addr)
{
    hic_image_header_t *header;
    hic_segment_entry_t *segments;
    uint64_t i;
    
    if (!loader || !loader->image_data) {
        return HIC_VERIFY_LOAD_ERROR;
    }
    
    header = (hic_image_header_t *)loader->image_data;
    
    // 验证头部
    if (memcmp(header->magic, HIC_IMG_MAGIC, 8) != 0) {
        return HIC_VERIFY_INVALID_MAGIC;
    }
    
    // 获取段表
    segments = (hic_segment_entry_t *)((uint8_t *)loader->image_data + 
                                      header->segment_table_offset);
    
    // 加载各段
    for (i = 0; i < header->segment_count; i++) {
        hic_segment_entry_t *seg = &segments[i];
        void *dest;
        void *src;
        
        dest = (uint8_t *)target_addr + seg->memory_offset;
        src = (uint8_t *)loader->image_data + seg->file_offset;
        
        // 清零BSS段
        if (seg->type == HIC_SEGMENT_TYPE_BSS) {
            memset(dest, 0, seg->memory_size);
        } else {
            // 复制其他段
            if (seg->file_size > 0) {
                memcpy(dest, src, seg->file_size);
            }
            
            // 清零剩余部分
            if (seg->memory_size > seg->file_size) {
                memset((uint8_t *)dest + seg->file_size, 0, 
                      seg->memory_size - seg->file_size);
            }
        }
    }
    
    loader->loaded_base = target_addr;
    
    return HIC_VERIFY_SUCCESS;
}

/**
 * 获取内核入口点
 */
uint64_t hic_image_get_entry_point(hic_image_loader_t *loader)
{
    hic_image_header_t *header;
    
    if (!loader || !loader->image_data) {
        return 0;
    }
    
    header = (hic_image_header_t *)loader->image_data;
    
    if (loader->loaded_base) {
        return (uint64_t)loader->loaded_base + header->entry_point;
    }
    
    return header->entry_point;
}

/**
 * 获取内核配置表
 */
void *hic_image_get_config(hic_image_loader_t *loader, uint64_t *size)
{
    hic_image_header_t *header;
    
    if (!loader || !loader->image_data) {
        return NULL;
    }
    
    header = (hic_image_header_t *)loader->image_data;
    
    if (size) {
        *size = header->config_table_size;
    }
    
    return (uint8_t *)loader->image_data + header->config_table_offset;
}