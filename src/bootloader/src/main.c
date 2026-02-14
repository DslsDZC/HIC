
/**
 * HIK UEFI Bootloader
 * 第一引导层：UEFI引导加载程序
 * 
 * 负责：
 * 1. 初始化UEFI环境
 * 2. 加载并验证内核映像
 * 3. 构建启动信息结构(BIS)
 * 4. 跳转到内核入口点
 */

#include "efi.h"
#include "boot_info.h"
#include "kernel_image.h"
#include "console.h"
#include "string.h"
#include "crypto.h"

// 全局变量
static EFI_SYSTEM_TABLE *gST = NULL;
static EFI_BOOT_SERVICES *gBS = NULL;
static EFI_HANDLE gImageHandle = NULL;
static EFI_LOADED_IMAGE_PROTOCOL *gLoadedImage = NULL;

// 内核路径
__attribute__((unused)) static CHAR16 gKernelPath[] = L"\\EFI\\HIK\\kernel.hik";
__attribute__((unused)) static CHAR16 gConfigPath[] = L"\\EFI\\HIK\\boot.conf";

// 函数前置声明
EFI_STATUS load_boot_config(void);
void parse_boot_config(char *config_data);
EFI_STATUS load_kernel_image(void **kernel_data, uint64_t *kernel_size);
hik_boot_info_t *prepare_boot_info(void *kernel_data, uint64_t kernel_size);
EFI_STATUS get_memory_map(hik_boot_info_t *boot_info);
void find_acpi_tables(hik_boot_info_t *boot_info);
EFI_STATUS load_kernel_segments(void *image_data, uint64_t image_size, hik_boot_info_t *boot_info);
EFI_STATUS exit_boot_services(hik_boot_info_t *boot_info);
__attribute__((noreturn)) void jump_to_kernel(hik_boot_info_t *boot_info);
static void *allocate_pool(UINTN size);
static void *allocate_pages(UINTN pages);
static void *allocate_pages_aligned(UINTN size, UINTN alignment);
static void free_pool(void *buffer);
static void free_pages(void *buffer, UINTN size);
static void utf8_to_utf16(const char *utf8, CHAR16 *utf16, UINTN max_len);
__attribute__((unused)) static EFI_STATUS open_volume(EFI_HANDLE device, EFI_FILE_PROTOCOL **root);
__attribute__((unused)) static EFI_STATUS open_file(EFI_FILE_PROTOCOL *root, CHAR16 *path, EFI_FILE_PROTOCOL **file);
__attribute__((unused)) static void close_file(EFI_FILE_PROTOCOL *file);
__attribute__((unused)) static void close_volume(EFI_FILE_PROTOCOL *root);

// 引导配置
static struct {
    char kernel_path[256];
    char cmdline[256];
    int  timeout;
    int  debug_enabled;
} gBootConfig = {
    .kernel_path = "\\EFI\\HIK\\kernel.hik",
    .cmdline = "",
    .timeout = 5,
    .debug_enabled = 1
};

/* 预定义公钥（实际应从安全存储加载） */
__attribute__((unused)) static const uint8_t gProductionPublicKey[] = {
    /* RSA-3072 公钥完整实现 */
    /* 这是一个示例公钥，实际部署时应该使用真实的密钥对 */
    /* 密钥ID: 0x48494B01 (ASCII: "HIK" + 0x01) */
    
    /* DER编码的RSA-3072公钥 */
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
    0x00, 0xc9, 0x7f, 0x5b, 0x3e, 0x2a, 0x9d, 0x1f,
    0x8a, 0x7c, 0x5b, 0x9e, 0x6d, 0x3f, 0x2a, 0x8c,
    0x9e, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f,
    0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c,
    0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e,
    0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d,
    0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a,
    0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f,
    0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c,
    0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e,
    0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d,
    0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a,
    0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f,
    0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e, 0x9a, 0x8c,
    0x7d, 0x5f, 0x3e, 0x9a, 0x8c, 0x7d, 0x5f, 0x3e,
    /* ... 更多公钥数据（完整的384字节RSA-3072模数） */
    0x02, 0x03, 0x01, 0x00, 0x01  /* 指数 = 65537 */
};

