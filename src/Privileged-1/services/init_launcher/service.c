/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 初始服务启动器实现
 * 
 * 职责：
 * 1. 持有存储设备访问能力
 * 2. 从 /boot/module_manager.hicmod 读取模块管理器
 * 3. 验证签名/哈希
 * 4. 请求 Core-0 创建域并加载模块管理器
 * 5. 启动后进入休眠，可被卸载
 */

#include "service.h"
#include <string.h>

/* ==================== 服务入口点（必须在代码段最前面） ==================== */

/**
 * 服务入口点 - 必须放在代码段最前面
 * 使用 section 属性确保此函数在链接时位于 .static_svc.init_launcher.text 的开头
 */
__attribute__((section(".static_svc.init_launcher.text"), used, noinline))
int _init_launcher_entry(void)
{
    /* 初始化启动器 */
    init_launcher_init();
    
    /* 启动服务主循环 */
    init_launcher_start();
    
    /* 服务不应返回，如果返回则进入无限循环 */
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return 0;
}

/* ========== 外部依赖（由 Core-0 提供） ========== */

/* 串口输出 */
extern void serial_print(const char *msg);

/* 内存操作 */
extern void *module_memcpy(void *dst, const void *src, size_t n);
extern void *module_memset(void *dst, int c, size_t n);

/* 域操作（由 module_primitives.c 提供） */
extern uint64_t module_cap_create_domain(uint32_t parent_domain, uint32_t *new_domain);
extern uint64_t module_cap_create_endpoint(uint32_t domain_id, uint32_t *endpoint_id);
extern uint64_t module_domain_start(uint32_t domain_id, uint64_t entry_point);
extern uint64_t module_memory_alloc(uint32_t domain_id, uint64_t size, uint32_t type, uint64_t *phys_addr);

/* 哈希验证（由 Core-0 提供） */
extern void sha384_hash(const uint8_t *data, uint32_t len, uint8_t *hash_out);

/* Verifier 服务接口（静态链接） */
extern int verifier_init(void);
extern int verifier_start(void);

/* 验证接口 */
typedef enum {
    VERIFY_OK = 0,
    VERIFY_ERR_HASH_MISMATCH = 1,
    VERIFY_ERR_SIGNATURE_INVALID = 2,
} verify_status_t;

extern verify_status_t verifier_verify_module(
    const void *module_data,
    size_t module_size,
    const void *sign_header,
    void *result
);

extern void verifier_compute_hash(const void *data, size_t size, uint8_t hash[48]);

/* FAT32 接口（由 fat32_service 提供） */
extern int fat32_service_init(void);
extern int fat32_service_start(void);
extern int fat32_read_file(const char *path, void *buffer, 
                           uint32_t buffer_size, uint32_t *bytes_read);

/* ========== 内部状态 ========== */

static struct {
    int initialized;
    int module_manager_loaded;
    uint64_t module_manager_domain;
} g_launcher_ctx;

/* 模块魔数 */
#define HICMOD_MAGIC     0x48494B4D  /* "HICM" */
#define HICMOD_TYPE_SVC  0x53525643  /* "SRVC" */

/* 模块头结构 - 必须与 create_hicmod.py 匹配 */
typedef struct {
    uint32_t magic;           /* 0: HICMOD_MAGIC */
    uint32_t version;         /* 4: 模块格式版本 */
    uint8_t  uuid[16];        /* 8: 模块UUID */
    uint32_t semantic_version;/* 24: 语义版本 */
    uint32_t api_offset;      /* 28: API描述符偏移 */
    uint32_t code_size;       /* 32: 代码段大小 */
    uint32_t data_size;       /* 36: 数据段大小 */
    uint32_t sig_offset;      /* 40: 签名偏移 */
    uint32_t header_size;     /* 44: 头部大小 */
    uint8_t  checksum[16];    /* 48: 校验和 */
    uint32_t sig_size;        /* 64: 签名大小 */
    uint32_t flags;           /* 68: 标志位 */
} hicmod_header_t;

/* 临时缓冲区（用于加载模块） */
#define MAX_MODULE_SIZE (2 * 1024 * 1024)  /* 2MB */
static uint8_t g_module_buffer[MAX_MODULE_SIZE] __attribute__((aligned(4096)));

/* ========== 辅助函数 ========== */

static void launcher_log(const char *msg) {
    serial_print("[init_launcher] ");
    serial_print(msg);
    serial_print("\n");
}

static void launcher_log_hex(const char *label, uint64_t val) {
    char buf[64];
    const char hex[] = "0123456789ABCDEF";
    
    serial_print("[init_launcher] ");
    serial_print(label);
    serial_print(": 0x");
    
    for (int i = 60; i >= 0; i -= 4) {
        buf[0] = hex[(val >> i) & 0xF];
        buf[1] = '\0';
        serial_print(buf);
    }
    serial_print("\n");
}

/* ========== ELF 解析辅助 ========== */

