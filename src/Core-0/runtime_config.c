/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核运行时配置系统实现
 * 支持通过引导层传递配置参数，实现无需重新编译即可调整内核行为
 */

#include "runtime_config.h"
#include "boot_info.h"
#include "yaml.h"
#include "lib/console.h"
#include "lib/string.h"
#include "lib/mem.h"

/* 内部字符串复制函数 */
static void strcopy_internal(char* dest, const char* src, size_t size)
{
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* 全局运行时配置 */
runtime_config_t g_runtime_config;

/* 配置变更回调 */
typedef struct config_callback {
    char name[64];
    config_change_callback_t callback;
} config_callback_t;

static config_callback_t g_config_callbacks[32];
static u32 g_num_callbacks = 0;

/* 默认配置值初始化 */
static void init_default_config(void)
{
    memzero(&g_runtime_config, sizeof(runtime_config_t));
    
    /* 系统配置默认值 */
    g_runtime_config.log_level = LOG_LEVEL_INFO;
    g_runtime_config.scheduler_policy = SCHEDULER_POLICY_PRIORITY;
    g_runtime_config.memory_policy = MEMORY_POLICY_BUDDY;
    g_runtime_config.security_level = SECURITY_LEVEL_STANDARD;
    
    /* 调度器配置默认值 */
    g_runtime_config.time_slice_ms = 10;
    g_runtime_config.max_threads = 256;
    g_runtime_config.idle_timeout_ms = 1000;
    
    /* 内存配置默认值 */
    g_runtime_config.heap_size_mb = 128;
    g_runtime_config.stack_size_kb = 8;
    g_runtime_config.page_cache_percent = 20;
    g_runtime_config.enable_swap = false;
    
    /* 安全配置默认值 */
    g_runtime_config.enable_secure_boot = true;
    g_runtime_config.enable_kaslr = true;
    g_runtime_config.enable_smap = true;
    g_runtime_config.enable_smep = true;
    g_runtime_config.enable_audit = true;
    
    /* 性能配置默认值 */
    g_runtime_config.enable_perf_counters = true;
    g_runtime_config.enable_fast_path = true;
    g_runtime_config.fast_path_threshold = 100;
    
    /* 调试配置默认值 */
    g_runtime_config.enable_debug = false;
    g_runtime_config.enable_trace = false;
    g_runtime_config.enable_verbose = false;
    g_runtime_config.debug_port = 0x3F8;
    
    /* 设备配置默认值 */
    g_runtime_config.enable_pci = true;
    g_runtime_config.enable_acpi = true;
    g_runtime_config.enable_serial = true;
    g_runtime_config.serial_baud = 115200;
    
    /* 能力系统配置默认值 */
    g_runtime_config.max_capabilities = 65536;
    g_runtime_config.enable_capability_derivation = true;
    
    /* 域配置默认值 */
    g_runtime_config.max_domains = 16;
    g_runtime_config.domain_stack_size_kb = 16;
    
    /* 中断配置默认值 */
    g_runtime_config.max_irqs = 256;
    g_runtime_config.enable_irq_fairness = true;
    
    /* 模块配置默认值 */
    g_runtime_config.enable_module_loading = false;
    g_runtime_config.max_modules = 32;
    
    /* 配置元数据 */
    g_runtime_config.config_source = CONFIG_SOURCE_DEFAULT;
    g_runtime_config.config_timestamp = 0;
    memzero(g_runtime_config.config_hash, sizeof(g_runtime_config.config_hash));
}

/* 初始化运行时配置系统 */
void runtime_config_init(void)
{
    console_puts("[CONFIG] Initializing runtime configuration system...\n");
    
    init_default_config();
    
    console_puts("[CONFIG] Runtime configuration initialized with defaults\n");
}

/* 从引导信息加载配置 */
void runtime_config_load_from_bootinfo(void)
{
    extern boot_state_t g_boot_state;
    
    if (!g_boot_state.boot_info) {
        console_puts("[CONFIG] No boot info available, using defaults\n");
        return;
    }
    
    console_puts("[CONFIG] Loading configuration from bootloader...\n");
    
    /* 从命令行加载配置 */
    if (g_boot_state.boot_info->cmdline[0] != '\0') {
        runtime_config_load_from_cmdline(g_boot_state.boot_info->cmdline);
    }
    
    /* 从platform.yaml加载配置 */
    if (g_boot_state.boot_info->config_data && g_boot_state.boot_info->config_size > 0) {
        runtime_config_load_from_yaml(
            (const char*)g_boot_state.boot_info->config_data,
            g_boot_state.boot_info->config_size
        );
    }
    
    /* 验证配置 */
    if (!runtime_config_validate()) {
        console_puts("[CONFIG] WARNING: Configuration validation failed, using defaults\n");
        init_default_config();
    }
    
    g_runtime_config.config_source = CONFIG_SOURCE_BOOTLOADER;
    g_runtime_config.config_timestamp = g_boot_state.boot_info->boot_log.boot_time;
    
    console_puts("[CONFIG] Configuration loaded successfully\n");
}

/* 从命令行加载配置 */
void runtime_config_load_from_cmdline(const char* cmdline)
{
    if (!cmdline || cmdline[0] == '\0') {
        return;
    }
    
    console_puts("[CONFIG] Parsing command line: ");
    console_puts(cmdline);
    console_puts("\n");
    
    /* 解析命令行参数 */
    char cmdline_copy[256];
    strcopy_internal(cmdline_copy, cmdline, sizeof(cmdline_copy));
    
    char* token = cmdline_copy;
    while (*token != '\0') {
        /* 跳过空格 */
        while (*token == ' ') token++;
        if (*token == '\0') break;
        
        /* 检查是否为配置项 */
        if (token[0] == '-' && token[1] == '-') {
            char* key = token + 2;
            char* value = key;
            
            /* 查找等号 */
            while (*value != '\0' && *value != '=') value++;
            
            if (*value == '=') {
                *value = '\0';
                value++;
                
                /* 设置配置项 */
                /* 处理所有配置项 */
                if (strcmp(key, "log_level") == 0) {
                    if (strcmp(value, "debug") == 0) {
                        g_runtime_config.log_level = LOG_LEVEL_DEBUG;
                    } else if (strcmp(value, "info") == 0) {
                        g_runtime_config.log_level = LOG_LEVEL_INFO;
                    } else if (strcmp(value, "warn") == 0) {
                        g_runtime_config.log_level = LOG_LEVEL_WARN;
                    } else if (strcmp(value, "error") == 0) {
                        g_runtime_config.log_level = LOG_LEVEL_ERROR;
                    } else if (strcmp(value, "trace") == 0) {
                        g_runtime_config.log_level = LOG_LEVEL_TRACE;
                    }
                } else if (strcmp(key, "scheduler_policy") == 0) {
                    if (strcmp(value, "fifo") == 0) {
                        g_runtime_config.scheduler_policy = SCHEDULER_POLICY_FIFO;
                    } else if (strcmp(value, "rr") == 0) {
                        g_runtime_config.scheduler_policy = SCHEDULER_POLICY_RR;
                    } else if (strcmp(value, "priority") == 0) {
                        g_runtime_config.scheduler_policy = SCHEDULER_POLICY_PRIORITY;
                    }
                } else if (strcmp(key, "memory_policy") == 0) {
                    if (strcmp(value, "firstfit") == 0) {
                        g_runtime_config.memory_policy = MEMORY_POLICY_FIRSTFIT;
                    } else if (strcmp(value, "bestfit") == 0) {
                        g_runtime_config.memory_policy = MEMORY_POLICY_BESTFIT;
                    } else if (strcmp(value, "buddy") == 0) {
                        g_runtime_config.memory_policy = MEMORY_POLICY_BUDDY;
                    }
                } else if (strcmp(key, "security_level") == 0) {
                    if (strcmp(value, "minimal") == 0) {
                        g_runtime_config.security_level = SECURITY_LEVEL_MINIMAL;
                    } else if (strcmp(value, "standard") == 0) {
                        g_runtime_config.security_level = SECURITY_LEVEL_STANDARD;
                    } else if (strcmp(value, "strict") == 0) {
                        g_runtime_config.security_level = SECURITY_LEVEL_STRICT;
                    }
                } else if (strcmp(key, "time_slice_ms") == 0) {
                    g_runtime_config.time_slice_ms = atoi(value);
                } else if (strcmp(key, "max_threads") == 0) {
                    g_runtime_config.max_threads = atoi(value);
                } else if (strcmp(key, "heap_size_mb") == 0) {
                    g_runtime_config.heap_size_mb = atoi(value);
                } else if (strcmp(key, "stack_size_kb") == 0) {
                    g_runtime_config.stack_size_kb = atoi(value);
                } else if (strcmp(key, "enable_debug") == 0) {
                    g_runtime_config.enable_debug = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_trace") == 0) {
                    g_runtime_config.enable_trace = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_verbose") == 0) {
                    g_runtime_config.enable_verbose = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_kaslr") == 0) {
                    g_runtime_config.enable_kaslr = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_smap") == 0) {
                    g_runtime_config.enable_smap = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_smep") == 0) {
                    g_runtime_config.enable_smep = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_audit") == 0) {
                    g_runtime_config.enable_audit = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_pci") == 0) {
                    g_runtime_config.enable_pci = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "enable_acpi") == 0) {
                    g_runtime_config.enable_acpi = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                } else if (strcmp(key, "serial_baud") == 0) {
                    g_runtime_config.serial_baud = atoi(value);
                } else if (strcmp(key, "max_capabilities") == 0) {
                    g_runtime_config.max_capabilities = atoi(value);
                } else if (strcmp(key, "max_domains") == 0) {
                    g_runtime_config.max_domains = atoi(value);
                } else if (strcmp(key, "max_irqs") == 0) {
                    g_runtime_config.max_irqs = atoi(value);
                }
            }
        }
        
        /* 移动到下一个参数 */
        while (*token != '\0' && *token != ' ') token++;
    }
}

