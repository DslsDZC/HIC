/**
 * HIK UEFI Bootloader - EFI Interface Header
 * 完整的EFI接口定义
 */

#ifndef HIK_BOOTLOADER_EFI_H
#define HIK_BOOTLOADER_EFI_H

// 基础类型
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef unsigned long long size_t;

// UEFI类型
typedef void* VOID;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint8_t CHAR8;
typedef uint16_t CHAR16;
typedef unsigned long long UINTN;
typedef signed long long INTN;
typedef uint8_t BOOLEAN;
typedef UINTN EFI_STATUS;

// GUID
typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} EFI_GUID;

// 常量
#define EFI_SUCCESS 0
#define EFI_ERROR(x) ((x) & 0x80000000)
#define EFI_LOAD_ERROR ((EFI_STATUS)1 | (1UL << 31))
#define EFI_INVALID_PARAMETER ((EFI_STATUS)2 | (1UL << 31))
#define EFI_UNSUPPORTED ((EFI_STATUS)3 | (1UL << 31))
#define EFI_OUT_OF_RESOURCES ((EFI_STATUS)9 | (1UL << 31))
#define EFI_SECURITY_VIOLATION ((EFI_STATUS)26 | (1UL << 31))
#define EFI_BUFFER_TOO_SMALL ((EFI_STATUS)5 | (1UL << 31))
#define EFI_NOT_FOUND ((EFI_STATUS)14 | (1UL << 31))
#define EFI_DEVICE_ERROR ((EFI_STATUS)7 | (1UL << 31))

// 分配类型
#define AllocateAnyPages 0

// 内存类型
typedef enum {
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
} EFI_MEMORY_TYPE;

// 表头
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

// 物理地址
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;

// 句柄和事件
typedef void* EFI_HANDLE;
typedef void* EFI_EVENT;

// 文件模式
#define EFI_FILE_MODE_READ 1ULL

// EFI文件协议
typedef struct _EFI_FILE_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (*Open)(struct _EFI_FILE_PROTOCOL *this, struct _EFI_FILE_PROTOCOL **new_handle,
                      CHAR16 *filename, uint64_t open_mode, uint64_t attributes);
    EFI_STATUS (*Close)(struct _EFI_FILE_PROTOCOL *this);
    EFI_STATUS (*Delete)(struct _EFI_FILE_PROTOCOL *this);
    EFI_STATUS (*Read)(struct _EFI_FILE_PROTOCOL *this, uint64_t *buffer_size, void *buffer);
    EFI_STATUS (*Write)(struct _EFI_FILE_PROTOCOL *this, uint64_t *buffer_size, void *buffer);
    EFI_STATUS (*GetPosition)(struct _EFI_FILE_PROTOCOL *this, uint64_t *position);
    EFI_STATUS (*SetPosition)(struct _EFI_FILE_PROTOCOL *this, uint64_t position);
    EFI_STATUS (*GetInfo)(struct _EFI_FILE_PROTOCOL *this, EFI_GUID *info_type,
                         uint64_t *buffer_size, void *buffer);
    EFI_STATUS (*SetInfo)(struct _EFI_FILE_PROTOCOL *this, EFI_GUID *info_type,
                         uint64_t buffer_size, void *buffer);
    EFI_STATUS (*Flush)(struct _EFI_FILE_PROTOCOL *this);
} EFI_FILE_PROTOCOL;

// EFI文件信息
typedef struct {
    uint64_t size;
    uint64_t file_size;
    uint64_t physical_size;
    uint8_t reserved[52];
    CHAR16 filename[1];
} EFI_FILE_INFO;

// 简单文件系统协议
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (*OpenVolume)(struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *this,
                             EFI_FILE_PROTOCOL **root_volume);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// 加载映像协议
typedef struct {
    uint32_t revision;
    EFI_HANDLE parent_handle;
    EFI_HANDLE device_handle;
    void *file_path;
    void *load_options;
    uint32_t load_options_size;
    void *image_base;
    uint64_t image_size;
    EFI_MEMORY_TYPE image_code_type;
    EFI_MEMORY_TYPE image_data_type;
    EFI_HANDLE image_handle;
} EFI_LOADED_IMAGE_PROTOCOL;

// 简单文本输出协议
typedef struct {
    uint64_t reset;
    uint64_t output_string;
    uint64_t test_string;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// 简单文本输入协议
typedef struct {
    uint64_t reset;
    uint64_t read_key_stroke;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

// EFI启动服务
typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    EFI_STATUS (*AllocatePool)(uint32_t pool_type, UINTN size, void **buffer);
    EFI_STATUS (*FreePool)(void *buffer);
    EFI_STATUS (*AllocatePages)(uint32_t allocation_type, uint32_t memory_type,
                                 UINTN pages, EFI_PHYSICAL_ADDRESS *memory);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS memory, UINTN pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *memory_map_size, void *memory_map,
                                  UINTN *map_key, UINTN *descriptor_size, uint32_t *descriptor_version);
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE handle, EFI_GUID *protocol, void **interface);
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE image_handle, UINTN map_key);
    void *reserved[50];
} EFI_BOOT_SERVICES;

// EFI配置表
typedef struct {
    EFI_GUID vendor_guid;
    void *vendor_table;
} EFI_CONFIGURATION_TABLE;

// EFI系统表
typedef struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER hdr;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    EFI_HANDLE console_in_handle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *con_in;
    EFI_HANDLE console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
    EFI_HANDLE standard_error_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *std_err;
    void *runtime_services;
    struct _EFI_BOOT_SERVICES *boot_services;
    uint64_t NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

// 内存描述符
typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

// GUID声明
extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;
extern EFI_GUID gEfiAcpi20TableGuid;
extern EFI_GUID gEfiAcpiTableGuid;

#endif // HIK_BOOTLOADER_EFI_H