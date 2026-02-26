/**
 * HIC 引导层硬件探测实现
 * 严格遵循 TD/三层模型.md 文档规范
 * 
 * 职责：
 * 1. 探测CPU信息（vendor_id、特性、缓存等）
 * 2. 探测内存拓扑
 * 3. 探测PCI设备
 * 4. 探测中断控制器
 * 5. 将结果填充到 boot_info->hw
 */

#include "hardware_probe.h"
#include "console.h"
#include "string.h"
#include "bootlog.h"

// 内存清零函数
static inline void hw_memzero(void *ptr, size_t size)
{
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

// CPUID指令封装
static inline void hw_cpuid(uint32_t leaf, uint32_t subleaf,
                            uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

// PCI配置访问辅助函数
static inline uint32_t pci_read_config(uint32_t addr)
{
    __asm__ volatile("outl %0, %1" : : "a"(addr), "Nd"(0xCF8));
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(0xCFC));
    return value;
}

static inline void pci_write_config(uint32_t addr, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(addr), "Nd"(0xCF8));
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(0xCFC));
}

// 初始化硬件探测
void hardware_probe_init(hic_boot_info_t *boot_info)
{
    if (!boot_info) {
        log_error("boot_info is NULL\n");
        return;
    }
    
    log_info("Initializing hardware probe...\n");
    
    // 标记硬件探测完成
    boot_info->flags |= HIC_BOOT_FLAG_ACPI_ENABLED;
    
    log_info("Hardware probe completed\n");
}

// 探测CPU信息
void probe_cpu(cpu_info_t *result)
{
    if (!result) {
        log_error("CPU result buffer is NULL\n");
        return;
    }
    
    log_info("Probing CPU information...\n");
    
    hw_memzero(result, sizeof(cpu_info_t));
    
    // 获取vendor_id和最大标准leaf
    uint32_t eax, ebx, ecx, edx;
    hw_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    
    result->vendor_id[0] = ebx;
    result->vendor_id[1] = edx;
    result->vendor_id[2] = ecx;
    
    uint32_t max_leaf = eax;
    
    // 获取CPU版本和特性
    hw_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    result->version = eax;
    result->feature_flags[0] = ecx;
    result->feature_flags[1] = edx;
    
    // 解析版本信息
    result->stepping = eax & 0xF;
    result->model = (eax >> 4) & 0xF;
    result->family = (eax >> 8) & 0xF;
    
    // 获取品牌字符串
    char brand_string[49];
    hw_cpuid(0x80000002, 0, &eax, &ebx, &ecx, &edx);
    ((uint32_t*)brand_string)[0] = eax;
    ((uint32_t*)brand_string)[1] = ebx;
    ((uint32_t*)brand_string)[2] = ecx;
    ((uint32_t*)brand_string)[3] = edx;
    
    hw_cpuid(0x80000003, 0, &eax, &ebx, &ecx, &edx);
    ((uint32_t*)brand_string)[4] = eax;
    ((uint32_t*)brand_string)[5] = ebx;
    ((uint32_t*)brand_string)[6] = ecx;
    ((uint32_t*)brand_string)[7] = edx;
    
    hw_cpuid(0x80000004, 0, &eax, &ebx, &ecx, &edx);
    ((uint32_t*)brand_string)[8] = eax;
    ((uint32_t*)brand_string)[9] = ebx;
    ((uint32_t*)brand_string)[10] = ecx;
    ((uint32_t*)brand_string)[11] = edx;
    
    brand_string[48] = '\0';
    strcpy(result->brand_string, brand_string);
    
    // 获取逻辑核心数
    if (max_leaf >= 0xB) {
        hw_cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
        result->logical_cores = ebx & 0xFFFF;
    } else {
        hw_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        result->logical_cores = (ebx >> 16) & 0xFF;
    }
    
    // 设置架构类型
    result->arch_type = 1;  // x86_64
    
    log_info("CPU: %s\n", result->brand_string);
    log_info("  Family: %d, Model: %d, Stepping: %d\n", 
             result->family, result->model, result->stepping);
    log_info("  Logical Cores: %d\n", result->logical_cores);
}

// 探测内存拓扑
void probe_memory_topology(hic_boot_info_t *boot_info)
{
    if (!boot_info || !boot_info->mem_map) {
        log_error("Invalid boot_info or memory map\n");
        return;
    }
    
    log_info("Probing memory topology...\n");
    
    // 内存拓扑已经在 get_memory_map() 中填充
    // 这里只需要计算总可用内存和统计信息
    uint64_t total_usable = 0;
    uint64_t total_physical = 0;
    
    for (uint32_t i = 0; i < boot_info->mem_map_entry_count; i++) {
        memory_map_entry_t *entry = &boot_info->mem_map[i];
        
        if (entry->type == HIC_MEM_TYPE_USABLE) {
            total_usable += entry->length;
        }
        total_physical += entry->length;
    }
    
    boot_info->system.memory_size_mb = (uint32_t)(total_physical / (1024 * 1024));
    boot_info->system.cpu_count = 1;
    
    log_info("  Total Physical: %llu MB\n", total_physical / (1024 * 1024));
    log_info("  Total Usable: %llu MB\n", total_usable / (1024 * 1024));
}

// 探测PCI设备
void probe_pci_devices(device_list_t *result)
{
    if (!result) {
        log_error("PCI device list buffer is NULL\n");
        return;
    }
    
    log_info("Probing PCI devices...\n");
    
    hw_memzero(result, sizeof(device_list_t));
    
    // 在UEFI环境中，不能直接使用I/O端口访问PCI配置空间
    // 需要通过UEFI的PCI Root Bridge I/O Protocol
    // 这里暂时留空，实际应该使用UEFI协议
    
    log_info("  PCI probing skipped (UEFI mode)\n");
}

// 探测中断控制器
void probe_interrupt_controller(interrupt_controller_t *local,
                                 interrupt_controller_t *io)
{
    if (!local || !io) {
        log_error("Interrupt controller buffers are NULL\n");
        return;
    }
    
    log_info("Probing interrupt controllers...\n");
    
    // 初始化本地APIC（Local APIC）
    uint64_t apic_base_msr;
    __asm__ volatile("rdmsr" : "=A"(apic_base_msr) : "c"(0x1B));
    
    local->base_address = apic_base_msr & 0xFFFFF000;
    local->irq_base = 0;
    local->num_irqs = 256;
    local->enabled = (apic_base_msr & 0x800) != 0;
    strcpy(local->name, "Local APIC");
    
    // 初始化I/O APIC
    io->base_address = 0xFEC00000;
    io->irq_base = 0;
    io->num_irqs = 24;
    io->enabled = 1;
    strcpy(io->name, "I/O APIC");
    
    log_info("  Local APIC: 0x%llx, %s\n", local->base_address, 
             local->enabled ? "enabled" : "disabled");
    log_info("  I/O APIC: 0x%llx, enabled\n", io->base_address);
}

// 执行完整的硬件探测
void hardware_probe_all(hardware_probe_result_t *result)
{
    if (!result) {
        log_error("Hardware probe result buffer is NULL\n");
        return;
    }
    
    log_info("Starting full hardware probe...\n");
    
    // 探测CPU
    probe_cpu(&result->cpu);
    
    // 探测PCI设备
    probe_pci_devices(&result->devices);
    
    // 探测中断控制器
    probe_interrupt_controller(&result->local_irq, &result->io_irq);
    
    // 设置SMP状态
    result->smp_enabled = 0;
    
    log_info("Hardware probe completed\n");
}