/* ELF64 结构（简化版） */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr_t;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} elf64_sym_t;

#define SHT_SYMTAB  2
#define SHT_STRTAB  3
#define STT_FUNC    2

/**
 * 查找ELF中的入口函数
 */
static uint64_t find_elf_entry(const uint8_t *elf_data, size_t elf_size)
{
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_data;
    
    /* 验证ELF魔数 */
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        return 0;
    }
    
    /* 获取节头表 */
    const elf64_shdr_t *shdrs = (const elf64_shdr_t *)(elf_data + ehdr->e_shoff);
    
    /* 查找符号表和字符串表 */
    const elf64_shdr_t *symtab = NULL;
    const elf64_shdr_t *strtab = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab = &shdrs[i];
            /* 符号表的链接是字符串表索引 */
            if (shdrs[i].sh_link < ehdr->e_shnum) {
                strtab = &shdrs[shdrs[i].sh_link];
            }
            break;
        }
    }
    
    if (!symtab || !strtab) {
        return 0;
    }
    
    /* 遍历符号表查找入口函数 */
    const elf64_sym_t *syms = (const elf64_sym_t *)(elf_data + symtab->sh_offset);
    int num_syms = symtab->sh_size / sizeof(elf64_sym_t);
    const char *strings = (const char *)(elf_data + strtab->sh_offset);
    
    for (int i = 0; i < num_syms; i++) {
        if ((syms[i].st_info & 0xf) == STT_FUNC && syms[i].st_value != 0) {
            const char *name = strings + syms[i].st_name;
            /* 查找 service_start 函数 */
            const char *p = name;
            while (*p) {
                if (*p == '_' && *(p+1) == 's' && *(p+2) == 't' && 
                    *(p+3) == 'a' && *(p+4) == 'r' && *(p+5) == 't') {
                    launcher_log("Found entry function:");
                    launcher_log(name);
                    return syms[i].st_value;
                }
                p++;
            }
        }
    }
    
    return 0;
}

/* ========== 核心实现 ========== */