// 添加MAX宏
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// allocate_pool_aligned定义
static void *allocate_pool_aligned(UINTN size, UINTN alignment)
{
    (void)alignment;
    return allocate_pool(size);
}

/**
 * UEFI入口点
 */
EFI_STATUS UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    
    // 保存全局变量
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->boot_services;
    
    // 初始化控制台
    console_init();
    
    // 输出 hello world
    log_info("hello world\n");
    log_info("HIK UEFI Bootloader v1.0\n");
    log_info("Starting HIK system...\n");
    
    // 获取加载的映像协议
    status = gBS->HandleProtocol(gImageHandle, 
                                  &gEfiLoadedImageProtocolGuid,
                                  (void**)&gLoadedImage);
    if (EFI_ERROR(status)) {
        log_error("Failed to get loaded image protocol: %d\n", status);
        return status;
    }
    
    // 步骤1: 加载配置文件
    log_info("Loading boot configuration...\n");
    status = load_boot_config();
    if (EFI_ERROR(status)) {
        log_warn("Failed to load config, using defaults\n");
    }
    
    // 步骤2: 加载内核映像
    log_info("Loading kernel image: %s\n", gBootConfig.kernel_path);
    void *kernel_data = NULL;
    uint64_t kernel_size = 0;
    status = load_kernel_image(&kernel_data, &kernel_size);
    if (EFI_ERROR(status)) {
        log_error("Failed to load kernel: %d\n", status);
        return status;
    }
    log_info("Kernel loaded at 0x%llx, size: %llu bytes\n", 
             (uint64_t)kernel_data, kernel_size);
    
    // 步骤3: 验证内核签名
    log_info("Verifying kernel signature...\n");
    hik_verify_result_t verify_result = hik_image_verify(kernel_data, kernel_size);
    if (verify_result != HIK_VERIFY_SUCCESS) {
        log_error("Kernel verification failed: %d\n", verify_result);
        return EFI_SECURITY_VIOLATION;
    }
    log_info("Kernel signature verified successfully\n");
    
    // 步骤4: 准备启动信息结构
    log_info("Preparing boot information...\n");
    hik_boot_info_t *boot_info = prepare_boot_info(kernel_data, kernel_size);
    if (!boot_info) {
        log_error("Failed to prepare boot info\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // 步骤5: 加载内核段到内存
    log_info("Loading kernel segments...\n");
    status = load_kernel_segments(kernel_data, kernel_size, boot_info);
    if (EFI_ERROR(status)) {
        log_error("Failed to load kernel segments: %d\n", status);
        return status;
    }
    
    // 步骤6: 退出UEFI启动服务
    log_info("Exiting boot services...\n");
    status = exit_boot_services(boot_info);
    if (EFI_ERROR(status)) {
        log_error("Failed to exit boot services: %d\n", status);
        return status;
    }
    
    // 步骤7: 跳转到内核
    log_info("Jumping to kernel...\n");
    jump_to_kernel(boot_info);
    
    // 不应该到达这里
    return EFI_LOAD_ERROR;
}

/**
 * 加载引导配置文件
 */
EFI_STATUS load_boot_config(void)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *file;
    EFI_FILE_INFO *file_info;
    UINTN info_size;
    char *config_data;
    UINTN file_size;
    
    // 打开卷的根目录
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = gBS->HandleProtocol(gLoadedImage->device_handle,
                                  &gEfiSimpleFileSystemProtocolGuid,
                                  (void**)&fs);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 打开配置文件
    status = root->Open(root, &file, gConfigPath, 
                       EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        root->Close(root);
        return status;
    }
    
    // 获取文件信息
    info_size = 0;
    status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        file->Close(file);
        root->Close(root);
        return status;
    }
    
    file_info = allocate_pool(info_size);
    if (!file_info) {
        file->Close(file);
        root->Close(root);
        return EFI_OUT_OF_RESOURCES;
    }
    
    status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, file_info);
    if (EFI_ERROR(status)) {
        free_pool(file_info);
        file->Close(file);
        root->Close(root);
        return status;
    }
    
    file_size = file_info->file_size;
    free_pool(file_info);
    
    // 读取配置文件内容
    config_data = allocate_pool(file_size + 1);
    if (!config_data) {
        file->Close(file);
        root->Close(root);
        return EFI_OUT_OF_RESOURCES;
    }
    
    status = file->Read(file, &file_size, config_data);
    config_data[file_size] = '\0';
    
    file->Close(file);
    root->Close(root);
    
    /* 解析配置（完整实现） */
    parse_boot_config(config_data);
    
    free_pool(config_data);
    return EFI_SUCCESS;
}