/* 从platform.yaml加载配置 */
void runtime_config_load_from_yaml(const char* yaml_data, size_t yaml_size)
{
    if (!yaml_data || yaml_size == 0) {
        return;
    }
    
    console_puts("[CONFIG] Loading configuration from YAML...\n");
    
    /* 解析YAML中的配置项 */
    /* 使用YAML解析器加载配置 */
    yaml_parser_t* parser = yaml_parser_create(yaml_data, yaml_size);
    if (!parser) {
        console_puts("[CONFIG] ERROR: Failed to create YAML parser\n");
        return;
    }
    
    int parse_result = yaml_parse(parser);
    if (parse_result != 0) {
        console_puts("[CONFIG] ERROR: Failed to parse YAML\n");
        yaml_parser_destroy(parser);
        return;
    }
    
    yaml_node_t* root = parser->root;
    if (!root) {
        console_puts("[CONFIG] ERROR: No YAML root node\n");
        yaml_parser_destroy(parser);
        return;
    }
    
    /* 查找系统配置 */
    yaml_node_t* system_node = root->children;
    while (system_node) {
        if (system_node->type == YAML_TYPE_MAPPING && 
            system_node->key && strcmp(system_node->key, "system") == 0) {
            break;
        }
        system_node = system_node->next;
    }
    
    if (system_node) {
        /* 查找日志级别 */
        yaml_node_t* log_level_node = system_node->children;
        while (log_level_node) {
            if (log_level_node->type == YAML_TYPE_SCALAR && 
                log_level_node->key && strcmp(log_level_node->key, "log_level") == 0) {
                const char* log_level_str = log_level_node->value;
                if (strcmp(log_level_str, "error") == 0) {
                    g_runtime_config.log_level = LOG_LEVEL_ERROR;
                } else if (strcmp(log_level_str, "warn") == 0) {
                    g_runtime_config.log_level = LOG_LEVEL_WARN;
                } else if (strcmp(log_level_str, "info") == 0) {
                    g_runtime_config.log_level = LOG_LEVEL_INFO;
                } else if (strcmp(log_level_str, "debug") == 0) {
                    g_runtime_config.log_level = LOG_LEVEL_DEBUG;
                } else if (strcmp(log_level_str, "trace") == 0) {
                    g_runtime_config.log_level = LOG_LEVEL_TRACE;
                }
                break;
            }
            log_level_node = log_level_node->next;
        }
        
        /* 查找调度策略 */
        yaml_node_t* scheduler_policy_node = system_node->children;
        while (scheduler_policy_node) {
            if (scheduler_policy_node->type == YAML_TYPE_SCALAR && 
                scheduler_policy_node->key && strcmp(scheduler_policy_node->key, "scheduler_policy") == 0) {
                const char* policy_str = scheduler_policy_node->value;
                if (strcmp(policy_str, "fifo") == 0) {
                    g_runtime_config.scheduler_policy = SCHEDULER_POLICY_FIFO;
                } else if (strcmp(policy_str, "rr") == 0) {
                    g_runtime_config.scheduler_policy = SCHEDULER_POLICY_RR;
                } else if (strcmp(policy_str, "priority") == 0) {
                    g_runtime_config.scheduler_policy = SCHEDULER_POLICY_PRIORITY;
                }
                break;
            }
            scheduler_policy_node = scheduler_policy_node->next;
        }
    }
    
    /* 查找调试配置 */
    yaml_node_t* debug_node = root->children;
    while (debug_node) {
        if (debug_node->type == YAML_TYPE_MAPPING && 
            debug_node->key && strcmp(debug_node->key, "debug") == 0) {
            break;
        }
        debug_node = debug_node->next;
    }
    
    if (debug_node) {
        /* 查找enable_debug */
        yaml_node_t* enable_debug_node = debug_node->children;
        while (enable_debug_node) {
            if (enable_debug_node->type == YAML_TYPE_SCALAR && 
                enable_debug_node->key && strcmp(enable_debug_node->key, "enable_debug") == 0) {
                g_runtime_config.enable_debug = strcmp(enable_debug_node->value, "true") == 0;
                break;
            }
            enable_debug_node = enable_debug_node->next;
        }
        
        /* 查找enable_trace */
        yaml_node_t* enable_trace_node = debug_node->children;
        while (enable_trace_node) {
            if (enable_trace_node->type == YAML_TYPE_SCALAR && 
                enable_trace_node->key && strcmp(enable_trace_node->key, "enable_trace") == 0) {
                g_runtime_config.enable_trace = strcmp(enable_trace_node->value, "true") == 0;
                break;
            }
            enable_trace_node = enable_trace_node->next;
        }
    }
    
    /* 查找安全配置 */
    yaml_node_t* security_node = root->children;
    while (security_node) {
        if (security_node->type == YAML_TYPE_MAPPING && 
            security_node->key && strcmp(security_node->key, "security") == 0) {
            break;
        }
        security_node = security_node->next;
    }
    
    if (security_node) {
        /* 查找enable_kaslr */
        yaml_node_t* enable_kaslr_node = security_node->children;
        while (enable_kaslr_node) {
            if (enable_kaslr_node->type == YAML_TYPE_SCALAR && 
                enable_kaslr_node->key && strcmp(enable_kaslr_node->key, "enable_kaslr") == 0) {
                g_runtime_config.enable_kaslr = strcmp(enable_kaslr_node->value, "true") == 0;
                break;
            }
            enable_kaslr_node = enable_kaslr_node->next;
        }
        
        /* 查找enable_audit */
        yaml_node_t* enable_audit_node = security_node->children;
        while (enable_audit_node) {
            if (enable_audit_node->type == YAML_TYPE_SCALAR && 
                enable_audit_node->key && strcmp(enable_audit_node->key, "enable_audit") == 0) {
                g_runtime_config.enable_audit = strcmp(enable_audit_node->value, "true") == 0;
                break;
            }
            enable_audit_node = enable_audit_node->next;
        }
    }
    
    yaml_parser_destroy(parser);
    
    g_runtime_config.config_source = CONFIG_SOURCE_PLATFORM_YAML;
}

