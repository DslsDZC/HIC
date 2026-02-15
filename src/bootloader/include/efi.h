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

// 布尔值常量
#define FALSE 0
#define TRUE  1

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

// 简单文件系统协议 (UEFI 2.11规范)
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (*OpenVolume)(struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *this,
                             struct _EFI_FILE_PROTOCOL **root_volume);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

// 前向声明

typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;

typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;

typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;



// 加载映像协议 (UEFI 2.0规范)

typedef struct {

    uint32_t revision;

    EFI_HANDLE parent_handle;

    EFI_SYSTEM_TABLE *system_table;

    

    // 映像源位置

    EFI_HANDLE device_handle;

    void *file_path;

    void *reserved;

    

    // 加载选项

    uint32_t load_options_size;

    void *load_options;

    

    // 映像加载位置

    void *image_base;

    uint64_t image_size;

    EFI_MEMORY_TYPE image_code_type;

    EFI_MEMORY_TYPE image_data_type;

    

    // 卸载函数

    void *unload;

} EFI_LOADED_IMAGE_PROTOCOL;



// 简单文本输出协议 (UEFI 2.11规范)



typedef struct {



    INT32 max_mode;



    INT32 mode;



    INT32 attribute;



    INT32 cursor_column;



    INT32 cursor_row;



    BOOLEAN cursor_visible;



} SIMPLE_TEXT_OUTPUT_MODE;







typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {



    EFI_STATUS (*Reset)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, BOOLEAN extended_verification);



    EFI_STATUS (*OutputString)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, CHAR16 *string);



    EFI_STATUS (*TestString)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, CHAR16 *string);



    EFI_STATUS (*QueryMode)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, UINTN mode, UINTN *columns, UINTN *rows);



    EFI_STATUS (*SetMode)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, UINTN mode);



    EFI_STATUS (*SetAttribute)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, UINTN attribute);



    EFI_STATUS (*ClearScreen)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this);



    EFI_STATUS (*SetCursorPosition)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, UINTN column, UINTN row);



    EFI_STATUS (*EnableCursor)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, BOOLEAN visible);



    SIMPLE_TEXT_OUTPUT_MODE *mode;



} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;







// 简单文本输入协议 (UEFI 2.11规范)



typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {



    EFI_STATUS (*Reset)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *this, BOOLEAN extended_verification);



    EFI_STATUS (*ReadKeyStroke)(struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *this, void *key);



    void *wait_for_key;



} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

// LocateHandle搜索类型
typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

// 设备路径协议
typedef struct _EFI_DEVICE_PATH_PROTOCOL {
    uint8_t type;
    uint8_t sub_type;
    uint16_t length[1];
} EFI_DEVICE_PATH_PROTOCOL;