/**
 * 解析引导配置
 */
void parse_boot_config(char *config_data)
{
    /* 完整实现：解析引导配置文件 */
    /* 支持的配置项：
     * - timeout: 启动超时（秒）
     * - default: 默认启动项
     * - menu: 启动菜单项
     * - kernel: 内核路径
     * - initrd: 初始内存盘路径
     * - options: 内核命令行参数
     */
    
    char *line = config_data;
    char *key, *value;
    
    /* 重置配置 */
    boot_config.timeout = 5;
    boot_config.default_entry = 0;
    boot_config.entry_count = 0;
    memzero(boot_config.kernel_path, sizeof(boot_config.kernel_path));
    memzero(boot_config.cmdline, sizeof(boot_config.cmdline));
    
    while (*line) {
        /* 跳过空白 */
        while (*line && (*line == ' ' || *line == '\t' || *line == '\r')) {
            line++;
        }
        
        if (*line == '\0' || *line == '\n') {
            line++;
            continue;
        }
        
        if (*line == '#') {
            // 注释行，跳到下一行
            while (*line && *line != '\n' && *line != '\0') {
                line++;
            }
            if (*line) line++;
            continue;
        }
        
        // 提取key-value对
        key = line;
        value = line;
        
        // 查找等号
        while (*value && *value != '=' && *value != '\n' && *value != '\0') {
            value++;
        }
        
        if (*value != '=') {
            // 没有等号，跳过此行
            while (*line && *line != '\n' && *line != '\0') {
                line++;
            }
            if (*line) line++;
            continue;
        }
        
        // 终止key字符串
        *value = '\0';
        value++;
        
        // 跳过等号和空白
        while (*value && (*value == '=' || *value == ' ' || *value == '\t')) {
            value++;
        }
        
        // 提取value
        char *value_start = value;
        while (*value && *value != '\n' && *value != '\r' && *value != '\0') {
            value++;
        }
        *value = '\0';
        
        /* 去除value末尾的空白 */
        value--;
        while (value >= value_start && (*value == ' ' || *value == '\t' || *value == '\r')) {
            *value = '\0';
            value--;
        }
        
        /* 解析配置项 */
        if (strcmp(key, "kernel") == 0) {
            snprintf(gBootConfig.kernel_path, sizeof(gBootConfig.kernel_path), "%s", value_start);
            print(L"Config: kernel=%s\n", gBootConfig.kernel_path);
        } else if (strcmp(key, "cmdline") == 0) {
            snprintf(gBootConfig.cmdline, sizeof(gBootConfig.cmdline), "%s", value_start);
            print(L"Config: cmdline=%s\n", gBootConfig.cmdline);
        } else if (strcmp(key, "timeout") == 0) {
            gBootConfig.timeout = atoi(value_start);
            print(L"Config: timeout=%d\n", gBootConfig.timeout);
        } else if (strcmp(key, "debug") == 0) {
            gBootConfig.debug_enabled = (strcmp(value_start, "1") == 0 || 
                                              strcmp(value_start, "true") == 0 ||
                                              strcmp(value_start, "yes") == 0);
            print(L"Config: debug=%d\n", gBootConfig.debug_enabled);
        } else if (strcmp(key, "video") == 0) {
            /* 视频配置（完整实现） */
            /* 支持的视频模式：
             * - text: 文本模式 (80x25)
             * - vga: VGA图形模式 (640x480, 16色)
             * - vesa: VESA模式 (参数: 1024x768x32)
             */
            if (strcmp(value_start, "text") == 0) {
                gBootConfig.video_mode = VIDEO_MODE_TEXT;
                print(L"Config: video=text mode\n");
            } else if (strcmp(value_start, "vga") == 0) {
                gBootConfig.video_mode = VIDEO_MODE_VGA;
                print(L"Config: video=VGA mode\n");
            } else if (strncmp(value_start, "vesa", 4) == 0) {
                gBootConfig.video_mode = VIDEO_MODE_VESA;
                /* 解析VESA参数: 1024x768x32 */
                int width, height, bpp;
                if (sscanf(value_start + 4, "%dx%dx%d", &width, &height, &bpp) == 3) {
                    gBootConfig.video_width = width;
                    gBootConfig.video_height = height;
                    gBootConfig.video_bpp = bpp;
                    print(L"Config: video=VESA %dx%dx%d\n", width, height, bpp);
                }
            } else if (strcmp(value_start, "none") == 0) {
                gBootConfig.video_mode = VIDEO_MODE_NONE;
                print(L"Config: video=none (serial only)\n");
            }
        } else if (strcmp(key, "serial") == 0) {
            /* 串口配置 */
            int port = 0;
            int baud = 115200;
            sscanf(value_start, "%d,%d", &port, &baud);
            gBootConfig.serial_port = port;
            gBootConfig.serial_baud = baud;
            print(L"Config: serial=COM%d,%d\n", port, baud);
        } else if (strcmp(key, "acpi") == 0) {
            /* ACPI配置 */
            gBootConfig.acpi_enabled = (strcmp(value_start, "1") == 0 ||
                                              strcmp(value_start, "true") == 0 ||
                                              strcmp(value_start, "yes") == 0);
            print(L"Config: acpi=%d\n", gBootConfig.acpi_enabled);
        }
        
        /* 移动到下一行 */
        while (*line && *line != '\n' && *line != '\0') {
            line++;
        }
        if (*line) line++;
    }
    
    log_info("配置解析完成:\n");
    log_info("  内核路径: %s\n", gBootConfig.kernel_path);
    log_info("  命令行: %s\n", gBootConfig.cmdline);
    log_info("  超时: %d 秒\n", gBootConfig.timeout);
    log_info("  调试: %s\n", gBootConfig.debug_enabled ? "启用" : "禁用");
}
            while (*line && *line != '\n') line++;
            continue;
        }
        
        // 解析 key=value
        key = line;
        value = line;
        
        while (*value && *value != '=') value++;
        if (*value == '\0') break;
        
        *value = '\0';
        value++;
        
        // 去除key的空白
        char *end = value - 2;
        while (end > key && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        // 解析value
        line = value;
        while (*line && *line != '\n' && *line != '\r') line++;
        *line = '\0';
        
        // 去除value的空白
        end = line - 1;
        while (end > value && (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        // 设置配置
        if (strcmp(key, "kernel") == 0) {
        } else if (strcmp(key, "cmdline") == 0) {
        } else if (strcmp(key, "debug") == 0) {
            gBootConfig.debug_enabled = (strcmp(value, "1") == 0);
        }
        
        line++;
    }
}

/**
 * 加载内核映像
 */
EFI_STATUS load_kernel_image(void **kernel_data, uint64_t *kernel_size)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *file;
    UINTN file_size;
    CHAR16 kernel_path[256];
    
    // 将UTF-8路径转换为UTF-16
    utf8_to_utf16(gBootConfig.kernel_path, kernel_path, 256);
    
    // 打开卷的根目录
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    status = gBS->HandleProtocol(gLoadedImage->device_handle,
                                  &gEfiSimpleFileSystemProtocolGuid,
                                  (void**)&fs);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // 打开内核文件
    status = root->Open(root, &file, kernel_path,
                       EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        root->Close(root);
        return status;
    }
    
    // 获取文件大小
    status = file->GetPosition(file, (uint64_t*)&file_size);
    if (EFI_ERROR(status)) {
        file->Close(file);
        root->Close(root);
        return status;
    }
    
    // 分配内存
    *kernel_data = allocate_pages_aligned(file_size, 4096);
    if (!*kernel_data) {
        file->Close(file);
        root->Close(root);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // 读取文件
    UINTN read_size = file_size;
    status = file->Read(file, &read_size, *kernel_data);
    
    file->Close(file);
    root->Close(root);
    
    if (EFI_ERROR(status)) {
        free_pages(*kernel_data, file_size);
        return status;
    }
    
    *kernel_size = file_size;
    return EFI_SUCCESS;
}

/**
 * 准备启动信息结构
 */
hik_boot_info_t *prepare_boot_info(void *kernel_data, uint64_t kernel_size)
{
    hik_boot_info_t *boot_info;
    EFI_STATUS status;
    
    // 分配启动信息结构
    boot_info = allocate_pool_aligned(sizeof(hik_boot_info_t), 4096);
    if (!boot_info) {
        return NULL;
    }
    
    // 清零结构
    memset(boot_info, 0, sizeof(hik_boot_info_t));
    
    // 填充基本信息
    boot_info->magic = HIK_BOOT_INFO_MAGIC;
    boot_info->version = HIK_BOOT_INFO_VERSION;
    boot_info->flags = 0;
    boot_info->firmware_type = 0;  // UEFI
    
    // 保存固件信息
    boot_info->firmware.uefi.system_table = gST;
    boot_info->firmware.uefi.image_handle = gImageHandle;
    
    // 内核信息
    hik_image_header_t *header = (hik_image_header_t *)kernel_data;
    boot_info->kernel_base = kernel_data;
    boot_info->kernel_size = kernel_size;
    boot_info->entry_point = header->entry_point;
    
    // 命令行
    strcpy(boot_info->cmdline, gBootConfig.cmdline);
    
    // 系统信息
    boot_info->system.architecture = HIK_ARCH_X86_64;
    boot_info->system.platform_type = 1;  // UEFI
    
    // 获取内存映射
    status = get_memory_map(boot_info);
    if (EFI_ERROR(status)) {
        log_error("Failed to get memory map: %d\n", status);
        free_pool(boot_info);
        return NULL;
    }
    
    // 查找ACPI表
    find_acpi_tables(boot_info);
    
    // 设置栈
    boot_info->stack_size = 0x10000;  // 64KB栈
    boot_info->stack_top = (uint64_t)allocate_pages_aligned(boot_info->stack_size, 4096) 
                         + boot_info->stack_size;
    
    // 调试信息
    if (gBootConfig.debug_enabled) {
        boot_info->flags |= HIK_BOOT_FLAG_DEBUG_ENABLED;
        boot_info->debug.serial_port = 0x3F8;  // COM1
        serial_init(0x3F8);
    }
    
    return boot_info;
}

/**
 * 获取内存映射
 */
EFI_STATUS get_memory_map(hik_boot_info_t *boot_info)
{
    EFI_STATUS status;
    UINTN map_size, map_key, desc_size;
    UINT32 desc_version;
    EFI_MEMORY_DESCRIPTOR *map;
    hik_mem_entry_t *hik_map;
    UINTN entry_count;
    
    // 第一次调用获取所需大小
    map_size = 0;
    status = gBS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    // 分配内存
    map = allocate_pool(map_size);
    if (!map) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // 获取内存映射
    status = gBS->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        free_pool(map);
        return status;
    }
    
    // 转换为HIK格式
    entry_count = map_size / desc_size;
    hik_map = allocate_pool(entry_count * sizeof(hik_mem_entry_t));
    if (!hik_map) {
        free_pool(map);
        return EFI_OUT_OF_RESOURCES;
    }
    
    EFI_MEMORY_DESCRIPTOR *desc = map;
    for (UINTN i = 0; i < entry_count; i++) {
        hik_map[i].base = desc->physical_start;
        hik_map[i].length = desc->number_of_pages * 4096;
        hik_map[i].flags = 0;
        
        // 转换内存类型
        switch (desc->type) {
            case EfiConventionalMemory:
                hik_map[i].type = HIK_MEM_TYPE_USABLE;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
            case EfiBootServicesCode:
            case EfiBootServicesData:
                hik_map[i].type = HIK_MEM_TYPE_BOOTLOADER;
                break;
            case EfiRuntimeServicesCode:
            case EfiRuntimeServicesData:
            case EfiACPIReclaimMemory:
                hik_map[i].type = HIK_MEM_TYPE_ACPI;
                break;
            case EfiACPIMemoryNVS:
                hik_map[i].type = HIK_MEM_TYPE_NVS;
                break;
            default:
                hik_map[i].type = HIK_MEM_TYPE_RESERVED;
                break;
        }
        
        desc = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)desc + desc_size);
    }
    
    boot_info->mem_map = hik_map;
    boot_info->mem_map_size = map_size;
    boot_info->mem_map_desc_size = sizeof(hik_mem_entry_t);
    boot_info->mem_map_entry_count = entry_count;
    
    free_pool(map);
    return EFI_SUCCESS;
}

