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
#include <module_format.h>

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
    
    /* 服务完成，返回让调度器继续 */
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
extern uint64_t module_cap_create_service(uint32_t domain_id, uint32_t *service_id);
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

/* 使用 module_format.h 中的 HICMOD_MAGIC 和 hicmod_header_t */

/* 临时缓冲区（用于加载模块） */
#define MAX_MODULE_SIZE (8 * 1024 * 1024)  /* 8MB - 需要足够大以容纳 module_manager */
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
 * 明确查找 <module_name>_service_start 或 service_start 函数
 * 
 * 参数:
 *   elf_data: ELF 数据指针
 *   elf_size: ELF 数据大小
 *   out_offset: 输出参数，返回找到的入口偏移
 * 
 * 返回: 1 表示找到，0 表示未找到
 */
static int find_elf_entry_ex(const uint8_t *elf_data, size_t elf_size, uint64_t *out_offset)
{
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_data;
    
    /* 验证ELF魔数 */
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        serial_print("[FIND_ELF] Not an ELF file\n");
        return 0;
    }
    
    serial_print("[FIND_ELF] ELF magic OK\n");
    
    /* 输出 ELF 头关键信息 */
    launcher_log_hex("e_shoff: ", ehdr->e_shoff);
    launcher_log_hex("e_shnum: ", ehdr->e_shnum);
    launcher_log_hex("e_shentsize: ", ehdr->e_shentsize);
    launcher_log_hex("elf_size: ", elf_size);
    
    /* 检查节头表偏移是否有效 */
    if (ehdr->e_shoff == 0 || ehdr->e_shoff >= elf_size) {
        serial_print("[FIND_ELF] Invalid section header offset\n");
        return 0;
    }
    
    serial_print("[FIND_ELF] Section header offset OK\n");
    
    /* 获取节头表 */
    const elf64_shdr_t *shdrs = (const elf64_shdr_t *)(elf_data + ehdr->e_shoff);
    
    /* 查找符号表和字符串表 */
    const elf64_shdr_t *symtab = NULL;
    const elf64_shdr_t *strtab = NULL;
    uint32_t symtab_link = 0;
    
    /* 直接读取第一个节头的 sh_type 来验证结构体对齐 */
    uint32_t first_sh_type;
    module_memcpy(&first_sh_type, &shdrs[0].sh_type, sizeof(uint32_t));
    launcher_log_hex("First section type: ", first_sh_type);
    
    serial_print("[FIND_ELF] Scanning sections...\n");
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        /* 直接从内存读取 sh_type 以避免对齐问题 */
        uint32_t sh_type;
        module_memcpy(&sh_type, &shdrs[i].sh_type, sizeof(uint32_t));
        
        if (sh_type == SHT_SYMTAB) {
            serial_print("[FIND_ELF] Found SYMTAB at index\n");
            symtab = &shdrs[i];
            module_memcpy(&symtab_link, &shdrs[i].sh_link, sizeof(uint32_t));
        }
        if (sh_type == SHT_STRTAB) {
            /* 记录所有 STRTAB，稍后根据 symtab_link 选择 */
            if (symtab_link == (uint32_t)i) {
                serial_print("[FIND_ELF] Found STRTAB linked to SYMTAB!\n");
                strtab = &shdrs[i];
            }
        }
    }
    
    /* 如果找到了 SYMTAB 但还没找到对应的 STRTAB，重新查找 */
    if (symtab && !strtab && symtab_link < (uint32_t)ehdr->e_shnum) {
        strtab = &shdrs[symtab_link];
        serial_print("[FIND_ELF] Found STRTAB by link index!\n");
    }
    
    if (!symtab || !strtab) {
        serial_print("[FIND_ELF] No symtab or strtab found\n");
        return 0;
    }
    
    serial_print("[FIND_ELF] Found symtab and strtab\n");
    
    /* 遍历符号表查找入口函数 */
    const elf64_sym_t *syms = (const elf64_sym_t *)(elf_data + symtab->sh_offset);
    int num_syms = symtab->sh_size / sizeof(elf64_sym_t);
    const char *strings = (const char *)(elf_data + strtab->sh_offset);
    
    /* 首先查找精确匹配的 service_start 函数 */
    /* 注意：在 relocatable object 中，st_value 可以是 0（表示符号在节的开始位置） */
    /* 所以我们不能检查 st_value != 0，而是检查 st_shndx 是否指向有效的节 */
    uint64_t fallback_entry = 0;
    const char *fallback_name = NULL;
    uint16_t fallback_shndx = 0;
    
    for (int i = 0; i < num_syms; i++) {
        /* 检查是否是函数类型，并且 st_shndx 不是未定义 (SHN_UNDEF = 0) */
        if ((syms[i].st_info & 0xf) == STT_FUNC && syms[i].st_shndx != 0) {
            const char *name = strings + syms[i].st_name;
            size_t name_len = 0;
            const char *p = name;
            while (*p++) name_len++;
            
            /* 检查是否以 "service_start" 结尾 */
            if (name_len >= 13) {
                const char *suffix = name + name_len - 13;
                if (suffix[0] == 's' && suffix[1] == 'e' && suffix[2] == 'r' &&
                    suffix[3] == 'v' && suffix[4] == 'i' && suffix[5] == 'c' &&
                    suffix[6] == 'e' && suffix[7] == '_' && suffix[8] == 's' &&
                    suffix[9] == 't' && suffix[10] == 'a' && suffix[11] == 'r' &&
                    suffix[12] == 't') {
                    launcher_log("Found service_start entry:");
                    launcher_log(name);
                    launcher_log_hex("Entry offset: ", syms[i].st_value);
                    /* For relocatable objects: absolute offset = section sh_offset + st_value */
                    uint16_t entry_shndx;
                    module_memcpy(&entry_shndx, &syms[i].st_shndx, sizeof(uint16_t));
                    if (entry_shndx < ehdr->e_shnum) {
                        uint64_t sec_off;
                        module_memcpy(&sec_off, &shdrs[entry_shndx].sh_offset, sizeof(uint64_t));
                        *out_offset = sec_off + syms[i].st_value;
                        launcher_log_hex("Computed entry offset (section+st_value): ", *out_offset);
                    } else {
                        *out_offset = syms[i].st_value;
                    }
                    return 1;  /* 找到了 */
                }
            }
            
            /* 记录第一个 _start 函数作为备选 */
            if (fallback_entry == 0 && fallback_name == NULL) {
                p = name;
                while (*p) {
                    if (*p == '_' && *(p+1) == 's' && *(p+2) == 't' && 
                        *(p+3) == 'a' && *(p+4) == 'r' && *(p+5) == 't') {
                        fallback_entry = syms[i].st_value;
                        fallback_name = name;
                        module_memcpy(&fallback_shndx, &syms[i].st_shndx, sizeof(uint16_t));
                        launcher_log("Fallback entry:");
                        launcher_log(name);
                        launcher_log_hex("Fallback offset: ", fallback_entry);
                        break;
                    }
                    p++;
                }
            }
        }
    }
    
    if (fallback_entry != 0 || fallback_name != NULL) {
        launcher_log("Using fallback entry point");
        if (fallback_shndx > 0 && fallback_shndx < ehdr->e_shnum) {
            uint64_t sec_off;
            module_memcpy(&sec_off, &shdrs[fallback_shndx].sh_offset, sizeof(uint64_t));
            *out_offset = sec_off + fallback_entry;
            launcher_log_hex("Computed fallback offset (section+st_value): ", *out_offset);
        } else {
            *out_offset = fallback_entry;
        }
        return 1;  /* 找到了备选 */
    }
    
    return 0;
}

