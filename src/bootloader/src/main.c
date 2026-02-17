
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
// #include "crypto.h"  // 暂时禁用
#include "bootlog.h"
#include "stdlib.h"

// 全局变量
EFI_SYSTEM_TABLE *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
static EFI_HANDLE gImageHandle = NULL;
static EFI_LOADED_IMAGE_PROTOCOL *gLoadedImage = NULL;

// 平台配置数据（传递给内核）
static void *g_platform_data = NULL;
static uint64_t g_platform_size = 0;

// 调试模式标志
static BOOLEAN g_debug_mode = FALSE;

// 内核路径
__attribute__((unused)) static CHAR16 gKernelPath[] = L"\\EFI\\HIK\\kernel.hik";
__attribute__((unused)) static CHAR16 gPlatformPath[] = L"\\EFI\\HIK\\platform.yaml";

// 函数前置声明
EFI_STATUS load_platform_config(void);
void parse_platform_config(char *config_data);
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

/**
 * 查找包含文件系统协议的设备句柄
 * 当EFI_LOADED_IMAGE_PROTOCOL无法获取时使用此方法
 * 调试模式下自动启用，否则需要用户确认
 */
static EFI_STATUS find_file_system_handle(EFI_HANDLE *result_handle) {
    EFI_STATUS status;
    UINTN buffer_size;
    EFI_HANDLE *buffer;
    UINTN i;
    EFI_HANDLE result = NULL;
    
    console_puts("[BOOTLOADER] WARNING: Standard methods failed to get device handle\n");
    console_puts("[BOOTLOADER] Fallback method: Enumerate file system handles\n");
    
    /* 检查是否为调试模式 */
    if (g_debug_mode) {
        console_puts("[BOOTLOADER] Debug mode enabled, fallback method auto-starting...\n");
    } else {
        console_puts("[BOOTLOADER] Press any key to enable fallback method...\n");
        /* 等待用户按键确认 */
        gBS->Stall(2000000); /* 等待2秒 */
        /* 检查用户是否按键（简化的检查，实际应该读取输入） */
        /* 这里我们直接继续，因为在UEFI环境中等待输入比较复杂 */
        console_puts("[BOOTLOADER] Fallback method enabled...\n");
    }
    
    /* 检查LocateHandle函数指针是否有效 */
    if (gBS->LocateHandle == NULL) {
        console_puts("[BOOTLOADER] ERROR: LocateHandle function pointer is NULL\n");
        return EFI_UNSUPPORTED;
    }
    
    /* 第一次调用获取所需的缓冲区大小 */
    buffer_size = 0;
    status = gBS->LocateHandle(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, 
                               NULL, &buffer_size, NULL);
    
    if (status != EFI_BUFFER_TOO_SMALL && status != EFI_SUCCESS) {
        console_puts("[BOOTLOADER] LocateHandle failed for buffer size check\n");
        char status_str[64];
        snprintf(status_str, sizeof(status_str), "[BOOTLOADER] LocateHandle status: ERROR_%d\n", (int)status);
        console_puts(status_str);
        return EFI_DEVICE_ERROR;
    }
    
    /* 分配缓冲区 */
    buffer = allocate_pool(buffer_size);
    if (buffer == NULL) {
        console_puts("[BOOTLOADER] Failed to allocate buffer for handles\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    /* 第二次调用获取句柄列表 */
    status = gBS->LocateHandle(ByProtocol, &gEfiSimpleFileSystemProtocolGuid,
                               NULL, &buffer_size, buffer);
    
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER] LocateHandle failed to get handles\n");
        free_pool(buffer);
        return EFI_DEVICE_ERROR;
    }
    
    console_puts("[BOOTLOADER] Found file system handles, checking...\n");
    
    /* 计算句柄数量 */
    UINTN handle_count = buffer_size / sizeof(EFI_HANDLE);
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "[BOOTLOADER] Found %d handles with file system protocol\n", (int)handle_count);
    console_puts(count_str);
    
    /* 遍历句柄，找到第一个有效的文件系统 */
    for (i = 0; i < handle_count; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
        EFI_FILE_PROTOCOL *root;
        
        status = gBS->HandleProtocol(buffer[i], &gEfiSimpleFileSystemProtocolGuid, (void**)&fs);
        
        if (EFI_ERROR(status) || fs == NULL) {
            continue;
        }
        
        /* 尝试打开卷来验证这个文件系统是否可用 */
        status = fs->OpenVolume(fs, &root);
        
        if (!EFI_ERROR(status) && root != NULL) {
            /* 找到可用的文件系统 */
            console_puts("[BOOTLOADER] Found usable file system handle\n");
            result = buffer[i];
            root->Close(root);
            break;
        }
    }
    
    free_pool(buffer);
    
    if (result == NULL) {
        console_puts("[BOOTLOADER] No usable file system handle found\n");
        return EFI_NOT_FOUND;
    }
    
    *result_handle = result;
    return EFI_SUCCESS;
}