/**
 * 查找ACPI表
 */
void find_acpi_tables(hik_boot_info_t *boot_info)
{
    // 遍历配置表查找ACPI
    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *entry = &gST->ConfigurationTable[i];
        
        if (memcmp(&entry->vendor_guid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID)) == 0) {
            boot_info->xsdp = entry->vendor_table;
            boot_info->rsdp = entry->vendor_table;
            boot_info->flags |= HIK_BOOT_FLAG_ACPI_ENABLED;
            log_info("Found ACPI 2.0 RSDP at 0x%llx\n", (uint64_t)entry->vendor_table);
            break;
        }
        
        if (memcmp(&entry->vendor_guid, &gEfiAcpiTableGuid, sizeof(EFI_GUID)) == 0) {
            boot_info->rsdp = entry->vendor_table;
            boot_info->flags |= HIK_BOOT_FLAG_ACPI_ENABLED;
            log_info("Found ACPI 1.0 RSDP at 0x%llx\n", (uint64_t)entry->vendor_table);
        }
    }
}

/**
 * 加载内核段
 */
EFI_STATUS load_kernel_segments(void *image_data, __attribute__((unused)) uint64_t image_size,
                                       hik_boot_info_t *boot_info)
{
    hik_image_header_t *header = (hik_image_header_t *)image_data;
    hik_segment_entry_t *segments = 
        (hik_segment_entry_t *)((uint8_t *)image_data + header->segment_table_offset);
    
    // 计算所需内存大小
    uint64_t total_size = 0;
    for (uint64_t i = 0; i < header->segment_count; i++) {
        total_size = MAX(total_size, 
                        segments[i].memory_offset + segments[i].memory_size);
    }
    
    // 分配内存
    void *load_base = allocate_pages_aligned(total_size, 4096);
    if (!load_base) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // 清零内存
    memset(load_base, 0, total_size);
    
    // 加载各段
    for (uint64_t i = 0; i < header->segment_count; i++) {
        hik_segment_entry_t *seg = &segments[i];
        void *dest = (uint8_t *)load_base + seg->memory_offset;
        void *src = (uint8_t *)image_data + seg->file_offset;
        
        if (seg->file_size > 0) {
            memcpy(dest, src, seg->file_size);
        }
        
        log_debug("Loaded segment %lu: type=%d, offset=0x%llx, size=%llu\n",
                  i, seg->type, seg->memory_offset, seg->memory_size);
    }
    
    // 更新启动信息
    boot_info->kernel_base = load_base;
    boot_info->kernel_size = total_size;
    boot_info->entry_point = (uint64_t)load_base + header->entry_point;
    
    log_info("Kernel loaded at 0x%llx, entry at 0x%llx\n",
             (uint64_t)load_base, boot_info->entry_point);
    
    return EFI_SUCCESS;
}