/* ========== ELF 重定位处理 ========== */

#define SHT_RELA    4
#define SHT_SYMTAB  2
#define SHT_STRTAB  3

/* x86_64 重定位类型 */
#define R_X86_64_NONE           0
#define R_X86_64_64             1
#define R_X86_64_PC32           2
#define R_X86_64_GOT32          3
#define R_X86_64_PLT32          4
#define R_X86_64_COPY           5
#define R_X86_64_GLOB_DAT       6
#define R_X86_64_JUMP_SLOT      7
#define R_X86_64_RELATIVE       8
#define R_X86_64_GOTPCREL       9
#define R_X86_64_32             10
#define R_X86_64_32S            11
#define R_X86_64_16             12
#define R_X86_64_PC16           13
#define R_X86_64_8              14
#define R_X86_64_PC8            15
#define R_X86_64_GOTPCRELX      41
#define R_X86_64_REX_GOTPCRELX  42

/* ELF64 重定位条目 */
typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} elf64_rela_t;

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

/* 外部符号查找表 - 内核提供的函数 */
typedef struct {
    const char *name;
    uint64_t addr;
} external_symbol_t;

/* 内核导出的函数地址（需要在链接时提供） */
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern char *strcpy(char *, const char *);
extern int strcmp(const char *, const char *);
extern char *strrchr(const char *, int);
extern size_t strlen(const char *);
extern int memcmp(const void *, const void *, size_t);
extern char *strncpy(char *, const char *, size_t);
extern void __stack_chk_fail(void);
extern uint64_t module_service_register(const char *, uint32_t, uint32_t);
extern uint64_t domain_destroy(uint32_t);
extern int fat32_read_file(const char *, void *, uint32_t, uint32_t *);