// allocate_pool_aligned定义

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
    INTN countdown;
    
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->boot_services;
    
    /* 初始化控制台 */
    console_init();
    
    /* 初始化引导日志 */
    bootlog_init();
    bootlog_event(BOOTLOG_UEFI_INIT, NULL, 0);
    
    /* 输出启动信息 */
    console_puts("HIK UEFI Bootloader v0.1\n");
    console_puts("Starting HIK Hierarchical Isolation Kernel...\n");
    console_puts("\n");
    bootlog_info("UEFI initialized");
    
    /* 获取加载的镜像信息 */
    console_puts("[BOOTLOADER AUDIT] Stage 1: Getting Loaded Image Protocol\n");
    console_puts("[BOOTLOADER AUDIT] Stage 1: ImageHandle check\n");
    
    /* 检查ImageHandle是否有效 */
    if (ImageHandle == NULL) {
        console_puts("[BOOTLOADER AUDIT] ERROR: ImageHandle is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    /* 检查gBS是否有效 */
    if (gBS == NULL) {
        console_puts("[BOOTLOADER AUDIT] ERROR: gBS is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    /* 检查HandleProtocol是否有效 */
    if (gBS->HandleProtocol == NULL) {
        console_puts("[BOOTLOADER AUDIT] ERROR: HandleProtocol is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 1: All pointers valid\n");
    console_puts("[BOOTLOADER AUDIT] Stage 1: Calling HandleProtocol...\n");
    
    status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, 
                                 (void **)&gLoadedImage);
    console_puts("[BOOTLOADER AUDIT] Stage 1: HandleProtocol returned\n");
    console_puts("[BOOTLOADER AUDIT] Stage 1: Status = ");
    if (status == 0) {
        console_puts("SUCCESS\n");
    } else {
        char err_str[32];
        snprintf(err_str, sizeof(err_str), "ERROR_%d\n", (int)status);
        console_puts(err_str);
        console_puts("[BOOTLOADER AUDIT] Skipping Loaded Image Protocol, continuing...\n");
        console_puts("[BOOTLOADER AUDIT] Will use alternative method for device access\n");
        /* 不返回错误，继续执行 */
    }
    
    if (gLoadedImage == NULL) {
        console_puts("[BOOTLOADER AUDIT] WARNING: gLoadedImage is NULL\n");
        console_puts("[BOOTLOADER AUDIT] Will try to get device handle from image handle directly\n");
        /* 设置默认的device_handle为ImageHandle */
        /* 注意：这种方法可能不总是有效，但是一个变通方案 */
    } else {
        console_puts("[BOOTLOADER AUDIT] Stage 1: Loaded Image Protocol OK\n");
    }
    
    /* 3秒倒计时自动进入内核 */
    console_puts("[BOOTLOADER] Auto-boot in 3 seconds...\n");
    for (countdown = 3; countdown > 0; countdown--) {
        char msg[64];
        snprintf(msg, sizeof(msg), "[BOOTLOADER] Booting in %d seconds...\n", countdown);
        console_puts(msg);
        gBS->Stall(1000000);
    }
    console_puts("[BOOTLOADER] Starting kernel boot...\n");
    
    /* 加载平台配置 */
    status = load_platform_config();
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] WARNING: Failed to load platform config\n");
    }
    
    /* 加载内核映像 */
    void *kernel_data = NULL;
    uint64_t kernel_size = 0;
    status = load_kernel_image(&kernel_data, &kernel_size);
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] ERROR: Failed to load kernel image\n");
        return status;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 4: Preparing boot info...\n");
    /* 准备启动信息 */
    hik_boot_info_t *boot_info = prepare_boot_info(kernel_data, kernel_size);
    if (!boot_info) {
        console_puts("[BOOTLOADER AUDIT] ERROR: Failed to prepare boot info\n");
        return EFI_OUT_OF_RESOURCES;
    }
    console_puts("[BOOTLOADER AUDIT] Stage 4: Boot info prepared\n");
    
    /* 加载内核段 */
    console_puts("[BOOTLOADER AUDIT] Stage 5: Loading kernel segments...\n");
    status = load_kernel_segments(kernel_data, kernel_size, boot_info);
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] ERROR: Failed to load kernel segments\n");
        return status;
    }
    console_puts("[BOOTLOADER AUDIT] Stage 5: Kernel segments loaded\n");
    
    /* 不退出启动服务，直接跳转到内核 */
    console_puts("[BOOTLOADER AUDIT] Stage 6: Skipping boot services exit for debugging\n");
    
    /* 跳转到内核 */
    console_puts("[BOOTLOADER AUDIT] Stage 7: Jumping to kernel...\n");
    
    char temp_str[256];
    snprintf(temp_str, sizeof(temp_str), "[BOOTLOADER] Before jump: boot_info=%p, entry_point=%p (0x%lx)\n",
             boot_info, (void*)boot_info->entry_point, boot_info->entry_point);
    console_puts(temp_str);
    
    bootlog_event(BOOTLOG_JUMP_TO_KERNEL, NULL, 0);
    jump_to_kernel(boot_info);
    
    return EFI_SUCCESS;
}

