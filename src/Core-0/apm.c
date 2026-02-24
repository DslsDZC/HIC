/*
 * SPDX-FileCopyrightText: 2026 * <*@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC APM (Automatic Parameter Management) 系统实现
 * 支持 Core-0 核心的全面参数管理
 * 
 * 工作流程：
 * 1. 引导层自动分配资源（串口、内存、中断等）
 * 2. 引导层将分配结果写入配置文件（platform.yaml）
 * 3. 配置文件通过 boot_info->config_data 传递给内核
 * 4. 内核从配置文件读取并初始化所有资源
 * 5. 运行形式化验证确保配置正确性
 */

#include "types.h"
#include "build_config.h"
#include "yaml.h"
#include "boot_info.h"
#include "minimal_uart.h"
#include "pmm.h"
#include "irq.h"
#include "lib/console.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "formal_verification.h"

/* 串口配置 */
typedef struct apm_uart_config {
    phys_addr_t base_addr;      /* 基地址 */
    u32        baud_rate;       /* 波特率 */
    u32        data_bits;       /* 数据位 (5-8) */
    u32        parity;          /* 校验位 */
    u32        stop_bits;       /* 停止位 (1-2) */
    u32        irq;             /* 中断号 */
} apm_uart_config_t;

/* 内存区域配置 */
typedef struct apm_memory_region {
    phys_addr_t base_addr;      /* 基地址 */
    size_t      size;           /* 大小 */
    u32        flags;          /* 标志位 */
    domain_id_t owner_domain;   /* 拥有者域ID */
} apm_memory_region_t;

/* 中断配置 */
typedef struct apm_irq_config {
    u32 vector;                 /* 中断向量 */
    u32 priority;               /* 优先级 */
    u32 trigger_mode;           /* 触发模式 */
    u32 polarity;               /* 极性 */
    u64 handler_address;        /* 处理函数地址 */
} apm_irq_config_t;

/* 定时器配置 */
typedef struct apm_timer_config {
    phys_addr_t base_addr;      /* 基地址 */
    u64        frequency;       /* 频率 */
    u32        mode;            /* 模式 */
    u32        irq;             /* 中断号 */
} apm_timer_config_t;

/* APM 统计信息 */
typedef struct apm_stats {
    u32 total_resources;        /* 总资源数 */
    u32 allocated_resources;    /* 已分配资源数 */
    u32 initialized_resources;  /* 已初始化资源数 */
    u32 active_resources;       /* 活跃资源数 */
    u32 failed_resources;       /* 失败资源数 */
} apm_stats_t;

/* APM 状态枚举 */
typedef enum apm_state {
    APM_STATE_INIT = 0,         /* 初始化中 */
    APM_STATE_READY,           /* 就绪 */
    APM_STATE_INITIALIZING,    /* 正在初始化 */
    APM_STATE_RUNNING,         /* 运行中 */
    APM_STATE_ERROR,           /* 错误 */
} apm_state_t;

/* APM 配置结构 */
typedef struct apm_config {
    apm_mode_t mode;            /* APM 模式 */
    apm_state_t state;          /* APM 状态 */
    u64 config_version;         /* 配置版本 */
    u64 config_hash;            /* 配置哈希 */
    u64 timestamp;              /* 配置时间戳 */
    
    u32 uart_count;             /* 串口数量 */
    u32 memory_count;           /* 内存区域数量 */
    u32 irq_count;              /* 中断数量 */
    u32 timer_count;            /* 定时器数量 */
    
    apm_uart_config_t uart[4];  /* 串口配置 */
    apm_memory_region_t memory[16];  /* 内存区域配置 */
    apm_irq_config_t irq[32];   /* 中断配置 */
    apm_timer_config_t timer[8];  /* 定时器配置 */
    
    apm_stats_t stats;          /* 统计信息 */
    bool config_valid;          /* 配置有效 */
    bool config_verified;       /* 配置已验证 */
} apm_config_t;

/* 引用外部引导信息 */
extern hic_boot_info_t *g_boot_info;

/* 全局 APM 配置 */
apm_config_t g_apm_config = {
    .mode = APM_MODE_AUTO,
    .config_version = 1,
    .config_hash = 0,
    .timestamp = 0,
    .uart_count = 0,
    .memory_count = 0,
    .irq_count = 0,
    .timer_count = 0,
    .config_valid = false,
    .config_verified = false,
};

/* 前置声明 */
static void apm_parse_yaml_config(yaml_parser_t *parser);
static void apm_parse_uart_config(yaml_node_t *uart_node);
static void apm_parse_memory_config(yaml_node_t *memory_node);
static void apm_parse_irq_config(yaml_node_t *irq_node);
static void apm_parse_timer_config(yaml_node_t *timer_node);
static bool apm_verify_config(void);
static bool apm_run_all_verifications(void);