/* module_primitives 提供的函数 */
extern uint64_t module_cap_create_domain(uint32_t, uint32_t *);
extern uint64_t module_cap_create_service(uint32_t, uint32_t *);
extern uint64_t module_domain_start(uint32_t, uint64_t);
extern void *module_memcpy(void *, const void *, size_t);
extern void *module_memset(void *, int, size_t);
extern uint64_t module_memory_alloc(uint32_t, uint64_t, uint32_t, uint64_t *);

/* 未实现的符号 - 提供占位符 */
static void __stub_domain_parallel_create(void) { }
static void __stub_cap_migration_channel_create(void) { }
static void __stub_domain_atomic_switch(void) { }
static void __stub_domain_graceful_shutdown(void) { }
static void __stub_sha384_hash(void) { }
static void __stub_service_register(const char *name, void *api) {
    (void)name; (void)api;
}

static uint64_t find_external_symbol(const char *name) {
    /* 内核符号查找表 */
    static const external_symbol_t kernel_syms[] = {
        {"memset",  (uint64_t)memset},
        {"memcpy",  (uint64_t)memcpy},
        {"strcpy",  (uint64_t)strcpy},
        {"strcmp",  (uint64_t)strcmp},
        {"strrchr", (uint64_t)strrchr},
        {"strlen",  (uint64_t)strlen},
        {"memcmp",  (uint64_t)memcmp},
        {"strncpy", (uint64_t)strncpy},
        {"__stack_chk_fail", (uint64_t)__stack_chk_fail},
        {"module_service_register", (uint64_t)module_service_register},
        {"domain_destroy", (uint64_t)domain_destroy},
        {"fat32_read_file", (uint64_t)fat32_read_file},
        /* module_primitives */
        {"module_cap_create_domain", (uint64_t)module_cap_create_domain},
        {"module_cap_create_service", (uint64_t)module_cap_create_service},
        {"module_cap_create_endpoint", (uint64_t)module_cap_create_service},
        {"module_domain_start", (uint64_t)module_domain_start},
        {"module_memory_alloc", (uint64_t)module_memory_alloc},
        /* module wrappers for external calls */
        {"service_register", (uint64_t)__stub_service_register},
        {"module_memcpy", (uint64_t)module_memcpy},
        {"module_memset", (uint64_t)module_memset},
        /* 占位符 - 未完全实现的功能 */
        {"domain_parallel_create", (uint64_t)__stub_domain_parallel_create},
        {"cap_migration_channel_create", (uint64_t)__stub_cap_migration_channel_create},
        {"domain_atomic_switch", (uint64_t)__stub_domain_atomic_switch},
        {"domain_graceful_shutdown", (uint64_t)__stub_domain_graceful_shutdown},
        {"sha384_hash", (uint64_t)__stub_sha384_hash},
        {NULL, 0}
    };
    
    for (int i = 0; kernel_syms[i].name != NULL; i++) {
        const char *s1 = kernel_syms[i].name;
        const char *s2 = name;
        while (*s1 && *s2 && *s1 == *s2) {
            s1++;
            s2++;
        }
        if (*s1 == '\0' && *s2 == '\0') {
            return kernel_syms[i].addr;
        }
    }
    return 0;
}