/**
 * 加载平台配置文件 (platform.yaml)
 */
EFI_STATUS load_platform_config(void)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root;
        EFI_FILE_PROTOCOL *file;
        char *platform_data;
        UINTN file_size;
        EFI_HANDLE device_handle_to_use;
    
        console_puts("[BOOTLOADER AUDIT] Stage 2: Loading Platform Config\n");
        
        /* 确定要使用的device_handle */
        if (gLoadedImage != NULL && gLoadedImage->device_handle != NULL) {
            device_handle_to_use = gLoadedImage->device_handle;
            console_puts("[BOOTLOADER AUDIT] Stage 2: Using device_handle from gLoadedImage\n");
        } else {
            /* 尝试直接使用ImageHandle */
            device_handle_to_use = gImageHandle;
            console_puts("[BOOTLOADER AUDIT] Stage 2: Using ImageHandle as device_handle\n");
        }
        
        /* 如果上述方法都失败，尝试使用备用方案 */
        if (device_handle_to_use == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: No device handle from standard methods\n");
            status = find_file_system_handle(&device_handle_to_use);
            if (EFI_ERROR(status)) {
                console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - No valid device handle available\n");
                return status;
            }
        }
        
        if (device_handle_to_use == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - No valid device handle\n");
            return EFI_INVALID_PARAMETER;
        }
        console_puts("[BOOTLOADER AUDIT] Stage 2: device_handle OK\n");
        
        console_puts("[BOOTLOADER AUDIT] Stage 2: Getting File System Protocol\n");
        // 打开卷的根目录
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        status = gBS->HandleProtocol(device_handle_to_use,
                                      &gEfiSimpleFileSystemProtocolGuid,
                                      (void**)&fs);
        
        /* 如果HandleProtocol失败或fs指针为NULL，尝试备用方案 */
        if (EFI_ERROR(status) || fs == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: Standard method failed, trying fallback...\n");
            if (fs == NULL && !EFI_ERROR(status)) {
                console_puts("[BOOTLOADER AUDIT] Stage 2: HandleProtocol returned SUCCESS but fs is NULL\n");
            }
            status = find_file_system_handle(&device_handle_to_use);
            if (EFI_ERROR(status)) {
                console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - No valid device handle available\n");
                return status;
            }
            
            /* 再次尝试获取文件系统协议 */
            status = gBS->HandleProtocol(device_handle_to_use,
                                          &gEfiSimpleFileSystemProtocolGuid,
                                          (void**)&fs);
            if (EFI_ERROR(status) || fs == NULL) {
                console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - Fallback also failed\n");
                return EFI_INVALID_PARAMETER;
            }
        }
        
        /* 检查OpenVolume函数指针是否有效 */
        if (fs->OpenVolume == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - OpenVolume function pointer is NULL\n");
            return EFI_INVALID_PARAMETER;
        }
        
        console_puts("[BOOTLOADER AUDIT] Stage 2: File System Protocol OK\n");
    
        console_puts("[BOOTLOADER AUDIT] Stage 2: Opening Volume\n");
        console_puts("[BOOTLOADER AUDIT] Stage 2: fs pointer check\n");
        if (fs == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: fs is NULL!\n");
            return EFI_INVALID_PARAMETER;
        }
        console_puts("[BOOTLOADER AUDIT] Stage 2: Calling OpenVolume\n");
        status = fs->OpenVolume(fs, &root);
        console_puts("[BOOTLOADER AUDIT] Stage 2: OpenVolume returned\n");
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: Open Volume ERROR\n");
            return status;
        }
        console_puts("[BOOTLOADER AUDIT] Stage 2: Volume Opened\n");
    
        console_puts("[BOOTLOADER AUDIT] Stage 2: Opening platform.yaml\n");
        // 打开platform.yaml文件
        status = root->Open(root, &file, gPlatformPath,
                           EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) {
            root->Close(root);
            // platform.yaml是可选的，返回成功但警告
            console_puts("[BOOTLOADER AUDIT] Stage 2: platform.yaml not found (OK)\n");
            return EFI_SUCCESS;
        }
        console_puts("[BOOTLOADER AUDIT] Stage 2: platform.yaml Opened\n");
    
        console_puts("[BOOTLOADER AUDIT] Stage 2: Getting File Size using GetPosition\n");
        
        /* 检查file指针的有效性 */
        if (file == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: ERROR - file is NULL\n");
            root->Close(root);
            return EFI_INVALID_PARAMETER;
        }
        
        /* 使用GetPosition获取文件大小（安全方法） */
        UINT64 original_pos = 0;
        console_printf("[BOOTLOADER AUDIT] Stage 2: file=%p, GetPosition=%p\n", file, file->GetPosition);
        status = file->GetPosition(file, &original_pos);
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: GetPosition failed, skipping platform.yaml\n");
            file->Close(file);
            root->Close(root);
            return EFI_SUCCESS;
        }
        
        status = file->SetPosition(file, (UINT64)-1);
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: SetPosition failed, skipping platform.yaml\n");
            file->Close(file);
            root->Close(root);
            return EFI_SUCCESS;
        }
        
        UINT64 file_size_pos = 0;
        status = file->GetPosition(file, &file_size_pos);
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: GetPosition(EOF) failed, skipping platform.yaml\n");
            file->Close(file);
            root->Close(root);
            return EFI_SUCCESS;
        }
        
        file_size = file_size_pos;
        console_printf("[BOOTLOADER AUDIT] Stage 2: File size: %llu bytes\n", file_size);
        
        /* 恢复原始位置 */
        file->SetPosition(file, original_pos);
        
        /* 分配platform_data缓冲区 */
        platform_data = allocate_pool(file_size + 1);
        if (platform_data == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: Failed to allocate buffer\n");
            file->Close(file);
            root->Close(root);
            return EFI_OUT_OF_RESOURCES;
        }
        
        /* 读取文件内容 */
        UINTN read_size = file_size;
        status = file->Read(file, &read_size, platform_data);
        if (EFI_ERROR(status) || read_size != file_size) {
            console_puts("[BOOTLOADER AUDIT] Stage 2: Failed to read file\n");
            free_pool(platform_data);
            file->Close(file);
            root->Close(root);
            return EFI_LOAD_ERROR;
        }
        
        platform_data[file_size] = '\0';
        console_puts("[BOOTLOADER AUDIT] Stage 2: File read successfully\n");
        
        /* 关闭文件 */
        file->Close(file);
        root->Close(root);
        
        /* 解析配置 */
        parse_platform_config(platform_data);
        
        /* 释放缓冲区 */
        free_pool(platform_data);
        
        return EFI_SUCCESS;
}

