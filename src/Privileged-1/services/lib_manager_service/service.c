/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC共享库管理服务实现
 * 
 * 运行在 Privileged-1 层，负责：
 * - 库的注册和验证
 * - 库的查询和引用计数
 * - 符号解析
 * - 版本管理
 * - 依赖解析
 * - 滚动更新
 */

#include "service.h"
#include "lib_manager.h"
#include "crypto_service.h"
#include "string.h"

/* ========== Core-0 原语接口（通过模块原语调用） ========== */

/* 外部原语函数（由 Core-0 提供） */
extern uint64_t module_memory_alloc(uint32_t domain_id, uint64_t size, uint64_t *phys_addr);
extern void module_memory_free(uint32_t domain_id, uint64_t phys_addr, uint64_t size);
extern uint64_t module_cap_create_memory(uint32_t domain_id, uint64_t base, uint64_t size, uint32_t rights);
extern void module_memcpy(void *dst, const void *src, uint64_t size);
extern uint64_t module_get_timestamp(void);

/* 能力权限 */
#define CAP_RIGHT_READ      (1U << 0)
#define CAP_RIGHT_WRITE     (1U << 1)
#define CAP_RIGHT_EXEC      (1U << 2)

/* 页大小 */
#define PAGE_SIZE           4096
#define PAGE_ALIGN(x)       (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* ========== 全局管理器 ========== */

static lib_manager_t g_lib_manager;

/* ========== 内部辅助函数 ========== */

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
 * 内存比较
 */
static int mem_cmp(const uint8_t *a, const uint8_t *b, int len)
{
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

/**
 * 内存复制
 */
static void mem_cpy(uint8_t *dst, const uint8_t *src, int len)
{
    for (int i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

/**
 * 内存清零
 */
static void mem_zero(void *ptr, uint64_t size)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint64_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

/**
 * 字符串长度
 */
static int str_len(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * 字符串比较
 */
static int str_cmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(const uint8_t *)a - *(const uint8_t *)b;
}

/**
 * 查找库实例
 */
static lib_instance_t* find_instance(const uint8_t uuid[16], uint32_t version)
{
    uint32_t major, minor, patch;
    hiclib_unpack_version(version, &major, &minor, &patch);
    
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        lib_instance_t *inst = &g_lib_manager.instances[i];
        
        if (inst->state == HICLIB_STATE_UNLOADED) continue;
        
        /* 匹配UUID */
        if (mem_cmp(inst->uuid, uuid, 16) != 0) continue;
        
        /* 如果指定了版本，也要匹配版本 */
        if (version != 0) {
            if (inst->major == major && inst->minor == minor && inst->patch == patch) {
                return inst;
            }
        } else {
            return inst;
        }
    }
    
    return NULL;
}

/**
 * 查找最佳版本
 */
static lib_instance_t* find_best_version(const uint8_t uuid[16], 
                                          uint32_t min_version, uint32_t max_version)
{
    uint32_t min_major, min_minor, min_patch;
    uint32_t max_major, max_minor, max_patch;
    
    hiclib_unpack_version(min_version, &min_major, &min_minor, &min_patch);
    hiclib_unpack_version(max_version, &max_major, &max_minor, &max_patch);
    
    lib_instance_t *best = NULL;
    uint32_t best_version = 0;
    
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        lib_instance_t *inst = &g_lib_manager.instances[i];
        
        if (inst->state == HICLIB_STATE_UNLOADED) continue;
        
        /* 匹配UUID */
        if (mem_cmp(inst->uuid, uuid, 16) != 0) continue;
        
        /* 检查版本约束 */
        if (hiclib_version_satisfies(inst->major, inst->minor, inst->patch,
                                      min_major, min_minor,
                                      max_major, max_minor)) {
            uint32_t inst_version = hiclib_pack_version(inst->major, inst->minor, inst->patch);
            if (best == NULL || inst_version > best_version) {
                best = inst;
                best_version = inst_version;
            }
        }
    }
    
    return best;
}

/**
 * 查找空闲槽位
 */
static lib_instance_t* find_free_slot(void)
{
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        if (g_lib_manager.instances[i].state == HICLIB_STATE_UNLOADED) {
            return &g_lib_manager.instances[i];
        }
    }
    return NULL;
}

/**
 * 验证库签名
 * 调用加密服务验证RSA签名
 */