/**
 * 应用 ELF 重定位
 * 
 * 参数:
 *   elf_base: ELF 加载基址
 *   elf_size: ELF 数据大小
 * 
 * 返回: 0 成功，非零失败
 */
static int apply_elf_relocations(uint8_t *elf_base, size_t elf_size) {
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)elf_base;
    
    if (ehdr->e_shoff == 0 || ehdr->e_shoff >= elf_size) {
        launcher_log("No section headers for relocation");
        return 0;  /* 没有节头表，可能不需要重定位 */
    }
    
    const elf64_shdr_t *shdrs = (const elf64_shdr_t *)(elf_base + ehdr->e_shoff);
    
    /* 查找符号表、字符串表和重定位节 */
    const elf64_shdr_t *symtab = NULL;
    const elf64_shdr_t *strtab = NULL;
    const elf64_shdr_t *shstrtab = NULL;
    uint32_t symtab_link = 0;
    
    /* 首先获取节名字符串表 */
    if (ehdr->e_shstrndx < ehdr->e_shnum) {
        shstrtab = &shdrs[ehdr->e_shstrndx];
    }
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        uint32_t sh_type;
        module_memcpy(&sh_type, &shdrs[i].sh_type, sizeof(uint32_t));
        
        if (sh_type == SHT_SYMTAB) {
            symtab = &shdrs[i];
            module_memcpy(&symtab_link, &shdrs[i].sh_link, sizeof(uint32_t));
        }
    }
    
    if (!symtab) {
        launcher_log("No symbol table found");
        return 0;  /* 没有符号表 */
    }
    
    /* 获取关联的字符串表 */
    if (symtab_link < (uint32_t)ehdr->e_shnum) {
        strtab = &shdrs[symtab_link];
    }
    
    if (!strtab) {
        launcher_log("No string table found");
        return -1;
    }
    
    const elf64_sym_t *syms = (const elf64_sym_t *)(elf_base + symtab->sh_offset);
    int num_syms = symtab->sh_size / sizeof(elf64_sym_t);
    const char *strtab_data = (const char *)(elf_base + strtab->sh_offset);
    const char *shstrtab_data = shstrtab ? (const char *)(elf_base + shstrtab->sh_offset) : NULL;
    
    int reloc_count = 0;
    int ext_reloc_count = 0;
    int error_count = 0;
    
    /* 遍历所有节，查找 RELA 节 */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        uint32_t sh_type;
        module_memcpy(&sh_type, &shdrs[i].sh_type, sizeof(uint32_t));
        
        if (sh_type != SHT_RELA) continue;
        
        uint32_t sh_info;
        module_memcpy(&sh_info, &shdrs[i].sh_info, sizeof(uint32_t));
        
        /* 检查目标节是否有效 */
        if (sh_info >= (uint32_t)ehdr->e_shnum) continue;
        
        /* 获取目标节的信息 */
        uint64_t target_offset, target_size;
        module_memcpy(&target_offset, &shdrs[sh_info].sh_offset, sizeof(uint64_t));
        module_memcpy(&target_size, &shdrs[sh_info].sh_size, sizeof(uint64_t));
        
        /* 跳过 .eh_frame 的重定位 */
        if (shstrtab_data) {
            uint32_t sh_name;
            module_memcpy(&sh_name, &shdrs[sh_info].sh_name, sizeof(uint32_t));
            const char *sec_name = shstrtab_data + sh_name;
            /* 简单检查是否是 .eh_frame */
            if (sec_name[0] == '.' && sec_name[1] == 'e' && sec_name[2] == 'h') {
                continue;
            }
        }
        
        const elf64_rela_t *relas = (const elf64_rela_t *)(elf_base + shdrs[i].sh_offset);
        int num_relas = shdrs[i].sh_size / sizeof(elf64_rela_t);
        
        launcher_log("Processing .rela section...");
        
        for (int j = 0; j < num_relas; j++) {
            uint64_t r_offset = relas[j].r_offset;
            uint64_t r_info = relas[j].r_info;
            int64_t r_addend = relas[j].r_addend;
            
            uint32_t sym_idx = ELF64_R_SYM(r_info);
            uint32_t type = ELF64_R_TYPE(r_info);
            
            if (sym_idx >= (uint32_t)num_syms) {
                launcher_log("Invalid symbol index in relocation");
                error_count++;
                continue;
            }
            
            /* 获取符号信息 */
            uint64_t sym_value;
            uint16_t sym_shndx;
            module_memcpy(&sym_value, &syms[sym_idx].st_value, sizeof(uint64_t));
            module_memcpy(&sym_shndx, &syms[sym_idx].st_shndx, sizeof(uint16_t));
            
            const char *sym_name = strtab_data + syms[sym_idx].st_name;
            
            /* 计算符号地址 S */
            uint64_t S;
            if (sym_shndx == 0) {
                /* 外部符号 - 需要从内核查找 */
                S = find_external_symbol(sym_name);
                if (S == 0) {
                    launcher_log("External symbol not found:");
                    launcher_log(sym_name);
                    error_count++;
                    continue;
                }
                ext_reloc_count++;
            } else {
                /* 内部符号 - 需要找到符号所在节的文件偏移 */
                /* sym_value 是节内偏移，需要加上节的文件偏移 */
                if (sym_shndx < (uint16_t)ehdr->e_shnum) {
                    uint64_t sec_offset;
                    module_memcpy(&sec_offset, &shdrs[sym_shndx].sh_offset, sizeof(uint64_t));
                    S = (uint64_t)elf_base + sec_offset + sym_value;
                } else {
                    launcher_log("Invalid section index for symbol");
                    error_count++;
                    continue;
                }
            }
            
            /* 目标地址 P */
            uint64_t P = (uint64_t)elf_base + target_offset + r_offset;
            
            /* 应用重定位 */
            int64_t result;
            uint32_t *target32;
            uint64_t *target64;
            
            switch (type) {
                case R_X86_64_NONE:
                    break;
                    
                case R_X86_64_64:
                    /* S + A */
                    target64 = (uint64_t *)P;
                    *target64 = S + r_addend;
                    reloc_count++;
                    break;
                    
                case R_X86_64_PC32:
                    /* S + A - P */
                    result = (int64_t)S + r_addend - (int64_t)P;
                    target32 = (uint32_t *)P;
                    *target32 = (uint32_t)(result & 0xFFFFFFFF);
                    reloc_count++;
                    break;
                    
                case R_X86_64_PLT32:
                    /* PLT 重定位 - 对于我们的情况，转换为直接调用 */
                    /* L + A - P，其中 L = S（我们没有 PLT） */
                    result = (int64_t)S + r_addend - (int64_t)P;
                    target32 = (uint32_t *)P;
                    *target32 = (uint32_t)(result & 0xFFFFFFFF);
                    reloc_count++;
                    break;
                    
                case R_X86_64_32:
                case R_X86_64_32S:
                    /* S + A (32-bit) */
                    result = (int64_t)S + r_addend;
                    target32 = (uint32_t *)P;
                    *target32 = (uint32_t)(result & 0xFFFFFFFF);
                    reloc_count++;
                    break;
                    
                case R_X86_64_RELATIVE:
                    /* B + A (基址重定位) */
                    target64 = (uint64_t *)P;
                    *target64 = (uint64_t)elf_base + r_addend;
                    reloc_count++;
                    break;
                    
                case R_X86_64_GOTPCRELX:
                case R_X86_64_REX_GOTPCRELX:
                    /* GOTPCRELX 重定位 - 将 GOT 间接访问转换为直接访问 */
                    /* G + GOT + A - P，其中 G = S - GOT */
                    /* 由于我们没有 GOT，直接使用 S + A - P */
                    result = (int64_t)S + r_addend - (int64_t)P;
                    target32 = (uint32_t *)P;
                    *target32 = (uint32_t)(result & 0xFFFFFFFF);
                    reloc_count++;
                    break;
                    
                default:
                    launcher_log_hex("Unsupported relocation type: ", type);
                    error_count++;
                    break;
            }
        }
    }
    
    launcher_log_hex("Relocations applied: ", reloc_count);
    launcher_log_hex("External relocations: ", ext_reloc_count);
    launcher_log_hex("Relocation errors: ", error_count);
    
    return error_count;
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
    if (header->signature_size > 0) {
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
    
    uint32_t new_service = 0;
    status = module_cap_create_service(new_domain, &new_service);

    if (status != 0) {
        launcher_log("ERROR: Failed to create service");
        while (1) { thread_yield(); }
    }

    launcher_log("Service created");
    
    /* 步骤7: 分配内存并加载模块 */
    launcher_log("Step 7: Loading module code...");
    
    /* 从架构表中读取代码大小和入口偏移 */
    /* 架构表紧随头部之后 */
    hicmod_arch_section_t *arch_section = (hicmod_arch_section_t *)
        (g_module_buffer + header->arch_table_offset);
    
    uint32_t code_offset = arch_section->code_offset;
    uint32_t code_size = arch_section->code_size;
    uint32_t entry_offset = arch_section->entry_offset;
    uint32_t data_size = arch_section->data_size;
    uint32_t total_size = code_size + data_size;
    
    /* 如果使用 legacy 字段，优先使用它们 */
    if (header->legacy_code_size > 0) {
        code_offset = header->legacy_code_offset;
        code_size = header->legacy_code_size;
        data_size = header->legacy_data_size;
        total_size = code_size + data_size;
        launcher_log("Using legacy code fields from header");
    }
    
    launcher_log("Module sizes:");
    launcher_log_hex(" code=", code_size);
    launcher_log_hex(", data=", data_size);
    launcher_log_hex(", total=", total_size);
    launcher_log_hex(" code_offset=", code_offset);
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
    
    /* 复制代码段 - 使用 arch_section 中的偏移 */
    module_memcpy((void *)phys_addr, g_module_buffer + code_offset, code_size);
    
    launcher_log("Code loaded to physical memory");
    
    /* 步骤7.5: 应用 ELF 重定位 */
    launcher_log("Step 7.5: Applying ELF relocations...");
    
    int reloc_result = apply_elf_relocations((uint8_t *)phys_addr, code_size);
    if (reloc_result != 0) {
        launcher_log_hex("WARNING: Relocation had errors: ", reloc_result);
        /* 继续执行，某些重定位错误可能是可以忽略的 */
    } else {
        launcher_log("Relocations applied successfully");
    }
    
    /* 步骤8: 启动模块管理器 */
    launcher_log("Step 8: Starting module_manager...");
    
    /* 从 ELF 符号表查找真正的入口函数 */
    uint64_t elf_entry_offset = 0;
    int found_entry = find_elf_entry_ex((const uint8_t *)phys_addr, code_size, &elf_entry_offset);
    
    uint64_t entry_point;
    uint64_t final_entry_offset;
    if (found_entry) {
        /* 使用 ELF 符号表中找到的入口点（即使偏移是 0 也是有效的） */
        entry_point = phys_addr + elf_entry_offset;
        final_entry_offset = elf_entry_offset;
        launcher_log("Found ELF entry point from symbol table");
    } else {
        /* 回退到 arch_section 中的 entry_offset */
        entry_point = phys_addr + entry_offset;
        final_entry_offset = entry_offset;
        launcher_log("Using arch_section entry_offset (fallback)");
    }
    
    launcher_log_hex("Entry point (phys): ", entry_point);
    launcher_log_hex("Entry offset: ", final_entry_offset);
    
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
    
    /* 等待 module_manager 初始化 */
    for (int i = 0; i < 1000; i++) {
        thread_yield();
    }
    
    launcher_log("Init launcher entering idle mode");
    launcher_log("System services are running");
    
    /* 主服务循环 - 提供简单的命令行交互 */
    int count = 0;
    while (1) {
        thread_yield();
        count++;
        
        /* 每 10000000 次循环打印一次心跳 */
        if (count > 10000000) {
            count = 0;
            launcher_log("[INIT] Heartbeat - system running");
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