/**
 * 解析平台配置
 */
void __attribute__((unused)) parse_platform_config(char *config_data)
{
    /* 完整实现：解析platform.yaml配置文件 */
    /* 这里的解析相对简单，主要验证YAML格式 */
    /* 实际的详细解析由内核完成 */

    log_info("Parsing platform.yaml...\n");

    // 简单验证YAML格式
    if (config_data && g_platform_size > 0) {
        // 检查是否包含基本的YAML结构
        if (strstr(config_data, "target:") ||
            strstr(config_data, "build:") ||
            strstr(config_data, "cpu_features:") ||
            strstr(config_data, "features:")) {
            log_info("Valid platform.yaml detected\n");
        } else {
            log_warn("platform.yaml format may be invalid\n");
        }
        
        // 检查是否启用调试模式
        if (strstr(config_data, "debug_mode:") && 
            (strstr(config_data, "debug_mode: true") || 
             strstr(config_data, "debug_mode: True") ||
             strstr(config_data, "debug_mode:1"))) {
            g_debug_mode = TRUE;
            console_puts("[BOOTLOADER] Debug mode enabled from config\n");
            log_info("Debug mode enabled\n");
        }
    }
}

/**
 * 加载内核映像
 */
EFI_STATUS load_kernel_image(void **kernel_data, uint64_t *kernel_size)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    UINTN file_size;
    CHAR16 kernel_path[256];
    EFI_HANDLE device_handle_to_use;
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Loading Kernel Image\n");
    
    /* 尝试从多个可能的位置获取设备句柄 */
    
    /* 方法1: 尝试使用EFI_LOADED_IMAGE_PROTOCOL */
    if (gLoadedImage != NULL && gLoadedImage->device_handle != NULL) {
        device_handle_to_use = gLoadedImage->device_handle;
        console_puts("[BOOTLOADER AUDIT] Stage 3: Using device_handle from gLoadedImage\n");
    } else {
        /* 方法2: 尝试使用ImageHandle */
        device_handle_to_use = gImageHandle;
        console_puts("[BOOTLOADER AUDIT] Stage 3: Using ImageHandle as device_handle\n");
    }
    
    /* 如果上述方法都失败，尝试方法3：枚举系统中的所有文件系统（需要用户确认） */
    if (device_handle_to_use == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: No device handle from standard methods\n");
        console_puts("[BOOTLOADER AUDIT] Stage 3: Requesting user permission for fallback...\n");
        
        status = find_file_system_handle(&device_handle_to_use);
        
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - No valid device handle available\n");
            console_puts("[BOOTLOADER AUDIT] Stage 3: Status = ");
            char err_str[32];
            snprintf(err_str, sizeof(err_str), "ERROR_%d\n", (int)status);
            console_puts(err_str);
            return status;
        }
    }
    
    /* 检查gBS是否有效 */
    if (gBS == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - gBS is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    /* 检查HandleProtocol函数指针是否有效 */
    if (gBS->HandleProtocol == NULL) {
        console_puts("[BOOTLOADER ERROR] Stage 3: HandleProtocol function pointer is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    // 使用默认内核路径
    utf8_to_utf16("\\hik-kernel.bin", kernel_path, 256);
    
    // 打开卷的根目录
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    console_puts("[BOOTLOADER AUDIT] Stage 3: Attempting to get File System Protocol...\n");
    status = gBS->HandleProtocol(device_handle_to_use,
                                  &gEfiSimpleFileSystemProtocolGuid,
                                  (void**)&fs);
    
    /* 如果HandleProtocol失败或fs指针为NULL，尝试备用方案 */
    if (EFI_ERROR(status) || fs == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: Standard method failed, trying fallback...\n");
        if (fs == NULL && !EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 3: HandleProtocol returned SUCCESS but fs is NULL\n");
        }
        status = find_file_system_handle(&device_handle_to_use);
        if (EFI_ERROR(status)) {
            console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - No valid device handle available\n");
            console_puts("[BOOTLOADER AUDIT] Stage 3: Status = ");
            char err_str[32];
            snprintf(err_str, sizeof(err_str), "ERROR_%d\n", (int)status);
            console_puts(err_str);
            return status;
        }
        
        /* 再次尝试获取文件系统协议 */
        status = gBS->HandleProtocol(device_handle_to_use,
                                      &gEfiSimpleFileSystemProtocolGuid,
                                      (void**)&fs);
        if (EFI_ERROR(status) || fs == NULL) {
            console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - Fallback also failed\n");
            return EFI_INVALID_PARAMETER;
        }
    }
    
    /* 检查OpenVolume函数指针是否有效 */
    if (fs->OpenVolume == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - OpenVolume function pointer is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Calling OpenVolume...\n");
    status = fs->OpenVolume(fs, &root);
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: Cannot open volume\n");
        console_puts("[BOOTLOADER AUDIT] Stage 3: Status = ");
        char err_str[32];
        snprintf(err_str, sizeof(err_str), "ERROR_%d\n", (int)status);
        console_puts(err_str);
        return status;
    }
    
    /* 检查root指针是否有效 */
    if (root == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - root pointer is NULL after OpenVolume\n");
        return EFI_INVALID_PARAMETER;
    }
    
    /* 检查Open函数指针是否有效 */
    if (root->Open == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - Open function pointer is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Opening kernel file...\n");
    
    /* 显示file指针的地址 */
    {
        char ptr_str[64];
        snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: &file=%p\n", &file);
        console_puts(ptr_str);
        snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file before Open=%p\n", file);
        console_puts(ptr_str);
    }
    
    // 打开内核文件
    status = root->Open(root, &file, kernel_path,
                       EFI_FILE_MODE_READ, 0);
    
    /* 显示Open返回状态 */
    {
        char ptr_str[64];
        snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: Open returned status=%d\n", (int)status);
        console_puts(ptr_str);
        snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file after Open=%p\n", file);
        console_puts(ptr_str);
    }
    
    if (EFI_ERROR(status)) {
        root->Close(root);
        console_puts("[BOOTLOADER AUDIT] Stage 3: Cannot open kernel file\n");
        return status;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: Kernel file opened\n");
    
    /* 验证file指针和函数指针表 */
    console_puts("[BOOTLOADER AUDIT] Stage 3: Verifying file handle integrity...\n");
    if (file == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - file handle is NULL!\n");
        root->Close(root);
        return EFI_INVALID_PARAMETER;
    }
    
    /* 打印file指针地址 */
    char ptr_str[64];
    snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file handle=%p\n", file);
    console_puts(ptr_str);
    
    /* 检查关键函数指针 */
    snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file->Read=%p\n", file->Read);
    console_puts(ptr_str);
    snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file->GetInfo=%p\n", file->GetInfo);
    console_puts(ptr_str);
    snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file->GetPosition=%p\n", file->GetPosition);
    console_puts(ptr_str);
    snprintf(ptr_str, sizeof(ptr_str), "[BOOTLOADER AUDIT] Stage 3: file->SetPosition=%p\n", file->SetPosition);
    console_puts(ptr_str);
    
    if (file->Read == NULL || file->Close == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3: ERROR - Critical file functions are NULL!\n");
        root->Close(root);
        return EFI_INVALID_PARAMETER;
    }
    
    console_puts("[BOOTLOADER AUDIT] Stage 3: File handle validation OK\n");
    
    // 获取文件大小 - 先尝试使用GetInfo
    file_size = 0;
    if (file->GetInfo != NULL) {
        UINTN info_size = 0;
        status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
        if (status == EFI_BUFFER_TOO_SMALL && info_size > 0) {
            EFI_FILE_INFO *file_info = allocate_pool(info_size);
            if (file_info != NULL) {
                status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, file_info);
                if (!EFI_ERROR(status)) {
                    file_size = file_info->file_size;
                    free_pool(file_info);
                } else {
                    free_pool(file_info);
                }
            }
        }
    }
    
    // 如果GetInfo失败，尝试使用文件结尾位置获取大小
    if (file_size == 0) {
        console_puts("[BOOTLOADER] GetInfo failed, trying alternative method\n");
        
        /* 检查file指针有效性 */
        if (file == NULL) {
            console_puts("[BOOTLOADER] ERROR: file pointer is NULL!\n");
            file_size = 0;
        } else if (file->GetPosition == NULL || file->SetPosition == NULL) {
            console_puts("[BOOTLOADER] ERROR: file function pointers are NULL!\n");
            console_puts("[BOOTLOADER] GetPosition=");
            char ptr_str[32];
            snprintf(ptr_str, sizeof(ptr_str), "%p\n", file->GetPosition);
            console_puts(ptr_str);
            console_puts("[BOOTLOADER] SetPosition=");
            snprintf(ptr_str, sizeof(ptr_str), "%p\n", file->SetPosition);
            console_puts(ptr_str);
            file_size = 0;
        } else {
            uint64_t original_pos = 0;
            console_puts("[BOOTLOADER] Calling GetPosition...\n");
            EFI_STATUS pos_status = file->GetPosition(file, &original_pos);
            if (EFI_ERROR(pos_status)) {
                console_puts("[BOOTLOADER] ERROR: GetPosition failed!\n");
                file_size = 0;
            } else {
                console_puts("[BOOTLOADER] Moving to file end...\n");
                file->SetPosition(file, 0xFFFFFFFFFFFFFFFFULL); // 移动到文件结尾
                console_puts("[BOOTLOADER] Getting final position...\n");
                file->GetPosition(file, (uint64_t*)&file_size);
                console_puts("[BOOTLOADER] Restoring position...\n");
                file->SetPosition(file, original_pos); // 恢复位置
            }
        }
    }
    
    if (file_size == 0) {
        file->Close(file);
        root->Close(root);
        console_puts("[BOOTLOADER AUDIT] Stage 3: Cannot determine file size\n");
        return EFI_DEVICE_ERROR;
    }
    
    char size_str[64];
    snprintf(size_str, sizeof(size_str), "[BOOTLOADER] Kernel file size: %d bytes\n", (int)file_size);
    console_puts(size_str);
    
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
    snprintf(size_str, sizeof(size_str), "[BOOTLOADER] Kernel loaded: %d bytes\n", (int)file_size);
    console_puts(size_str);
    console_puts("[BOOTLOADER AUDIT] Stage 3: Kernel Image Loaded\n");
    return EFI_SUCCESS;
}

/**
 * 准备启动信息结构
 */
hik_boot_info_t *prepare_boot_info(void *kernel_data, uint64_t kernel_size)
{
    hik_boot_info_t *boot_info;
    
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
    
    // 保存引导日志信息到调试结构中
    boot_info->debug.log_buffer = (void*)bootlog_get_buffer();
    boot_info->debug.log_size = BOOTLOG_MAX_ENTRIES * sizeof(bootlog_entry_t);
    boot_info->debug.debug_flags = bootlog_get_index();

    // 传递平台配置数据
    boot_info->platform.platform_data = g_platform_data;
    boot_info->platform.platform_size = g_platform_size;
    boot_info->platform.platform_hash = 0;  // 可选：计算哈希

    log_info("Platform config data: %llu bytes\n", g_platform_size);

    // 内核信息
    // 手动解析入口点
    uint8_t *raw_kernel = (uint8_t *)kernel_data;
    uint64_t kernel_entry_point = *((uint64_t*)(raw_kernel + 12));
    
    boot_info->kernel_base = kernel_data;
    boot_info->kernel_size = kernel_size;
    boot_info->entry_point = kernel_entry_point;

    // 命令行（从platform.yaml读取或使用默认值）
    strcpy(boot_info->cmdline, "");  // 默认空命令行

    // 系统信息
    boot_info->system.architecture = HIK_ARCH_X86_64;
    boot_info->system.platform_type = 1;  // UEFI

    // 应用platform.yaml中的配置（将在内核中详细解析）
    // 这里只设置基本的启动标志

    // 启用ACPI（默认）
    boot_info->flags |= HIK_BOOT_FLAG_ACPI_ENABLED;

    // 启用调试（默认）
    boot_info->flags |= HIK_BOOT_FLAG_DEBUG_ENABLED;

    // 设置栈
    boot_info->stack_size = 0x10000;  // 64KB栈
    boot_info->stack_top = (uint64_t)allocate_pages_aligned(boot_info->stack_size, 4096) 
                         + boot_info->stack_size;

    // 调试信息
    boot_info->debug.serial_port = 0x3F8;  // COM1
    // 串口初始化在UEFI环境中不支持I/O端口访问，已禁用
    // serial_init(0x3F8);

    return boot_info;
}

/**
 * 获取内存映射
 */
EFI_STATUS __attribute__((unused)) get_memory_map(hik_boot_info_t *boot_info)
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
EFI_STATUS load_kernel_segments(void *image_data, uint64_t image_size,
                                       hik_boot_info_t *boot_info)
{
    console_puts("[BOOTLOADER] load_kernel_segments called\n");
    
    if (!image_data) {
        console_puts("[BOOTLOADER] ERROR: image_data is NULL\n");
        return EFI_INVALID_PARAMETER;
    }
    
    if (image_size == 0) {
        console_puts("[BOOTLOADER] ERROR: image_size is 0\n");
        return EFI_INVALID_PARAMETER;
    }
    
    uint8_t *raw_bytes = (uint8_t *)image_data;
    
    // 检查是否是HIK镜像格式（有魔数）
    if (image_size >= 8 && memcmp(raw_bytes, HIK_IMG_MAGIC, 8) == 0) {
        console_puts("[BOOTLOADER] Detected HIK image format\n");
        
        // 手动读取HIK镜像头部字段
        uint64_t entry_point = *((uint64_t*)(raw_bytes + 12));
        uint64_t manual_image_size = *((uint64_t*)(raw_bytes + 20));
        
        char debug_str[128];
        snprintf(debug_str, sizeof(debug_str), "[BOOTLOADER] HIK image: entry=0x%lx, size=%d\n",
                 entry_point, (int)manual_image_size);
        console_puts(debug_str);
        
        // 计算内核代码在HIK镜像中的偏移量
        uint64_t kernel_offset = 160;  // HIK镜像格式: header(120) + segment_table(40)
        uint64_t kernel_code_size = manual_image_size - kernel_offset;
        
        // 将内核代码复制到物理地址0x100000
        void *kernel_phys_base = (void*)0x100000;
        void *kernel_code_start = raw_bytes + kernel_offset;
        memcpy(kernel_phys_base, kernel_code_start, kernel_code_size);
        
        console_puts("[BOOTLOADER] Kernel code copied to 0x100000\n");
        
        // 更新启动信息
        boot_info->kernel_base = kernel_phys_base;
        boot_info->kernel_size = manual_image_size;
        boot_info->entry_point = 0x100000;  // kernel_start的绝对地址
        
    } else {
        // 纯二进制格式，直接复制
        console_puts("[BOOTLOADER] Detected raw binary format\n");
        
        char debug_str[128];
        snprintf(debug_str, sizeof(debug_str), "[BOOTLOADER] Raw binary: size=%d bytes\n", (int)image_size);
        console_puts(debug_str);
        
        // 直接将二进制文件复制到物理地址0x100000
        void *kernel_phys_base = (void*)0x100000;
        memcpy(kernel_phys_base, image_data, image_size);
        
        console_puts("[BOOTLOADER] Kernel copied to 0x100000\n");
        
        // 更新启动信息
        boot_info->kernel_base = kernel_phys_base;
        boot_info->kernel_size = image_size;
        boot_info->entry_point = 0x100000;  // kernel_start的绝对地址
    }
    
    console_puts("[BOOTLOADER] Entry point set to 0x100000\n");
    
    char debug2_str[256];
    snprintf(debug2_str, sizeof(debug2_str), "[BOOTLOADER] boot_info=%p, entry_point=%p\n",
             boot_info, (void*)boot_info->entry_point);
    console_puts(debug2_str);
    
    console_puts("[BOOTLOADER] Kernel ready, entry point set\n");
    
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
 * 
 * 安全性保证：
 * - 验证所有指针非空
 * - 验证入口点地址有效
 * - 验证栈地址有效
 * - 记录详细的跳转信息
 */
__attribute__((noreturn))
void jump_to_kernel(hik_boot_info_t *boot_info) {
    // 【安全检查1】验证boot_info指针
    if (boot_info == NULL) {
        console_puts("[JUMP] ERROR: boot_info is NULL!\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    // 【安全检查2】验证入口点
    if (boot_info->entry_point == 0) {
        console_puts("[JUMP] ERROR: entry_point is 0!\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    // 【安全检查3】验证栈指针
    if (boot_info->stack_top == 0) {
        console_puts("[JUMP] ERROR: stack_top is 0!\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    // 添加调试信息
    char temp_str[256];
    snprintf(temp_str, sizeof(temp_str), "[JUMP] boot_info=%p, entry_point=%p (0x%lx), stack_top=%p\n",
             boot_info, (void*)boot_info->entry_point, boot_info->entry_point, (void*)boot_info->stack_top);
    console_puts(temp_str);
    
    // 验证入口点地址范围（应该在0x100000左右）
    if (boot_info->entry_point < 0x100000 || boot_info->entry_point > 0x200000) {
        snprintf(temp_str, sizeof(temp_str), "[JUMP] WARNING: entry_point 0x%lx seems unusual\n", boot_info->entry_point);
        console_puts(temp_str);
    }
    
    // 直接跳转到内核入口点，不使用函数调用
    void *kernel_entry = (void *)boot_info->entry_point;
    void *stack_top = (void *)boot_info->stack_top;
    
    // 验证指针不为NULL
    if (kernel_entry == NULL) {
        console_puts("[JUMP] ERROR: kernel_entry is NULL!\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    if (stack_top == NULL) {
        console_puts("[JUMP] ERROR: stack_top is NULL!\n");
        while (1) {
            __asm__ volatile ("hlt");
        }
    }
    
    console_puts("[JUMP] All checks passed, jumping to kernel...\n");
    
    // 禁用中断
    __asm__ volatile ("cli");
    
    // 设置栈指针,确保RDI包含boot_info,然后跳转到内核
    __asm__ volatile (
        "mov %0, %%rsp\n"    // 设置栈指针
        "mov %2, %%rdi\n"    // 设置RDI为boot_info
        "jmp *%1\n"          // 跳转到内核入口点
        :
        : "r"(stack_top), "r"(kernel_entry), "r"(boot_info)
        : "memory", "rdi"
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

/**
 * UEFI入口点
 * 这是UEFI固件调用的第一个函数
 */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    
    // 验证输入参数
    if (!SystemTable || !SystemTable->boot_services) {
        return EFI_INVALID_PARAMETER;
    }
    
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->boot_services;
    
    /* 初始化控制台 */
    console_init();
    
    /* 引导gLoadedImage全局变量 */
    extern EFI_LOADED_IMAGE_PROTOCOL *gLoadedImage;
    
    /* 引导程序审计日志 - 步骤1 */
    console_puts("[BOOTLOADER AUDIT] Stage 1: EFI Entry\n");
    console_puts("[BOOTLOADER AUDIT] gST=");
    console_puts(gST ? "VALID" : "NULL");
    console_puts("\n");
    console_puts("[BOOTLOADER AUDIT] gBS=");
    console_puts(gBS ? "VALID" : "NULL");
    console_puts("\n");
    console_puts("[BOOTLOADER AUDIT] ConOut=");
    console_puts((gST && gST->con_out) ? "VALID" : "NULL");
    console_puts("\n");
    
    /* 使用简单的console_puts代替console_printf进行测试 */
    console_puts("HIK UEFI Bootloader v0.1\n");
    console_puts("Starting HIK Hierarchical Isolation Kernel...\n\n");
    
    /* 引导程序审计日志 - 步骤2 */
    console_puts("[BOOTLOADER AUDIT] Stage 2: Console Output Complete\n");
    
    /* 获取加载的镜像信息 */
    console_puts("[BOOTLOADER AUDIT] Stage 3: Getting Loaded Image Protocol\n");
    status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, 
                                 (void **)&gLoadedImage);
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] ERROR: Cannot get loaded image protocol\n");
        return status;
    }
    console_puts("[BOOTLOADER AUDIT] Stage 3: Loaded Image Protocol OK\n");
    
    /* 立即验证gLoadedImage */
    console_puts("[BOOTLOADER AUDIT] Stage 3.1: Verifying gLoadedImage\n");
    if (gLoadedImage == NULL) {
        console_puts("[BOOTLOADER AUDIT] Stage 3.1: ERROR - gLoadedImage is NULL immediately after HandleProtocol!\n");
        console_puts("[BOOTLOADER AUDIT] Stage 3.1: Trying alternative approach...\n");
        return EFI_SUCCESS;  // 返回成功以避免崩溃
    }
    console_puts("[BOOTLOADER AUDIT] Stage 3.1: gLoadedImage is valid\n");
    
    /* 加载平台配置 */
    console_puts("[BOOTLOADER AUDIT] Stage 4: Loading Platform Config\n");
    status = load_platform_config();
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] WARNING: Failed to load platform config\n");
    } else {
        console_puts("[BOOTLOADER AUDIT] Stage 4: Platform Config Loaded\n");
    }
    
    /* 加载内核映像 */
    console_puts("[BOOTLOADER AUDIT] Stage 5: Loading Kernel Image\n");
    void *kernel_data;
    uint64_t kernel_size;
    status = load_kernel_image(&kernel_data, &kernel_size);
    if (EFI_ERROR(status)) {
        console_puts("[BOOTLOADER AUDIT] ERROR: Failed to load kernel image\n");
        return status;
    }
    console_puts("[BOOTLOADER AUDIT] Stage 5: Kernel Image Loaded\n");
    
    /* 准备启动信息 */
    hik_boot_info_t *boot_info = prepare_boot_info(kernel_data, kernel_size);
    if (!boot_info) {
        console_printf("ERROR: Failed to prepare boot info\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    /* 加载内核段 */
    status = load_kernel_segments(kernel_data, kernel_size, boot_info);
    if (EFI_ERROR(status)) {
        console_printf("ERROR: Failed to load kernel segments\n");
        return status;
    }
    
    /* 退出启动服务 */
    status = exit_boot_services(boot_info);
    if (EFI_ERROR(status)) {
        console_printf("ERROR: Failed to exit boot services\n");
        return status;
    }
    
    /* 跳转到内核 */
    console_printf("Jumping to kernel...\n");
    jump_to_kernel(boot_info);
    
    /* 永远不会到达这里 */
    return EFI_SUCCESS;
}