static int verify_library_signature(const hiclib_header_t *header, 
                                     const uint8_t *lib_data,
                                     uint32_t lib_size)
{
    /* 检查签名偏移和大小是否有效 */
    if (header->signature_offset == 0 || header->signature_size == 0) {
        /* 无签名的库，根据策略拒绝 */
        return LIB_ERR_SIGNATURE_INVALID;
    }
    
    /* 检查签名区域是否在文件范围内 */
    if (header->signature_offset + header->signature_size > lib_size) {
        return LIB_ERR_SIGNATURE_INVALID;
    }
    
    /* 提取签名 */
    const uint8_t *signature = lib_data + header->signature_offset;
    uint32_t sig_len = header->signature_size;
    
    /* 计算签名的数据范围（整个库数据减去签名区域） */
    /* 方法：先计算整个库的哈希，再验证签名 */
    uint8_t digest[SHA384_DIGEST_SIZE];
    
    /* 分段计算哈希：
     * - 头部（不包括签名偏移/大小字段）
     * - 段表到签名区域之前
     * - 签名区域之后的数据
     */
    sha384_ctx_t ctx;
    sha384_init(&ctx);
    
    /* 哈希头部（排除签名字段） */
    sha384_update(&ctx, (const uint8_t *)header, 64); /* 前64字节 */
    sha384_update(&ctx, (const uint8_t *)header + 80, 48); /* 跳过签名字段 */
    
    /* 哈希签名区域之前的数据 */
    if (header->signature_offset > sizeof(hiclib_header_t)) {
        sha384_update(&ctx, lib_data + sizeof(hiclib_header_t),
                      header->signature_offset - sizeof(hiclib_header_t));
    }
    
    /* 哈希签名区域之后的数据 */
    uint32_t after_sig_offset = header->signature_offset + header->signature_size;
    if (after_sig_offset < lib_size) {
        sha384_update(&ctx, lib_data + after_sig_offset,
                      lib_size - after_sig_offset);
    }
    
    sha384_final(&ctx, digest);
    
    /* 验证签名 */
    /* 使用内置公钥验证（简化：使用固定公钥） */
    /* 在实际系统中，公钥应从证书链获取 */
    rsa_public_key_t public_key = {
        .key_size = 256,  /* RSA-2048 */
        .exponent = 65537, /* 常用公钥指数 */
        .modulus = {
            /* 示例公钥模数（实际系统应从安全存储读取） */
            0x00, 0xb5, 0x2c, 0x0b, 0x13, 0x7e, 0x21, 0x8c,
            0x33, 0x29, 0x6f, 0xa5, 0x1d, 0x8c, 0x1a, 0x9b,
            0x5f, 0x2a, 0xb1, 0x3e, 0xc7, 0x8e, 0x6d, 0x28,
            0xd1, 0x8f, 0x44, 0x0b, 0x6e, 0x54, 0x1b, 0x5e,
            0x2f, 0x96, 0x4d, 0x1f, 0x90, 0xf2, 0x98, 0x07,
            0x8b, 0x2f, 0x67, 0x22, 0x53, 0x35, 0xe7, 0x40,
            0x87, 0xc6, 0xa9, 0x41, 0xf3, 0x5c, 0x52, 0x1b,
            0x0e, 0x38, 0x16, 0xd3, 0xf1, 0x0d, 0x5e, 0x03,
            0x94, 0x24, 0x4b, 0x5b, 0x80, 0x57, 0x39, 0xe7,
            0x0a, 0x87, 0x0d, 0x23, 0x2d, 0x9c, 0xbd, 0xe7,
            0x1f, 0x7a, 0xf1, 0x7a, 0x7f, 0x6f, 0x14, 0x16,
            0x17, 0xde, 0x4e, 0xb9, 0x1e, 0x9b, 0x6b, 0xe0,
            0x8d, 0xb5, 0x6a, 0x88, 0x17, 0x09, 0x59, 0x1a,
            0x93, 0x84, 0xdb, 0xa3, 0x58, 0x09, 0x23, 0xe5,
            0x19, 0x20, 0x37, 0x3e, 0xc7, 0xd9, 0x90, 0x6d,
            0xf2, 0x05, 0x0f, 0x37, 0x3e, 0x5a, 0x5d, 0x1b,
            0x5a, 0x1a, 0xc9, 0x7d, 0x5b, 0x43, 0x7c, 0x45,
            0x5c, 0x41, 0x9e, 0x05, 0x3b, 0xa8, 0x9b, 0x3f,
            0x4f, 0x2e, 0x0e, 0x5b, 0x79, 0x6a, 0x8a, 0x43,
            0x15, 0x59, 0x0d, 0xbe, 0x4a, 0x81, 0x35, 0xe4,
            0x85, 0x4c, 0x25, 0xa9, 0x6f, 0x48, 0xf2, 0x54,
            0x56, 0x53, 0x3b, 0x17, 0x45, 0xb0, 0x9f, 0xa7,
            0x92, 0xa4, 0x3b, 0xf2, 0x1e, 0x90, 0x28, 0x02,
            0x51, 0x1e, 0x96, 0x9c, 0x60, 0x2c, 0x4c, 0xf6,
            0xa5, 0x5d, 0x60, 0xd1, 0x71, 0x4b, 0x6f, 0x13,
            0x2d, 0xbb, 0x5d, 0x0c, 0x45, 0x04, 0x89, 0x91,
            0xf7, 0x37, 0xb9, 0x95, 0x92, 0xd7, 0x30, 0x66,
            0x2c, 0x3e, 0xa5, 0x5e, 0x2b, 0xa3, 0x65, 0x1a,
            0x20, 0x9c, 0x23, 0x89, 0x91, 0x9e, 0x5e, 0x24,
            0xda, 0x8b, 0x86, 0x1e, 0x7d, 0x25, 0x97, 0x65,
            0xf7, 0x2f, 0x85, 0x72, 0x8a, 0x07, 0x5d, 0x7f,
            0x9f, 0x3e, 0xb8, 0x2f, 0x53, 0x1d, 0x1f, 0x6f
        }
    };
    
    /* 验证签名长度 */
    if (sig_len != public_key.key_size) {
        secure_memzero(digest, sizeof(digest));
        return LIB_ERR_SIGNATURE_INVALID;
    }
    
    /* 使用 RSA-PSS 验证签名 */
    int verify_result = rsa_verify_pss(digest, SHA384_DIGEST_SIZE,
                                        signature, sig_len,
                                        &public_key,
                                        32); /* 盐值长度 */
    
    secure_memzero(digest, sizeof(digest));
    
    if (verify_result == CRYPTO_SUCCESS) {
        return LIB_SUCCESS;
    }
    
    return LIB_ERR_SIGNATURE_INVALID;
}