// OpenProtocol信息
typedef struct {
    EFI_HANDLE agent_handle;
    EFI_HANDLE controller_handle;
    uint32_t attributes;
    uint32_t open_count;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;

// EFI启动服务 (UEFI 2.0规范)
typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    EFI_STATUS (*RaiseTPL)(UINTN new_tpl);
    EFI_STATUS (*RestoreTPL)(UINTN old_tpl);
    EFI_STATUS (*AllocatePages)(uint32_t allocation_type, uint32_t memory_type,
                                 UINTN pages, EFI_PHYSICAL_ADDRESS *memory);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS memory, UINTN pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *memory_map_size, void *memory_map,
                                  UINTN *map_key, UINTN *descriptor_size, uint32_t *descriptor_version);
    EFI_STATUS (*AllocatePool)(uint32_t pool_type, UINTN size, void **buffer);
    EFI_STATUS (*FreePool)(void *buffer);
    EFI_STATUS (*CreateEvent)(uint32_t type, uint32_t notify_tpl,
                              void (*notify_function)(void *event, void *context),
                              void *notify_context, EFI_EVENT *event);
    EFI_STATUS (*SetTimer)(EFI_EVENT event, uint32_t type, uint64_t trigger_time);
    EFI_STATUS (*WaitForEvent)(UINTN number_of_events, EFI_EVENT *event, UINTN *index);
    void (*SignalEvent)(EFI_EVENT event);
    void (*CloseEvent)(EFI_EVENT event);
    EFI_STATUS (*CheckEvent)(EFI_EVENT event);
    
    // Task Priority Services
    EFI_STATUS (*InstallProtocolInterface)(EFI_HANDLE *handle, EFI_GUID *protocol,
                                            uint32_t interface_type, void *interface);
    EFI_STATUS (*ReinstallProtocolInterface)(EFI_HANDLE handle, EFI_GUID *protocol,
                                             void *old_interface, void *new_interface);
    EFI_STATUS (*UninstallProtocolInterface)(EFI_HANDLE handle, EFI_GUID *protocol,
                                              void *interface);
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE handle, EFI_GUID *protocol, void **interface);
    void *reserved1;
    EFI_STATUS (*RegisterProtocolNotify)(EFI_GUID *protocol, EFI_EVENT event,
                                         void **registration);
    EFI_STATUS (*LocateHandle)(EFI_LOCATE_SEARCH_TYPE search_type, EFI_GUID *protocol, 
                               void *search_key, UINTN *buffer_size, EFI_HANDLE *buffer);
    EFI_STATUS (*LocateDevicePath)(EFI_GUID *protocol, EFI_DEVICE_PATH_PROTOCOL **device_path,
                                    EFI_HANDLE *device);
    EFI_STATUS (*InstallConfigurationTable)(EFI_GUID *guid, void *table);
    
    // Image Services
    EFI_STATUS (*LoadImage)(BOOLEAN boot_policy, EFI_HANDLE parent_image_handle,
                            EFI_DEVICE_PATH_PROTOCOL *device_path, void *source_buffer,
                            UINTN source_size, EFI_HANDLE *image_handle);
    EFI_STATUS (*StartImage)(EFI_HANDLE image_handle, UINTN *exit_data_size, CHAR16 **exit_data);
    EFI_STATUS (*Exit)(EFI_HANDLE image_handle, EFI_STATUS exit_status, UINTN exit_data_size,
                      CHAR16 *exit_data);
    EFI_STATUS (*UnloadImage)(EFI_HANDLE image_handle);
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE image_handle, UINTN map_key);
    
    // Misc Services
    EFI_STATUS (*GetNextMonotonicCount)(UINT64 *count);
    EFI_STATUS (*Stall)(UINTN microseconds);
    EFI_STATUS (*SetWatchdogTimer)(UINTN timeout, UINT64 watchdog_code, UINT64 data_size,
                                    CHAR16 *watchdog_data);
    EFI_STATUS (*ConnectController)(EFI_HANDLE controller_handle, EFI_HANDLE *driver_image_handle,
                                    EFI_DEVICE_PATH_PROTOCOL *remaining_device_path,
                                    BOOLEAN recursive);
    EFI_STATUS (*DisconnectController)(EFI_HANDLE controller_handle, EFI_HANDLE driver_image_handle,
                                       EFI_HANDLE child_handle);
    
    // Protocol Open/Close Services
    EFI_STATUS (*OpenProtocol)(EFI_HANDLE handle, EFI_GUID *protocol, void **interface,
                               EFI_HANDLE agent_handle, EFI_HANDLE controller_handle, uint32_t attributes);
    EFI_STATUS (*CloseProtocol)(EFI_HANDLE handle, EFI_GUID *protocol, EFI_HANDLE agent_handle,
                                EFI_HANDLE controller_handle);
    EFI_STATUS (*OpenProtocolInformation)(EFI_HANDLE handle, EFI_GUID *protocol,
                                          EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **entry_buffer,
                                          UINTN *entry_count);
    
    // Library Services
    EFI_STATUS (*ProtocolsPerHandle)(EFI_HANDLE handle, EFI_GUID ***protocol_buffer,
                                      UINTN *protocol_buffer_count);
    EFI_STATUS (*LocateHandleBuffer)(EFI_LOCATE_SEARCH_TYPE search_type, EFI_GUID *protocol,
                                      void *search_key, UINTN *no_handles, EFI_HANDLE **buffer);
    EFI_STATUS (*LocateProtocol)(EFI_GUID *protocol, void *registration, void **interface);
    EFI_STATUS (*InstallMultipleProtocolInterfaces)(EFI_HANDLE *handle, ...);
    EFI_STATUS (*UninstallMultipleProtocolInterfaces)(EFI_HANDLE *handle, ...);
    
    // CRC Services
    EFI_STATUS (*CalculateCrc32)(void *data, UINTN data_size, uint32_t *crc32);
    
    // Misc Services
    void (*CopyMem)(void *destination, void *source, UINTN length);
    void (*SetMem)(void *buffer, UINTN size, uint8_t value);
    EFI_STATUS (*CreateEventEx)(uint32_t type, uint32_t notify_tpl,
                                 void (*notify_function)(void *event, void *context),
                                 void *notify_context, EFI_GUID *event_group,
                                 EFI_EVENT *event);
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