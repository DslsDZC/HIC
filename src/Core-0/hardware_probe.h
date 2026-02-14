/**
 * HIK内核运行时硬件探测模块
 * 提供架构无关的硬件发现接口
 */

#include <stdint.h>
#include "types.h"
#include "hal.h"

/* CPU信息结构（架构无关） */
typedef struct cpu_info {
    u32 vendor_id[3];           /* 厂商ID */
    u32 version;               /* 版本信息 */
    u32 feature_flags[4];      /* 特性标志（架构相关） */
    u8 family;                 /* CPU家族 */
    u8 model;                  /* CPU型号 */
    u8 stepping;               /* 步进 */
    u8 cache_line_size;        /* 缓存行大小 */
    u32 logical_cores;         /* 逻辑核心数 */
    u32 physical_cores;        /* 物理核心数 */
    u64 clock_frequency;       /* 时钟频率(Hz) */
    char brand_string[49];     /* 品牌字符串 */
    u32 arch_type;             /* 架构类型（枚举值） */
} cpu_info_t;

/* 内存区域类型 */
typedef enum {
    MEM_TYPE_USABLE,           /* 可用内存 */
    MEM_TYPE_RESERVED,         /* 保留内存 */
    MEM_TYPE_ACPI_RECLAIMABLE, /* ACPI可回收 */
    MEM_TYPE_ACPI_NVS,         /* ACPI NVS */
    MEM_TYPE_UNUSABLE,         /* 不可用 */
    MEM_TYPE_COUNT
} mem_type_t;

/* 内存区域 */
typedef struct mem_region {
    u64 base;                  /* 基地址 */
    u64 length;                /* 长度 */
    u32 type;                  /* 类型 (mem_type_t) */
    u32 acpi_attr;             /* ACPI属性 */
} mem_region_t;

/* 内存拓扑信息 */
typedef struct memory_topology {
    u64 total_usable;          /* 总可用内存 */
    u64 total_physical;        /* 总物理内存 */
    u32 region_count;          /* 区域数量 */
    mem_region_t regions[128]; /* 内存区域表 */
} memory_topology_t;

/* 设备类型（架构无关） */
typedef enum {
    DEVICE_TYPE_PCI,
    DEVICE_TYPE_PLATFORM,
    DEVICE_TYPE_IRQ_CONTROLLER,
    DEVICE_TYPE_TIMER,
    DEVICE_TYPE_SERIAL,
    DEVICE_TYPE_DISPLAY,
    DEVICE_TYPE_STORAGE,
    DEVICE_TYPE_NETWORK,
    DEVICE_TYPE_UNKNOWN
} device_type_t;

/* PCI设备类型 */
typedef enum {
    PCI_CLASS_STORAGE,
    PCI_CLASS_NETWORK,
    PCI_CLASS_DISPLAY,
    PCI_CLASS_MULTIMEDIA,
    PCI_CLASS_BRIDGE,
    PCI_CLASS_SERIAL,
    PCI_CLASS_SYSTEM,
    PCI_CLASS_INPUT,
    PCI_CLASS_UNKNOWN
} pci_class_t;

/* 设备信息（架构无关） */
typedef struct device {
    device_type_t type;        /* 设备类型 */
    u16 vendor_id;             /* 厂商ID */
    u16 device_id;             /* 设备ID */
    u16 class_code;            /* 类代码 */
    u8 revision;               /* 版本号 */
    u64 base_address;          /* 基地址 */
    u64 address_size;          /* 地址空间大小 */
    u8 irq;                    /* IRQ号 */
    char name[64];             /* 设备名称 */
    
    /* 架构特定数据 */
    union {
        struct {
            u8 bus;            /* 总线号 */
            u8 device;         /* 设备号 */
            u8 function;       /* 功能号 */
            u32 bar[6];        /* 基址寄存器 */
            u32 bar_size[6];   /* BAR大小 */
        } pci;
        struct {
            u64 address;       /* 平台设备地址 */
            u32 id;            /* 设备ID */
        } platform;
    };
} device_t;

/* 设备列表 */
typedef struct device_list {
    u32 device_count;          /* 设备数量 */
    device_t devices[256];
} device_list_t;

/* 中断控制器信息（架构无关） */
typedef struct interrupt_controller {
    u64 base_address;          /* 控制器基地址 */
    u32 irq_base;              /* IRQ基地址 */
    u32 num_irqs;              /* IRQ数量 */
    u8 enabled;                /* 是否启用 */
    char name[32];             /* 控制器名称 */
} interrupt_controller_t;

/* 硬件探测结果 */
typedef struct hardware_probe_result {
    cpu_info_t cpu;            /* CPU信息 */
    memory_topology_t memory;  /* 内存拓扑 */
    device_list_t devices;     /* 设备列表 */
    interrupt_controller_t local_irq;  /* 本地中断控制器 */
    interrupt_controller_t io_irq;     /* I/O中断控制器 */
    u8 smp_enabled;            /* SMP是否启用 */
} hardware_probe_result_t;

/* 外部API声明（由探测模块实现） */

/**
 * 探测CPU信息
 */
hik_status_t probe_cpu(cpu_info_t *result);

/**
 * 探测内存拓扑
 */
hik_status_t probe_memory_topology(const hik_mem_entry_t *boot_mem_map,
                                   u32 mem_map_count,
                                   memory_topology_t *result);

/**
 * 探测PCI设备
 */
hik_status_t probe_pci_devices(device_list_t *result);

/**
 * 探测中断控制器
 */
hik_status_t probe_interrupt_controller(interrupt_controller_t *local,
                                        interrupt_controller_t *io);

/**
 * 初始化硬件探测模块
 */
void hardware_probe_init(void);

/**
 * 执行完整的硬件探测
 */
hik_status_t hardware_probe_all(hardware_probe_result_t *result);

/* 架构特定的CPUID/MSR操作 */
static inline u32 arch_cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, 
                              u32 *ecx, u32 *edx) {
#if CURRENT_ARCH == ARCH_X86_64
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
    return *eax;
#else
    *eax = *ebx = *ecx = *edx = 0;
    return 0;
#endif
}

static inline u64 arch_rdmsr(u32 msr) {
#if CURRENT_ARCH == ARCH_X86_64
    u32 low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((u64)high << 32) | low;
#else
    return 0;
#endif
}

static inline void arch_wrmsr(u32 msr, u64 value) {
#if CURRENT_ARCH == ARCH_X86_64
    u32 low = value & 0xFFFFFFFF;
    u32 high = value >> 32;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(msr));
#endif
}

/* IO端口操作（仅x86） */
static inline u8 arch_inb(u16 port) {
#if CURRENT_ARCH == ARCH_X86_64
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
#else
    (void)port;
    return 0xFF;
#endif
}

static inline void arch_outb(u16 port, u8 value) {
#if CURRENT_ARCH == ARCH_X86_64
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
#else
    (void)port;
    (void)value;
#endif
}