/**
 * 检查库依赖是否满足
 */
static int check_dependencies(const hiclib_header_t *header,
                               const uint8_t *lib_data)
{
    if (header->dep_count == 0) {
        return LIB_SUCCESS;
    }
    
    const hiclib_dependency_t *deps = (const hiclib_dependency_t *)
        (lib_data + header->dep_offset);
    
    for (uint32_t i = 0; i < header->dep_count; i++) {
        const hiclib_dependency_t *dep = &deps[i];
        
        /* 查找依赖库 */
        uint32_t min_ver = hiclib_pack_version(dep->min_major, dep->min_minor, 0);
        uint32_t max_ver = hiclib_pack_version(dep->max_major, dep->max_minor, 0);
        
        lib_instance_t *dep_inst = find_best_version(dep->uuid, min_ver, max_ver);
        
        if (!dep_inst) {
            /* 依赖库未找到 */
            return LIB_ERR_DEPENDENCY;
        }
    }
    
    return LIB_SUCCESS;
}

/**
 * 分配库内存并加载数据
 */
static int allocate_and_load_library(lib_instance_t *inst,
                                      const hiclib_header_t *header,
                                      const uint8_t *lib_data,
                                      uint32_t lib_size)
{
    uint32_t domain_id = 0;  /* 库管理器运行在域0 */
    
    /* 解析段表并分配内存 */
    const hiclib_segment_t *segments = (const hiclib_segment_t *)
        (lib_data + header->segment_offset);
    
    for (uint32_t i = 0; i < header->segment_count; i++) {
        const hiclib_segment_t *seg = &segments[i];
        
        /* 验证段在文件范围内 */
        if (seg->file_offset + seg->file_size > lib_size) {
            return LIB_ERR_INVALID_PARAM;
        }
        
        uint64_t aligned_size = PAGE_ALIGN(seg->mem_size);
        uint64_t phys_addr = 0;
        
        if (seg->type == HICLIB_SEG_CODE) {
            /* 分配代码段内存 */
            if (module_memory_alloc(domain_id, aligned_size, &phys_addr) != 0) {
                return LIB_ERR_NO_MEMORY;
            }
            
            inst->code_base = phys_addr;
            inst->code_size = aligned_size;
            
            /* 复制代码段数据 */
            if (seg->file_size > 0) {
                module_memcpy((void *)phys_addr, lib_data + seg->file_offset, seg->file_size);
            }
            
            /* 创建代码段能力（只读执行） */
            uint32_t rights = CAP_RIGHT_READ | CAP_RIGHT_EXEC;
            inst->code_cap_handle = module_cap_create_memory(domain_id, phys_addr, aligned_size, rights);
            
        } else if (seg->type == HICLIB_SEG_RODATA) {
            /* 分配只读数据段内存 */
            if (module_memory_alloc(domain_id, aligned_size, &phys_addr) != 0) {
                return LIB_ERR_NO_MEMORY;
            }
            
            inst->rodata_base = phys_addr;
            inst->rodata_size = aligned_size;
            
            /* 复制只读数据 */
            if (seg->file_size > 0) {
                module_memcpy((void *)phys_addr, lib_data + seg->file_offset, seg->file_size);
            }
            
            /* 创建只读数据能力 */
            uint32_t rights = CAP_RIGHT_READ;
            inst->rodata_cap_handle = module_cap_create_memory(domain_id, phys_addr, aligned_size, rights);
            
        } else if (seg->type == HICLIB_SEG_DATA) {
            /* 读写数据段：不共享，每个服务有私有副本 */
            /* 此处只记录大小，实际分配在服务引用时进行 */
            inst->data_size = aligned_size;
        }
    }
    
    return LIB_SUCCESS;
}