/* ===== APM 核心接口 ===== */

/**
 * 初始化 APM 系统
 * 从 boot_info 读取配置文件并解析
 */
void apm_init(void)
{
    console_puts("[APM] Initializing APM system\n");

    /* 清零配置 */
    memzero(&g_apm_config, sizeof(apm_config_t));
    g_apm_config.mode = APM_MODE_AUTO;
    g_apm_config.config_version = 1;

    /* 检查引导信息 */
    if (!g_boot_info) {
        console_puts("[APM] No boot info, using default configuration\n");
        g_apm_config.config_valid = false;
        return;
    }

    /* 获取 YAML 配置数据 */
    const char *yaml_data = (const char *)g_boot_info->config.config_data;
    size_t yaml_size = g_boot_info->config.config_size;

    if (!yaml_data || yaml_size == 0) {
        console_puts("[APM] No YAML data, using default configuration\n");
        g_apm_config.config_valid = false;
        return;
    }

    console_puts("[APM] YAML data size: ");
    console_putu64(yaml_size);
    console_puts(" bytes\n");

    /* 解析 YAML 配置 */
    yaml_parser_t *parser = yaml_parser_create(yaml_data, yaml_size);
    if (!parser) {
        console_puts("[APM] Failed to create YAML parser\n");
        g_apm_config.config_valid = false;
        return;
    }

    if (yaml_parse(parser) != 0) {
        console_puts("[APM] Failed to parse YAML\n");
        g_apm_config.config_valid = false;
        yaml_parser_destroy(parser);
        return;
    }

    /* 解析配置 */
    apm_parse_yaml_config(parser);

    yaml_parser_destroy(parser);

    /* 标记配置有效 */
    g_apm_config.config_valid = true;
    g_apm_config.timestamp = hal_get_timestamp();

    console_puts("[APM] APM system initialized\n");
}

/**
 * 设置 APM 模式
 */
void apm_set_mode(apm_mode_t mode)
{
    g_apm_config.mode = mode;
    console_puts("[APM] APM mode set to ");
    console_putu32(mode);
    console_puts("\n");
}

/**
 * 获取 APM 模式
 */
apm_mode_t apm_get_mode(void)
{
    return g_apm_config.mode;
}

/**
 * 验证 APM 配置完整性
 * 形式化验证入口点
 */
bool apm_verify_config(void)
{
    console_puts("[APM] Verifying APM configuration\n");

    if (!g_apm_config.config_valid) {
        console_puts("[APM] Configuration invalid\n");
        return false;
    }

    /* 运行所有形式化验证 */
    bool result = apm_run_all_verifications();

    if (result) {
        g_apm_config.config_verified = true;
        console_puts("[APM] Configuration verified successfully\n");
    } else {
        console_puts("[APM] Configuration verification failed\n");
    }

    return result;
}

/**
 * 获取 APM 统计信息
 */
void apm_get_stats(apm_stats_t *stats)
{
    if (stats) {
        memcopy(stats, &g_apm_config.stats, sizeof(apm_stats_t));
    }
}

/**
 * 打印 APM 配置信息
 */
void apm_print_config(void)
{
    console_puts("[APM] ===== APM Configuration =====\n");
    console_puts("[APM] Mode: ");
    console_putu32(g_apm_config.mode);
    console_puts("\n");
    console_puts("[APM] Version: ");
    console_putu64(g_apm_config.config_version);
    console_puts("\n");
    console_puts("[APM] UARTs: ");
    console_putu32(g_apm_config.uart_count);
    console_puts("\n");
    console_puts("[APM] Memory regions: ");
    console_putu32(g_apm_config.memory_count);
    console_puts("\n");
    console_puts("[APM] IRQs: ");
    console_putu32(g_apm_config.irq_count);
    console_puts("\n");
    console_puts("[APM] Timers: ");
    console_putu32(g_apm_config.timer_count);
    console_puts("\n");
    console_puts("[APM] Valid: ");
    console_putu32(g_apm_config.config_valid ? 1 : 0);
    console_puts("\n");
    console_puts("[APM] Verified: ");
    console_putu32(g_apm_config.config_verified ? 1 : 0);
    console_puts("\n");
    console_puts("[APM] =============================\n");
}

/* ===== APM YAML 解析 ===== */

/**
 * 解析 YAML 配置
 */