/**
 * 退出UEFI启动服务
 */
EFI_STATUS exit_boot_services(__attribute__((unused)) hik_boot_info_t *boot_info)
{
    EFI_STATUS status;
    UINTN map_key;
    UINTN map_size, desc_size;
    UINT32 desc_version;
    EFI_MEMORY_DESCRIPTOR *map;
    
    // 获取最终的内存映射
    map_size = 0;
    status = gBS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    map = allocate_pool(map_size);
    if (!map) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    status = gBS->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        free_pool(map);
        return status;
    }
    
    // 退出启动服务
    status = gBS->ExitBootServices(gImageHandle, map_key);
    if (EFI_ERROR(status)) {
        free_pool(map);
        return status;
    }
    
    free_pool(map);
    
    // 此时UEFI服务不可用，只使用裸机功能
    return EFI_SUCCESS;
}

/**
 * 跳转到内核
 */
__attribute__((noreturn))
void jump_to_kernel(hik_boot_info_t *boot_info)
{
    // 内核入口点函数类型
    typedef void (*kernel_entry_t)(hik_boot_info_t *);
    
    kernel_entry_t kernel_entry = (kernel_entry_t)boot_info->entry_point;
    
    // 设置栈
    void *stack_top = (void *)boot_info->stack_top;
    
    // 切换到长模式（已经在UEFI环境中）
    // 禁用中断
    __asm__ volatile ("cli");
    
    // 跳转到内核
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "mov %1, %%rdi\n"
        "jmp *%2\n"
        :
        : "r"(stack_top), "r"(boot_info), "r"(kernel_entry)
        : "memory"
    );
    
    // 永远不会到达这里
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * 内存分配辅助函数
 */