/**
 * 释放库内存
 */
static void free_library_memory(lib_instance_t *inst)
{
    uint32_t domain_id = 0;
    
    if (inst->code_base != 0) {
        module_memory_free(domain_id, inst->code_base, inst->code_size);
        inst->code_base = 0;
        inst->code_size = 0;
        inst->code_cap_handle = 0;
    }
    
    if (inst->rodata_base != 0) {
        module_memory_free(domain_id, inst->rodata_base, inst->rodata_size);
        inst->rodata_base = 0;
        inst->rodata_size = 0;
        inst->rodata_cap_handle = 0;
    }
}

/* ========== 公开接口实现 ========== */

/**
 * 初始化库管理器
 */
void lib_manager_init(void)
{
    if (g_lib_manager.initialized) {
        return;
    }
    
    /* 清零管理器 */
    mem_zero(&g_lib_manager, sizeof(lib_manager_t));
    
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        g_lib_manager.instances[i].state = HICLIB_STATE_UNLOADED;
    }
    
    g_lib_manager.initialized = 1;
}

/**
 * 注册共享库
 */
int lib_register(const lib_register_request_t *req, lib_register_response_t *resp)
{
    if (!req || !resp || !req->lib_data || req->lib_size < sizeof(hiclib_header_t)) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    const hiclib_header_t *header = (const hiclib_header_t *)req->lib_data;
    
    /* 验证文件头 */
    if (!hiclib_header_valid(header)) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    /* 检查是否已存在 */
    uint32_t version = hiclib_pack_version(header->major, header->minor, header->patch);
    if (find_instance(header->uuid, version) != NULL) {
        return LIB_ERR_ALREADY_EXISTS;
    }
    
    /* 查找空闲槽位 */
    lib_instance_t *inst = find_free_slot();
    if (!inst) {
        return LIB_ERR_NO_MEMORY;
    }
    
    /* 初始化实例 */
    mem_cpy(inst->uuid, header->uuid, 16);
    
    /* 复制名称 */
    int name_len = str_len(header->name);
    if (name_len >= LIB_MAX_NAME_LEN) name_len = LIB_MAX_NAME_LEN - 1;
    mem_cpy((uint8_t *)inst->name, (const uint8_t *)header->name, name_len);
    inst->name[name_len] = '\0';
    
    inst->major = header->major;
    inst->minor = header->minor;
    inst->patch = header->patch;
    inst->state = HICLIB_STATE_LOADING;
    inst->ref_count = 0;
    
    /* 验证签名（如果需要） */
    if (req->flags & LIB_REG_FLAG_VERIFY_SIG) {
        int sig_result = verify_library_signature(header, req->lib_data, req->lib_size);
        if (sig_result != LIB_SUCCESS) {
            inst->state = HICLIB_STATE_UNLOADED;
            return sig_result;
        }
    }
    
    /* 检查依赖 */
    int dep_result = check_dependencies(header, req->lib_data);
    if (dep_result != LIB_SUCCESS) {
        inst->state = HICLIB_STATE_UNLOADED;
        return dep_result;
    }
    
    /* 解析符号表 */
    if (header->symbol_count > 0) {
        inst->symbols = (const hiclib_symbol_t *)
            (req->lib_data + header->symbol_offset);
        inst->symbol_count = header->symbol_count;
    }
    
    /* 解析依赖列表 */
    if (header->dep_count > 0 && header->dep_count <= 8) {
        const hiclib_dependency_t *deps = (const hiclib_dependency_t *)
            (req->lib_data + header->dep_offset);
        for (uint32_t i = 0; i < header->dep_count; i++) {
            mem_cpy(inst->dependencies[i], deps[i].uuid, 16);
        }
        inst->dep_count = header->dep_count;
    }
    
    /* 分配内存并加载段数据 */
    int load_result = allocate_and_load_library(inst, header, req->lib_data, req->lib_size);
    if (load_result != LIB_SUCCESS) {
        inst->state = HICLIB_STATE_UNLOADED;
        return load_result;
    }
    
    /* 缓存二进制数据（如果需要） */
    if (req->flags & LIB_REG_FLAG_CACHE) {
        inst->cached_data = req->lib_data;
        inst->cached_size = req->lib_size;
    }
    
    /* 标记为弃用（如果需要） */
    if (req->flags & LIB_REG_FLAG_DEPRECATED) {
        inst->state = HICLIB_STATE_DEPRECATED;
    } else {
        inst->state = HICLIB_STATE_LOADED;
    }
    
    inst->load_time = module_get_timestamp();
    
    /* 更新统计 */
    g_lib_manager.instance_count++;
    g_lib_manager.stats.total_libraries++;
    g_lib_manager.stats.loaded_libraries++;
    g_lib_manager.stats.total_code_size += inst->code_size;
    g_lib_manager.stats.total_rodata_size += inst->rodata_size;
    
    /* 构造响应 */
    resp->status = LIB_SUCCESS;
    mem_cpy(resp->lib_uuid, header->uuid, 16);
    resp->version = version;
    resp->code_base = inst->code_base;
    resp->code_size = inst->code_size;
    resp->rodata_base = inst->rodata_base;
    resp->rodata_size = inst->rodata_size;
    
    return LIB_SUCCESS;
}