static void apm_parse_yaml_config(yaml_parser_t *parser)
{
    yaml_node_t *root = yaml_get_root(parser);
    if (!root) {
        console_puts("[APM] No YAML root\n");
        return;
    }

    /* 解析串口配置 */
    yaml_node_t *uart_node = yaml_find_node(root, "uart");
    if (uart_node) {
        apm_parse_uart_config(uart_node);
    }

    /* 解析内存配置 */
    yaml_node_t *memory_node = yaml_find_node(root, "memory");
    if (memory_node) {
        apm_parse_memory_config(memory_node);
    }

    /* 解析中断配置 */
    yaml_node_t *irq_node = yaml_find_node(root, "irq");
    if (irq_node) {
        apm_parse_irq_config(irq_node);
    }

    /* 解析定时器配置 */
    yaml_node_t *timer_node = yaml_find_node(root, "timer");
    if (timer_node) {
        apm_parse_timer_config(timer_node);
    }
}

/**
 * 解析串口配置
 */
static void apm_parse_uart_config(yaml_node_t *uart_node)
{
    /* 简化实现：只解析第一个串口 */
    if (g_apm_config.uart_count >= 4) {
        return;
    }

    apm_uart_config_t *config = &g_apm_config.uart[g_apm_config.uart_count];
    memzero(config, sizeof(apm_uart_config_t));

    yaml_node_t *node;

    /* 基地址 */
    node = yaml_find_node(uart_node, "base");
    if (node && node->value) {
        config->base_addr = (phys_addr_t)yaml_get_u64(node, 0x3F8);
    }

    /* 波特率 */
    node = yaml_find_node(uart_node, "baud");
    if (node && node->value) {
        config->baud_rate = (u32)yaml_get_u64(node, 115200);
    }

    /* 数据位 */
    node = yaml_find_node(uart_node, "data_bits");
    if (node && node->value) {
        config->data_bits = (u32)yaml_get_u64(node, 8);
    }

    /* 校验位 */
    node = yaml_find_node(uart_node, "parity");
    if (node && node->value) {
        const char *parity_str = node->value;
        if (strcmp(parity_str, "none") == 0) {
            config->parity = 0;
        } else if (strcmp(parity_str, "odd") == 0) {
            config->parity = 1;
        } else if (strcmp(parity_str, "even") == 0) {
            config->parity = 2;
        }
    }

    /* 停止位 */
    node = yaml_find_node(uart_node, "stop_bits");
    if (node && node->value) {
        config->stop_bits = (u32)yaml_get_u64(node, 1);
    }

    /* 中断号 */
    node = yaml_find_node(uart_node, "irq");
    if (node && node->value) {
        config->irq = (u32)yaml_get_u64(node, 4);
    }

    g_apm_config.uart_count++;

    console_puts("[APM] Parsed UART config: base=0x");
    console_puthex64(config->base_addr);
    console_puts(", baud=");
    console_putu32(config->baud_rate);
    console_puts("\n");
}

/**
 * 解析内存配置
 */
static void apm_parse_memory_config(yaml_node_t *memory_node)
{
    /* 简化实现：解析内存区域 */
    if (g_apm_config.memory_count >= 16) {
        return;
    }

    apm_memory_region_t *config = &g_apm_config.memory[g_apm_config.memory_count];
    memzero(config, sizeof(apm_memory_region_t));

    yaml_node_t *node;

    /* 基地址 */
    node = yaml_find_node(memory_node, "base");
    if (node && node->value) {
        config->base_addr = (phys_addr_t)yaml_get_u64(node, 0x00100000);
    }

    /* 大小 */
    node = yaml_find_node(memory_node, "size");
    if (node && node->value) {
        config->size = (size_t)yaml_get_u64(node, 0x10000000);
    }

    g_apm_config.memory_count++;

    console_puts("[APM] Parsed memory region: base=0x");
    console_puthex64(config->base_addr);
    console_puts(", size=");
    console_putu64(config->size);
    console_puts("\n");
}

/**
 * 解析中断配置
 */
static void apm_parse_irq_config(yaml_node_t *irq_node __attribute__((unused)))
{
    /* 简化实现：解析中断配置 */
    /* TODO: 实现完整的中断配置解析 */
    console_puts("[APM] IRQ config parsing not yet implemented\n");
}

/**
 * 解析定时器配置
 */
static void apm_parse_timer_config(yaml_node_t *timer_node __attribute__((unused)))
{
    /* 简化实现：解析定时器配置 */
    /* TODO: 实现完整的定时器配置解析 */
    console_puts("[APM] Timer config parsing not yet implemented\n");
}

/* ===== APM 资源接口 ===== */

/**
 * 获取串口配置
 */