/* 获取配置项值 */
config_item_t* runtime_config_get_item(const char* name)
{
    /* 首先检查自定义配置项 */
    for (u32 i = 0; i < g_runtime_config.num_custom_items; i++) {
        if (strcmp(g_runtime_config.custom_items[i].name, name) == 0) {
            return &g_runtime_config.custom_items[i];
        }
    }
    
    /* 检查内置配置项 */
    if (strcmp(name, "log_level") == 0) {
        static config_item_t item;
        strcopy_internal(item.name, "log_level", sizeof(item.name));
        item.type = CONFIG_TYPE_ENUM;
        item.value.enum_val = g_runtime_config.log_level;
        item.description = "日志级别 (0=error, 1=warn, 2=info, 3=debug, 4=trace)";
        return &item;
    }
    
    return NULL;
}

/* 设置配置项值 */
hic_status_t runtime_config_set_item(const char* name, const void* value, config_type_t type)
{
    if (!name || !value) {
        return HIC_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否为只读配置 */
    config_item_t* item = runtime_config_get_item(name);
    if (item && (item->flags & CONFIG_FLAG_READONLY)) {
        console_puts("[CONFIG] ERROR: Configuration item '");
        console_puts(name);
        console_puts("' is read-only\n");
        return HIC_ERROR_PERMISSION_DENIED;
    }
    
    /* 设置内置配置项 */
    if (strcmp(name, "log_level") == 0) {
        if (type == CONFIG_TYPE_ENUM) {
            log_level_t old_val = g_runtime_config.log_level;
            g_runtime_config.log_level = *(log_level_t*)value;
            
            /* 触发回调 */
            for (u32 i = 0; i < g_num_callbacks; i++) {
                if (strcmp(g_config_callbacks[i].name, name) == 0) {
                    g_config_callbacks[i].callback(name, &old_val, value);
                }
            }
            
            return HIC_SUCCESS;
        }
    } else if (strcmp(name, "enable_debug") == 0) {
        if (type == CONFIG_TYPE_BOOL) {
            bool old_val = g_runtime_config.enable_debug;
            g_runtime_config.enable_debug = *(bool*)value;
            
            /* 触发回调 */
            for (u32 i = 0; i < g_num_callbacks; i++) {
                if (strcmp(g_config_callbacks[i].name, name) == 0) {
                    g_config_callbacks[i].callback(name, &old_val, value);
                }
            }
            
            return HIC_SUCCESS;
        }
    }
    
    return HIC_ERROR_NOT_FOUND;
}

/* 重置配置项为默认值 */
void runtime_config_reset_item(const char* name)
{
    if (!name) {
        return;
    }
    
    /* 重置为默认值 */
    if (strcmp(name, "log_level") == 0) {
        g_runtime_config.log_level = LOG_LEVEL_INFO;
    } else if (strcmp(name, "enable_debug") == 0) {
        g_runtime_config.enable_debug = false;
    }
}

/* 打印当前配置 */
void runtime_config_print(void)
{
    console_puts("\n=== Runtime Configuration ===\n");
    
    console_puts("System:\n");
    console_puts("  log_level: ");
    console_puti32(g_runtime_config.log_level);
    console_puts("\n");
    
    console_puts("  scheduler_policy: ");
    console_puti32(g_runtime_config.scheduler_policy);
    console_puts("\n");
    
    console_puts("  memory_policy: ");
    console_puti32(g_runtime_config.memory_policy);
    console_puts("\n");
    
    console_puts("  security_level: ");
    console_puti32(g_runtime_config.security_level);
    console_puts("\n");
    
    console_puts("Security:\n");
    console_puts("  enable_secure_boot: ");
    console_puts(g_runtime_config.enable_secure_boot ? "true" : "false");
    console_puts("\n");
    
    console_puts("  enable_kaslr: ");
    console_puts(g_runtime_config.enable_kaslr ? "true" : "false");
    console_puts("\n");
    
    console_puts("  enable_audit: ");
    console_puts(g_runtime_config.enable_audit ? "true" : "false");
    console_puts("\n");
    
    console_puts("Debug:\n");
    console_puts("  enable_debug: ");
    console_puts(g_runtime_config.enable_debug ? "true" : "false");
    console_puts("\n");
    
    console_puts("  enable_trace: ");
    console_puts(g_runtime_config.enable_trace ? "true" : "false");
    console_puts("\n");
    
    console_puts("  debug_port: 0x");
    console_puthex32(g_runtime_config.debug_port);
    console_puts("\n");
    
    console_puts("==============================\n\n");
}

/* 验证配置一致性 */
bool runtime_config_validate(void)
{
    /* 验证配置值在合理范围内 */
    if (g_runtime_config.max_threads == 0 || g_runtime_config.max_threads > 1024) {
        console_puts("[CONFIG] ERROR: Invalid max_threads value\n");
        return false;
    }
    
    if (g_runtime_config.heap_size_mb < 16 || g_runtime_config.heap_size_mb > 4096) {
        console_puts("[CONFIG] ERROR: Invalid heap_size_mb value\n");
        return false;
    }
    
    if (g_runtime_config.time_slice_ms < 1 || g_runtime_config.time_slice_ms > 1000) {
        console_puts("[CONFIG] ERROR: Invalid time_slice_ms value\n");
        return false;
    }
    
    return true;
}

/* 导出配置为YAML */
void runtime_config_export_yaml(char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return;
    }
    
    /* 生成完整YAML格式 */
    size_t offset = 0;
    
    /* 添加配置头 */
    const char* header = "runtime_config:\n";
    size_t header_len = strlen(header);
    if (offset + header_len < buffer_size) {
        strcopy_internal(buffer + offset, header, buffer_size - offset);
        offset += header_len;
    }
    
    /* 添加系统配置 */
    const char* system = "  system:\n";
    size_t system_len = strlen(system);
    if (offset + system_len < buffer_size) {
        strcopy_internal(buffer + offset, system, buffer_size - offset);
        offset += system_len;
    }
    
    /* 添加日志级别 */
    const char* log_level_key = "    log_level: ";
    size_t log_level_key_len = strlen(log_level_key);
    if (offset + log_level_key_len < buffer_size) {
        strcopy_internal(buffer + offset, log_level_key, buffer_size - offset);
        offset += log_level_key_len;
    }
    
    const char* log_level_values[] = {"error", "warn", "info", "debug", "trace"};
    const char* log_level_value = log_level_values[g_runtime_config.log_level];
    size_t log_level_value_len = strlen(log_level_value);
    if (offset + log_level_value_len < buffer_size) {
        strcopy_internal(buffer + offset, log_level_value, buffer_size - offset);
        offset += log_level_value_len;
    }
    
    if (offset + 1 < buffer_size) {
        buffer[offset++] = '\n';
    }
    
    /* 添加调试配置 */
    const char* debug = "  debug:\n";
    size_t debug_len = strlen(debug);
    if (offset + debug_len < buffer_size) {
        strcopy_internal(buffer + offset, debug, buffer_size - offset);
        offset += debug_len;
    }
    
    /* 添加enable_debug */
    const char* enable_debug_key = "    enable_debug: ";
    size_t enable_debug_key_len = strlen(enable_debug_key);
    if (offset + enable_debug_key_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_debug_key, buffer_size - offset);
        offset += enable_debug_key_len;
    }
    
    const char* enable_debug_value = g_runtime_config.enable_debug ? "true" : "false";
    size_t enable_debug_value_len = strlen(enable_debug_value);
    if (offset + enable_debug_value_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_debug_value, buffer_size - offset);
        offset += enable_debug_value_len;
    }
    
    if (offset + 1 < buffer_size) {
        buffer[offset++] = '\n';
    }
    
    /* 添加enable_trace */
    const char* enable_trace_key = "    enable_trace: ";
    size_t enable_trace_key_len = strlen(enable_trace_key);
    if (offset + enable_trace_key_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_trace_key, buffer_size - offset);
        offset += enable_trace_key_len;
    }
    
    const char* enable_trace_value = g_runtime_config.enable_trace ? "true" : "false";
    size_t enable_trace_value_len = strlen(enable_trace_value);
    if (offset + enable_trace_value_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_trace_value, buffer_size - offset);
        offset += enable_trace_value_len;
    }
    
    if (offset + 1 < buffer_size) {
        buffer[offset++] = '\n';
    }
    
    /* 添加安全配置 */
    const char* security = "  security:\n";
    size_t security_len = strlen(security);
    if (offset + security_len < buffer_size) {
        strcopy_internal(buffer + offset, security, buffer_size - offset);
        offset += security_len;
    }
    
    /* 添加enable_kaslr */
    const char* enable_kaslr_key = "    enable_kaslr: ";
    size_t enable_kaslr_key_len = strlen(enable_kaslr_key);
    if (offset + enable_kaslr_key_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_kaslr_key, buffer_size - offset);
        offset += enable_kaslr_key_len;
    }
    
    const char* enable_kaslr_value = g_runtime_config.enable_kaslr ? "true" : "false";
    size_t enable_kaslr_value_len = strlen(enable_kaslr_value);
    if (offset + enable_kaslr_value_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_kaslr_value, buffer_size - offset);
        offset += enable_kaslr_value_len;
    }
    
    if (offset + 1 < buffer_size) {
        buffer[offset++] = '\n';
    }
    
    /* 添加enable_audit */
    const char* enable_audit_key = "    enable_audit: ";
    size_t enable_audit_key_len = strlen(enable_audit_key);
    if (offset + enable_audit_key_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_audit_key, buffer_size - offset);
        offset += enable_audit_key_len;
    }
    
    const char* enable_audit_value = g_runtime_config.enable_audit ? "true" : "false";
    size_t enable_audit_value_len = strlen(enable_audit_value);
    if (offset + enable_audit_value_len < buffer_size) {
        strcopy_internal(buffer + offset, enable_audit_value, buffer_size - offset);
        offset += enable_audit_value_len;
    }
    
    if (offset + 1 < buffer_size) {
        buffer[offset++] = '\n';
    }
    
    /* 结束字符串 */
    if (offset < buffer_size) {
        buffer[offset] = '\0';
    }
}

/* 注册配置变更回调 */
void runtime_config_register_callback(const char* name, config_change_callback_t callback)
{
    if (!name || !callback) {
        return;
    }
    
    if (g_num_callbacks >= 32) {
        console_puts("[CONFIG] WARNING: Callback table full\n");
        return;
    }
    
    strcopy_internal(g_config_callbacks[g_num_callbacks].name, name, sizeof(g_config_callbacks[g_num_callbacks].name));
    g_config_callbacks[g_num_callbacks].callback = callback;
    g_num_callbacks++;
    
    console_puts("[CONFIG] Registered callback for '");
    console_puts(name);
    console_puts("'\n");
}

/* 检查配置是否可以运行时修改 */
bool runtime_config_is_runtime_modifiable(const char* name)
{
    config_item_t* item = runtime_config_get_item(name);
    if (!item) {
        return false;
    }
    
    return (item->flags & CONFIG_FLAG_RUNTIME) != 0;
}