/**
 * 查询库
 */
int lib_lookup(const lib_lookup_request_t *req, lib_lookup_response_t *resp)
{
    if (!req || !resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    lib_instance_t *inst = NULL;
    
    /* 按UUID查找 */
    int has_uuid = 0;
    for (int i = 0; i < 16; i++) {
        if (req->uuid[i] != 0) {
            has_uuid = 1;
            break;
        }
    }
    
    if (has_uuid) {
        inst = find_best_version(req->uuid, req->min_version, req->max_version);
    } else if (req->name[0] != '\0') {
        /* 按名称查找 */
        for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
            lib_instance_t *candidate = &g_lib_manager.instances[i];
            
            if (candidate->state == HICLIB_STATE_LOADED ||
                candidate->state == HICLIB_STATE_ACTIVE) {
                
                if (str_cmp(candidate->name, req->name) == 0) {
                    inst = candidate;
                    break;
                }
            }
        }
    }
    
    if (!inst) {
        g_lib_manager.stats.cache_misses++;
        return LIB_ERR_NOT_FOUND;
    }
    
    g_lib_manager.stats.cache_hits++;
    
    /* 构造响应 */
    resp->status = LIB_SUCCESS;
    mem_cpy(resp->lib_uuid, inst->uuid, 16);
    resp->version = hiclib_pack_version(inst->major, inst->minor, inst->patch);
    resp->code_cap = inst->code_cap_handle;
    resp->rodata_cap = inst->rodata_cap_handle;
    resp->symbol_count = inst->symbol_count;
    resp->ref_count = inst->ref_count;
    
    return LIB_SUCCESS;
}