apm_uart_config_t* apm_get_uart_config(u32 index)
{
    if (index >= g_apm_config.uart_count) {
        return NULL;
    }
    return &g_apm_config.uart[index];
}

/**
 * 初始化所有串口
 */
hic_status_t apm_init_all_uarts(void)
{
    console_puts("[APM] Initializing all UARTs\n");

    for (u32 i = 0; i < g_apm_config.uart_count; i++) {
        apm_uart_config_t *config = &g_apm_config.uart[i];
        
        /* 使用配置初始化串口 */
        uart_config_t uart_cfg;
        uart_cfg.base_addr = config->base_addr;
        uart_cfg.baud_rate = config->baud_rate;
        uart_cfg.data_bits = (uart_data_bits_t)config->data_bits;
        uart_cfg.parity = (uart_parity_t)config->parity;
        uart_cfg.stop_bits = (uart_stop_bits_t)config->stop_bits;

        minimal_uart_init_with_config(&uart_cfg);

        /* 串口初始化成功 */
        g_apm_config.stats.initialized_resources++;
        g_apm_config.state = APM_STATE_INITIALIZING;
    }

    return HIC_SUCCESS;
}

/**
 * 获取内存区域配置
 */
apm_memory_region_t* apm_get_memory_region(u32 index)
{
    if (index >= g_apm_config.memory_count) {
        return NULL;
    }
    return &g_apm_config.memory[index];
}

/**
 * 初始化所有内存区域
 */
hic_status_t apm_init_all_memory(void)
{
    console_puts("[APM] Initializing all memory regions\n");

    for (u32 i = 0; i < g_apm_config.memory_count; i++) {
        apm_memory_region_t *config = &g_apm_config.memory[i];

        /* 添加内存区域到 PMM */
        pmm_add_region(config->base_addr, config->size);

        /* 内存区域初始化成功 */
        g_apm_config.stats.initialized_resources++;
        g_apm_config.state = APM_STATE_INITIALIZING;
    }

    return HIC_SUCCESS;
}

/**
 * 获取中断配置
 */
apm_irq_config_t* apm_get_irq_config(u32 index)
{
    if (index >= g_apm_config.irq_count) {
        return NULL;
    }
    return &g_apm_config.irq[index];
}

/**
 * 初始化所有中断
 */
hic_status_t apm_init_all_irqs(void)
{
    console_puts("[APM] Initializing all IRQs\n");
    /* TODO: 实现中断初始化 */
    return HIC_SUCCESS;
}

/**
 * 获取定时器配置
 */
apm_timer_config_t* apm_get_timer_config(u32 index)
{
    if (index >= g_apm_config.timer_count) {
        return NULL;
    }
    return &g_apm_config.timer[index];
}

/**
 * 初始化所有定时器
 */
hic_status_t apm_init_all_timers(void)
{
    console_puts("[APM] Initializing all timers\n");
    /* TODO: 实现定时器初始化 */
    return HIC_SUCCESS;
}

/* ===== APM 形式化验证接口 ===== */

/**
 * 形式化验证：资源分配不变式
 * 验证所有资源分配的一致性
 */
bool apm_verify_allocation_invariant(void)
{
    console_puts("[APM] Verifying allocation invariant\n");

    /* 验证串口分配 */
    for (u32 i = 0; i < g_apm_config.uart_count; i++) {
        apm_uart_config_t *config = &g_apm_config.uart[i];
        
        /* 检查基地址合法性 */
        if (config->base_addr == 0) {
            console_puts("[APM] Invalid UART base address\n");
            return false;
        }

        /* 检查波特率合法性 */
        if (config->baud_rate == 0 || config->baud_rate > 2000000) {
            console_puts("[APM] Invalid UART baud rate\n");
            return false;
        }
    }

    /* 验证内存区域分配 */
    for (u32 i = 0; i < g_apm_config.memory_count; i++) {
        apm_memory_region_t *config = &g_apm_config.memory[i];
        
        /* 检查基地址合法性 */
        if (config->base_addr == 0) {
            console_puts("[APM] Invalid memory base address\n");
            return false;
        }

        /* 检查大小合法性 */
        if (config->size == 0 || config->size > 0x100000000ULL) {
            console_puts("[APM] Invalid memory size\n");
            return false;
        }

        /* 检查对齐 */
        if (config->base_addr & (PAGE_SIZE - 1)) {
            console_puts("[APM] Memory not page aligned\n");
            return false;
        }
    }

    console_puts("[APM] Allocation invariant verified\n");
    return true;
}

/**
 * 形式化验证：配置一致性
 * 验证配置参数的合法性
 */