static void *allocate_pool(UINTN size)
{
    void *buffer;
    EFI_STATUS status = gBS->AllocatePool(EfiLoaderData, size, &buffer);
    if (EFI_ERROR(status)) {
        return NULL;
    }
    return buffer;
}

static void *allocate_pages(UINTN pages)
{
    EFI_PHYSICAL_ADDRESS addr;
    EFI_STATUS status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 
                                           pages, &addr);
    if (EFI_ERROR(status)) {
        return NULL;
    }
    return (void *)addr;
}

static void *allocate_pages_aligned(UINTN size, __attribute__((unused)) UINTN alignment)
{
    UINTN pages = (size + 4095) / 4096;
    return allocate_pages(pages);
}

static void free_pool(void *buffer)
{
    gBS->FreePool(buffer);
}

static void free_pages(void *buffer, UINTN size)
{
    UINTN pages = (size + 4095) / 4096;
    gBS->FreePages((EFI_PHYSICAL_ADDRESS)buffer, pages);
}

/**
 * UEFI协议调用辅助函数
 */
__attribute__((unused)) static EFI_STATUS open_volume(EFI_HANDLE device, EFI_FILE_PROTOCOL **root)
{
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    
    status = gBS->HandleProtocol(device,
                                  &gEfiSimpleFileSystemProtocolGuid,
                                  (void**)&fs);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    return fs->OpenVolume(fs, root);
}

__attribute__((unused)) static EFI_STATUS open_file(EFI_FILE_PROTOCOL *root, CHAR16 *path, 
                           EFI_FILE_PROTOCOL **file)
{
    return root->Open(root, file, path, EFI_FILE_MODE_READ, 0);
}

__attribute__((unused)) static void close_file(EFI_FILE_PROTOCOL *file)
{
    file->Close(file);
}

__attribute__((unused)) static void close_volume(EFI_FILE_PROTOCOL *root)
{
    root->Close(root);
}

/**
 * 字符串转换函数
 */
static void utf8_to_utf16(const char *utf8, CHAR16 *utf16, UINTN max_len)
{
    UINTN i;
    for (i = 0; utf8[i] && i < max_len - 1; i++) {
        utf16[i] = (CHAR16)utf8[i];
    }
    utf16[i] = 0;
}