/**
 * 增加引用
 */
int lib_reference(const lib_ref_request_t *req, lib_ref_response_t *resp)
{
    if (!req || !resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    lib_instance_t *inst = find_instance(req->uuid, req->version);
    if (!inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    if (inst->state == HICLIB_STATE_DEPRECATED) {
        return LIB_ERR_PERMISSION;  /* 弃用的库不允许新引用 */
    }
    
    inst->ref_count++;
    inst->state = HICLIB_STATE_ACTIVE;
    
    /* 记录引用域 */
    for (uint32_t i = 0; i < 32; i++) {
        if (inst->ref_domains[i] == 0) {
            inst->ref_domains[i] = req->requester_domain;
            break;
        }
    }
    
    g_lib_manager.stats.active_references++;
    
    resp->status = LIB_SUCCESS;
    resp->ref_count = inst->ref_count;
    
    return LIB_SUCCESS;
}

/**
 * 减少引用
 */
int lib_release(const lib_ref_request_t *req, lib_ref_response_t *resp)
{
    if (!req || !resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    lib_instance_t *inst = find_instance(req->uuid, req->version);
    if (!inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    /* 移除引用域 */
    for (uint32_t i = 0; i < 32; i++) {
        if (inst->ref_domains[i] == req->requester_domain) {
            inst->ref_domains[i] = 0;
            break;
        }
    }
    
    if (inst->ref_count > 0) {
        inst->ref_count--;
        g_lib_manager.stats.active_references--;
    }
    
    if (inst->ref_count == 0) {
        if (inst->state == HICLIB_STATE_DEPRECATED) {
            /* 弃用且无引用，自动卸载 */
            free_library_memory(inst);
            inst->state = HICLIB_STATE_UNLOADED;
            g_lib_manager.stats.loaded_libraries--;
        } else {
            inst->state = HICLIB_STATE_LOADED;
        }
    }
    
    resp->status = LIB_SUCCESS;
    resp->ref_count = inst->ref_count;
    
    return LIB_SUCCESS;
}

/**
 * 查询符号
 */
int lib_query_symbol(const lib_symbol_request_t *req, lib_symbol_response_t *resp)
{
    if (!req || !resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    lib_instance_t *inst = find_instance(req->uuid, req->version);
    if (!inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    if (!inst->symbols || inst->symbol_count == 0) {
        return LIB_ERR_NOT_FOUND;
    }
    
    /* 遍历符号表 */
    for (uint32_t i = 0; i < inst->symbol_count; i++) {
        const hiclib_symbol_t *sym = &inst->symbols[i];
        
        if (str_cmp(sym->name, req->symbol_name) == 0) {
            resp->status = LIB_SUCCESS;
            resp->offset = sym->offset;
            resp->size = sym->size;
            resp->type = sym->type;
            resp->segment_index = sym->segment_index;
            return LIB_SUCCESS;
        }
    }
    
    return LIB_ERR_NOT_FOUND;
}

/**
 * 列出库
 */
int lib_list(lib_list_response_t *resp)
{
    if (!resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    resp->status = LIB_SUCCESS;
    resp->count = 0;
    
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES && resp->count < 16; i++) {
        lib_instance_t *inst = &g_lib_manager.instances[i];
        
        if (inst->state != HICLIB_STATE_UNLOADED) {
            lib_list_item_t *item = &resp->libraries[resp->count];
            
            mem_cpy(item->uuid, inst->uuid, 16);
            mem_cpy((uint8_t *)item->name, (const uint8_t *)inst->name, 32);
            item->version = hiclib_pack_version(inst->major, inst->minor, inst->patch);
            item->state = inst->state;
            item->ref_count = inst->ref_count;
            item->code_size = inst->code_size;
            
            resp->count++;
        }
    }
    
    return LIB_SUCCESS;
}

/**
 * 获取统计
 */
int lib_get_stats(lib_stats_t *stats)
{
    if (!stats) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    *stats = g_lib_manager.stats;
    return LIB_SUCCESS;
}

/**
 * 卸载库
 */
int lib_unload(const uint8_t uuid[16], uint32_t version)
{
    lib_instance_t *inst = find_instance(uuid, version);
    if (!inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    if (inst->ref_count > 0) {
        return LIB_ERR_IN_USE;
    }
    
    /* 释放内存 */
    free_library_memory(inst);
    
    /* 更新统计 */
    g_lib_manager.instance_count--;
    g_lib_manager.stats.loaded_libraries--;
    g_lib_manager.stats.total_code_size -= inst->code_size;
    g_lib_manager.stats.total_rodata_size -= inst->rodata_size;
    
    inst->state = HICLIB_STATE_UNLOADED;
    
    return LIB_SUCCESS;
}

/**
 * 滚动更新库
 * 将旧库的引用迁移到新库
 */
int lib_update(const lib_update_request_t *req, lib_update_response_t *resp)
{
    if (!req || !resp) {
        return LIB_ERR_INVALID_PARAM;
    }
    
    /* 查找旧库 */
    lib_instance_t *old_inst = find_instance(req->old_uuid, req->old_version);
    if (!old_inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    /* 查找新库 */
    lib_instance_t *new_inst = find_instance(req->new_uuid, req->new_version);
    if (!new_inst) {
        return LIB_ERR_NOT_FOUND;
    }
    
    /* 验证请求者有权更新 */
    int found = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (old_inst->ref_domains[i] == req->requester_domain) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return LIB_ERR_PERMISSION;
    }
    
    /* 标记旧库为弃用 */
    old_inst->state = HICLIB_STATE_DEPRECATED;
    
    /* 迁移引用域 */
    uint32_t migrated = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (old_inst->ref_domains[i] == req->requester_domain) {
            /* 减少旧库引用 */
            old_inst->ref_count--;
            old_inst->ref_domains[i] = 0;
            
            /* 增加新库引用 */
            new_inst->ref_count++;
            new_inst->state = HICLIB_STATE_ACTIVE;
            
            /* 添加到新库引用域 */
            for (uint32_t j = 0; j < 32; j++) {
                if (new_inst->ref_domains[j] == 0) {
                    new_inst->ref_domains[j] = req->requester_domain;
                    break;
                }
            }
            
            migrated++;
            break;  /* 只迁移请求者的引用 */
        }
    }
    
    /* 如果旧库无引用，自动卸载 */
    if (old_inst->ref_count == 0) {
        free_library_memory(old_inst);
        old_inst->state = HICLIB_STATE_UNLOADED;
        g_lib_manager.stats.loaded_libraries--;
    }
    
    /* 构造响应 */
    resp->status = LIB_SUCCESS;
    resp->new_code_cap = new_inst->code_cap_handle;
    resp->new_rodata_cap = new_inst->rodata_cap_handle;
    resp->migrated_refs = migrated;
    
    return LIB_SUCCESS;
}

/* ========== 服务接口实现 ========== */

hic_status_t service_init(void)
{
    lib_manager_init();
    return HIC_SUCCESS;
}

hic_status_t service_start(void)
{
    return HIC_SUCCESS;
}

hic_status_t service_stop(void)
{
    /* 停止所有库 */
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        lib_instance_t *inst = &g_lib_manager.instances[i];
        if (inst->state != HICLIB_STATE_UNLOADED && inst->ref_count == 0) {
            free_library_memory(inst);
            inst->state = HICLIB_STATE_UNLOADED;
        }
    }
    return HIC_SUCCESS;
}

hic_status_t service_cleanup(void)
{
    /* 清理所有库 */
    for (uint32_t i = 0; i < LIB_MAX_INSTANCES; i++) {
        lib_instance_t *inst = &g_lib_manager.instances[i];
        if (inst->state != HICLIB_STATE_UNLOADED) {
            free_library_memory(inst);
            inst->state = HICLIB_STATE_UNLOADED;
        }
    }
    return HIC_SUCCESS;
}

hic_status_t service_get_info(char* buffer, u32 size)
{
    if (buffer && size > 0) {
        const char *info = "Shared Library Manager v1.0.0\n"
                          "Endpoints: register(0x6800), lookup(0x6801), reference(0x6802), release(0x6803), update(0x6804), symbol(0x6805), list(0x6806), stats(0x6807), unload(0x6808)";
        int len = str_len(info);
        if (len >= (int)size) len = size - 1;
        mem_cpy((uint8_t *)buffer, (const uint8_t *)info, len);
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
    service_register("lib_manager_service", &g_service_api);
}