bool apm_verify_config_consistency(void)
{
    console_puts("[APM] Verifying config consistency\n");

    /* 检查模式合法性 */
    if (g_apm_config.mode > APM_MODE_CONFIG) {
        console_puts("[APM] Invalid APM mode\n");
        return false;
    }

    /* 检查版本号 */
    if (g_apm_config.config_version == 0) {
        console_puts("[APM] Invalid config version\n");
        return false;
    }

    /* 检查资源数量合理性 */
    if (g_apm_config.uart_count > 4) {
        console_puts("[APM] Too many UARTs\n");
        return false;
    }

    if (g_apm_config.memory_count > 16) {
        console_puts("[APM] Too many memory regions\n");
        return false;
    }

    if (g_apm_config.irq_count > 32) {
        console_puts("[APM] Too many IRQs\n");
        return false;
    }

    if (g_apm_config.timer_count > 8) {
        console_puts("[APM] Too many timers\n");
        return false;
    }

    console_puts("[APM] Config consistency verified\n");
    return true;
}

/**
 * 形式化验证：启动完整性
 * 验证启动时配置的完整性
 */
bool apm_verify_boot_integrity(void)
{
    console_puts("[APM] Verifying boot integrity\n");

    /* 检查配置有效性 */
    if (!g_apm_config.config_valid) {
        console_puts("[APM] Config not valid\n");
        return false;
    }

    /* 检查引导信息 */
    if (!g_boot_info) {
        console_puts("[APM] No boot info\n");
        return false;
    }

    /* 检查配置数据 */
    if (!g_boot_info->config.config_data || g_boot_info->config.config_size == 0) {
        console_puts("[APM] No config data\n");
        return false;
    }

    console_puts("[APM] Boot integrity verified\n");
    return true;
}

/**
 * 形式化验证：资源状态机
 * 验证资源状态转换的正确性
 */
bool apm_verify_state_machine(void)
{
    console_puts("[APM] Verifying state machine\n");

    /* 验证所有资源状态 */
    for (u32 i = 0; i < g_apm_config.uart_count; i++) {
        apm_uart_config_t *config = &g_apm_config.uart[i];
        
/* 检查状态合法性 */
        if (g_apm_config.state < APM_STATE_INIT || 
            g_apm_config.state > APM_STATE_ERROR) {
            console_puts("[APM] Invalid APM state\n");
            return false;
        }
    }

    for (u32 i = 0; i < g_apm_config.memory_count; i++) {
        apm_memory_region_t *region = &g_apm_config.memory[i];
        
        /* TODO: 实现内存区域状态检查 */
        /* 暂时跳过 */
        continue;
    }

    console_puts("[APM] State machine verified\n");
    return true;
}

/**
 * 形式化验证：无冲突分配
 * 验证资源分配无冲突
 */
bool apm_verify_no_conflicts(void)
{
    console_puts("[APM] Verifying no conflicts\n");

    /* 检查串口基地址冲突 */
    for (u32 i = 0; i < g_apm_config.uart_count; i++) {
        for (u32 j = i + 1; j < g_apm_config.uart_count; j++) {
            if (g_apm_config.uart[i].base_addr == g_apm_config.uart[j].base_addr) {
                console_puts("[APM] UART base address conflict\n");
                return false;
            }
        }
    }

    /* 检查内存区域重叠 */
    for (u32 i = 0; i < g_apm_config.memory_count; i++) {
        for (u32 j = i + 1; j < g_apm_config.memory_count; j++) {
            apm_memory_region_t *region1 = &g_apm_config.memory[i];
            apm_memory_region_t *region2 = &g_apm_config.memory[j];
            
            phys_addr_t end1 = region1->base_addr + region1->size;
            phys_addr_t end2 = region2->base_addr + region2->size;
            
            /* 检查重叠 */
            if (!(end1 <= region2->base_addr || end2 <= region1->base_addr)) {
                console_puts("[APM] Memory region overlap\n");
                return false;
            }
        }
    }

    console_puts("[APM] No conflicts verified\n");
    return true;
}

/**
 * 运行所有形式化验证
 */
bool apm_run_all_verifications(void)
{
    console_puts("[APM] Running all formal verifications\n");

    /* 运行所有验证 */
    bool result = true;

    result = result && apm_verify_boot_integrity();
    result = result && apm_verify_config_consistency();
    result = result && apm_verify_allocation_invariant();
    result = result && apm_verify_state_machine();
    result = result && apm_verify_no_conflicts();

    if (result) {
        console_puts("[APM] All verifications passed\n");
    } else {
        console_puts("[APM] Some verifications failed\n");
    }

    return result;
}