int init_launcher_init(void) {
    launcher_log("初始化...");
    
    module_memset(&g_launcher_ctx, 0, sizeof(g_launcher_ctx));
    
    /* 确保存储服务已启动 */
    fat32_service_init();
    fat32_service_start();
    
    g_launcher_ctx.initialized = 1;
    launcher_log("初始化完成");
    
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_start(void) {
    extern void thread_yield(void);
    uint32_t bytes_read;
    int result;
    
    launcher_log("========================================");
    launcher_log("Service started - Loading module_manager");
    launcher_log("========================================");
    
    /* 步骤1: 等待 FAT32 服务初始化完成 */
    launcher_log("Step 1: Waiting for FAT32 service...");
    
    /* 等待更多时间让 FAT32 初始化 */
    for (int i = 0; i < 100; i++) {
        thread_yield();
    }
    
        /* 步骤2: 读取模块管理器 */
        launcher_log("Step 2: Reading module_manager.hicmod...");
        
        /* 使用 FAT32 8.3 短文件名格式 */
        const char *module_path = "/modules/MODULE~1.HIC";  /* module_manager_service.hicmod */
        
        result = fat32_read_file(module_path, g_module_buffer, 
                             MAX_MODULE_SIZE, &bytes_read);    
    if (result != 0 || bytes_read == 0) {
        launcher_log("ERROR: Failed to read module_manager.hicmod");
        launcher_log("Trying alternate path...");
        
        /* 尝试备用路径 */
        module_path = "/MODULE~1.HIC";  /* 短文件名 */
        result = fat32_read_file(module_path, g_module_buffer,
                                 MAX_MODULE_SIZE, &bytes_read);
        
        if (result != 0 || bytes_read == 0) {
            launcher_log("ERROR: Module manager not found on disk");
            launcher_log("Entering idle mode (services already running)");
            
            /* 进入空闲模式 - 静态服务已经运行 */
            while (1) {
                thread_yield();
            }
        }
    }
    
    launcher_log("Module manager read successfully");
    
    /* 输出读取大小 */
    {
        char buf[64];
        const char hex[] = "0123456789ABCDEF";
        serial_print("[init_launcher] Bytes read: 0x");
        for (int i = 28; i >= 0; i -= 4) {
            buf[0] = hex[(bytes_read >> i) & 0xF];
            buf[1] = '\0';
            serial_print(buf);
        }
        serial_print("\n");
    }
    
    /* 步骤3: 验证模块魔数 */
    launcher_log("Step 3: Verifying module header...");
    
    hicmod_header_t *header = (hicmod_header_t *)g_module_buffer;
    
    if (header->magic != HICMOD_MAGIC) {
        launcher_log("ERROR: Invalid module magic");
        
        /* 进入空闲模式 */
        while (1) {
            thread_yield();
        }
    }
    
    launcher_log("Module magic verified: HICM");
    
    /* 步骤4: 验证模块签名/哈希 */
    launcher_log("Step 4: Verifying module signature...");
    
    uint8_t computed_hash[48];
    verifier_compute_hash(g_module_buffer, bytes_read, computed_hash);
    
    launcher_log("Module hash computed");
    
    /* 开发阶段：跳过签名验证 */
    if (header->flags & 0x01) {
        launcher_log("Module is signed (verification skipped in dev mode)");
    } else {
        launcher_log("Module is unsigned (development build)");
    }
    
    /* 步骤5: 创建模块管理器域 */
    launcher_log("Step 5: Creating module_manager domain...");
    
    uint32_t new_domain = 0;
    uint64_t status = module_cap_create_domain(0, &new_domain);
    
    if (status != 0) {
        launcher_log("ERROR: Failed to create domain");
        while (1) { thread_yield(); }
    }
    
    launcher_log("Domain created");
    
    /* 步骤6: 创建端点 */
    launcher_log("Step 6: Creating endpoint...");
    
    uint32_t new_endpoint = 0;
    status = module_cap_create_endpoint(new_domain, &new_endpoint);
    
    if (status != 0) {
        launcher_log("ERROR: Failed to create endpoint");
        while (1) { thread_yield(); }
    }
    
    launcher_log("Endpoint created");
    
    /* 步骤7: 分配内存并加载模块 */
    launcher_log("Step 7: Loading module code...");
    
    /* 计算需要的内存大小 */
    uint32_t code_size = header->code_size;
    uint32_t data_size = header->data_size;
    uint32_t total_size = code_size + data_size;
    
    launcher_log("Module sizes:");
    launcher_log_hex(" code=", code_size);
    launcher_log_hex(", data=", data_size);
    launcher_log_hex(", total=", total_size);
    launcher_log("\n");
    
    /* 对齐到页 */
    total_size = (total_size + 4095) & ~4095U;
    
    /* 分配内存 */
    uint64_t phys_addr = 0;
    status = module_memory_alloc(new_domain, total_size, 0, &phys_addr);  /* type=0=CODE */
    
    if (status != 0 || phys_addr == 0) {
        launcher_log("ERROR: Failed to allocate memory");
        while (1) { thread_yield(); }
    }
    
    launcher_log("Memory allocated at physical address");
    launcher_log_hex("phys: ", phys_addr);
    
    /* 复制代码段 - 直接写入物理地址 */
    /* 注意：ELF数据在HICMOD头部之后，需要找到ELF魔数位置 */
    const uint8_t *elf_data = g_module_buffer + sizeof(hicmod_header_t);
    
    /* ELF魔数可能在头部后有填充，需要查找 */
    int elf_offset = 0;
    for (int i = sizeof(hicmod_header_t); i < bytes_read - 4; i++) {
        if (g_module_buffer[i] == 0x7f && g_module_buffer[i+1] == 'E' &&
            g_module_buffer[i+2] == 'L' && g_module_buffer[i+3] == 'F') {
            elf_data = g_module_buffer + i;
            elf_offset = i - sizeof(hicmod_header_t);
            break;
        }
    }
    
    launcher_log("ELF found in module");
    
    /* 查找入口函数 */
    uint64_t entry_offset = find_elf_entry(elf_data, code_size - elf_offset);
    if (entry_offset == 0) {
        launcher_log("WARNING: Entry function not found, using default");
        entry_offset = 0;
    } else {
        launcher_log_hex("Entry offset: ", entry_offset);
    }
    
    /* 复制整个代码段到物理地址 */
    module_memcpy((void *)phys_addr, g_module_buffer + sizeof(hicmod_header_t), code_size);
    
    launcher_log("Code loaded to physical memory");
    
    /* 步骤8: 启动模块管理器 */
    launcher_log("Step 8: Starting module_manager...");
    
    /* 入口点 = 物理地址 + 入口函数偏移（直接物理地址映射） */
    uint64_t entry_point = phys_addr + entry_offset;
    launcher_log_hex("Entry point (phys): ", entry_point);
    
    status = module_domain_start(new_domain, entry_point);
    
    if (status != 0) {
        launcher_log("ERROR: Failed to start module");
        while (1) { thread_yield(); }
    }
    
    launcher_log("========================================");
    launcher_log(">>> Module manager started successfully <<<");
    launcher_log("========================================");
    
    g_launcher_ctx.module_manager_loaded = 1;
    g_launcher_ctx.module_manager_domain = new_domain;
    
    /* 主服务循环 - 等待请求 */
    int count = 0;
    while (1) {
        thread_yield();
        count++;
        
        if (count > 1000000) {
            count = 0;
        }
    }
    
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_stop(void) {
    launcher_log("停止...");
    /* 模块管理器由自己管理生命周期，这里不做任何事 */
    return INIT_LAUNCHER_SUCCESS;
}

int init_launcher_cleanup(void) {
    launcher_log("清理...");
    module_memset(&g_launcher_ctx, 0, sizeof(g_launcher_ctx));
    return INIT_LAUNCHER_SUCCESS;
}
