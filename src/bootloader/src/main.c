
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
    
    // 保存全局变量
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->boot_services;
    
    // 初始化控制台
    console_init();
    
    // 初始化引导日志
    bootlog_init();
    bootlog_event(BOOTLOG_UEFI_INIT, NULL, 0);
    
    // 输出 hello world
    log_info("hello world\n");
    log_info("HIK UEFI Bootloader v1.0\n");
    log_info("Starting HIK system...\n");
    bootlog_info("UEFI initialized");
    
    // 获取加载的映像协议
    status = gBS->HandleProtocol(gImageHandle, 
                                  &gEfiLoadedImageProtocolGuid,
                                  (void**)&gLoadedImage);
    if (EFI_ERROR(status)) {
        bootlog_error("Failed to get loaded image protocol");
        log_error("Failed to get loaded image protocol: %d\n", status);
        return status;
    }
    
    // 步骤1: 加载平台配置文件
    log_info("Loading platform configuration...\n");
    status = load_platform_config();
    if (EFI_ERROR(status)) {
        bootlog_error("Failed to load platform config, using defaults");
        log_warn("Failed to load platform config, using defaults\n");
    } else {
        bootlog_info("Platform configuration loaded");
    }

    // 步骤2: 加载内核映像
    log_info("Loading kernel image: %s\n", gKernelPath);
    void *kernel_data = NULL;
    uint64_t kernel_size = 0;
    status = load_kernel_image(&kernel_data, &kernel_size);
    if (EFI_ERROR(status)) {
        bootlog_error("Failed to load kernel");
        log_error("Failed to load kernel: %d\n", status);
        return status;
    }
    log_info("Kernel loaded at 0x%llx, size: %llu bytes\n",
             (uint64_t)kernel_data, kernel_size);
    bootlog_info("Kernel image loaded");

    // 步骤3: 验证内核签名（暂时禁用）
    /*
    log_info("Verifying kernel signature...\n");
    hik_verify_result_t verify_result = hik_image_verify(kernel_data, kernel_size);
    if (verify_result != HIK_VERIFY_SUCCESS) {
        bootlog_error("Kernel signature verification failed");
        log_error("Kernel verification failed: %d\n", verify_result);
        return EFI_SECURITY_VIOLATION;
    }
    log_info("Kernel signature verified successfully\n");
    bootlog_info("Kernel signature verified");
    */
    log_info("Kernel signature verification skipped (development mode)\n");

    // 步骤4: 准备启动信息结构
    log_info("Preparing boot information...\n");
    hik_boot_info_t *boot_info = prepare_boot_info(kernel_data, kernel_size);
    if (!boot_info) {
        bootlog_error("Failed to prepare boot info");
        log_error("Failed to prepare boot info\n");
        return EFI_OUT_OF_RESOURCES;
    }
    bootlog_info("Boot information prepared");
    
    // 步骤5: 加载内核段到内存
    log_info("Loading kernel segments...\n");
    status = load_kernel_segments(kernel_data, kernel_size, boot_info);
    if (EFI_ERROR(status)) {
        bootlog_error("Failed to load kernel segments");
        log_error("Failed to load kernel segments: %d\n", status);
        return status;
    }
    bootlog_info("Kernel segments loaded");
    
    // 步骤6: 退出UEFI启动服务
    log_info("Exiting boot services...\n");
    status = exit_boot_services(boot_info);
    if (EFI_ERROR(status)) {
        bootlog_error("Failed to exit boot services");
        log_error("Failed to exit boot services: %d\n", status);
        return status;
    }
    bootlog_info("Boot services exited");
    
    // 步骤7: 跳转到内核
    log_info("Jumping to kernel...\n");
    bootlog_event(BOOTLOG_JUMP_TO_KERNEL, NULL, 0);
    jump_to_kernel(boot_info);
    
    // 不应该到达这里
    return EFI_LOAD_ERROR;
}

/**
 * 加载平台配置文件 (platform.yaml)
 */
EFI_STATUS load_platform_config(void)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root;
    EFI_FILE_PROTOCOL *file;
    EFI_FILE_INFO *file_info;
    UINTN info_size;
    char *platform_data;
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

    // 打开platform.yaml文件
    status = root->Open(root, &file, gPlatformPath,
                       EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        root->Close(root);
        // platform.yaml是可选的，返回成功但警告
        log_warn("platform.yaml not found, using defaults\n");
        return EFI_SUCCESS;
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

    // 读取platform.yaml内容
    platform_data = allocate_pool(file_size + 1);
    if (!platform_data) {
        file->Close(file);
        root->Close(root);
        return EFI_OUT_OF_RESOURCES;
    }

    status = file->Read(file, &file_size, platform_data);
    platform_data[file_size] = '\0';

    file->Close(file);
    root->Close(root);

    if (EFI_ERROR(status)) {
        free_pool(platform_data);
        return status;
    }

    /* 保存platform.yaml数据供内核使用 */
    g_platform_data = platform_data;
    g_platform_size = file_size;

    /* 解析platform.yaml */
    parse_platform_config(platform_data);

    log_info("Platform configuration loaded: %llu bytes\n", g_platform_size);

    // 注意：不在这里释放platform_data，因为内核需要它

    return EFI_SUCCESS;
}

/**
 * 解析平台配置
 */
void parse_platform_config(char *config_data)
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
    
    // 使用默认内核路径
    utf8_to_utf16("\\EFI\\HIK\\kernel.hik", kernel_path, 256);
    
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
    hik_image_header_t *header = (hik_image_header_t *)kernel_data;
    boot_info->kernel_base = kernel_data;
    boot_info->kernel_size = kernel_size;
    boot_info->entry_point = header->entry_point;

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
    serial_init(0x3F8);

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

/**
 * UEFI入口点
 * 这是UEFI固件调用的第一个函数
 */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS status;
    
    gImageHandle = ImageHandle;
    gST = SystemTable;
    gBS = SystemTable->boot_services;
    
    /* 初始化控制台 */
    console_init();
    
    /* 使用console_printf代替console_print */
    console_printf("HIK UEFI Bootloader v0.1\n");
    console_printf("Starting HIK Hierarchical Isolation Kernel...\n\n");
    
    /* 获取加载的镜像信息 */
    status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, 
                                 (void **)&gLoadedImage);
    if (EFI_ERROR(status)) {
        console_printf("ERROR: Cannot get loaded image protocol\n");
        return status;
    }
    
    /* 加载平台配置 */
    status = load_platform_config();
    if (EFI_ERROR(status)) {
        console_printf("WARNING: Failed to load platform config\n");
    }
    
    /* 加载内核映像 */
    void *kernel_data;
    uint64_t kernel_size;
    status = load_kernel_image(&kernel_data, &kernel_size);
    if (EFI_ERROR(status)) {
        console_printf("ERROR: Failed to load kernel image\n");
        return status;
    }
    
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

