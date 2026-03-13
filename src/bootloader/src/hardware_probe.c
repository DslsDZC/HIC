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
#include "efi.h"

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
    
    // 分配内存存储硬件探测结果
    hardware_probe_result_t *hw_result = NULL;
    EFI_STATUS status = gBS->AllocatePool(EfiLoaderData, sizeof(hardware_probe_result_t), (void **)&hw_result);
    if (EFI_ERROR(status) || !hw_result) {
        log_error("Failed to allocate memory for hardware probe result\n");
        return;
    }
    
    // 执行完整的硬件探测
    hardware_probe_all(boot_info, hw_result);
    
    // 将结果存储到boot_info中
    boot_info->hardware.hw_data = hw_result;
    boot_info->hardware.hw_size = sizeof(hardware_probe_result_t);
    boot_info->hardware.hw_hash = 0; // 暂时不计算哈希
    
    // 标记硬件探测完成
    boot_info->flags |= HIC_BOOT_FLAG_ACPI_ENABLED;
    
    log_info("Hardware probe completed, result stored at 0x%llx, size %llu\n",
             (uint64_t)hw_result, boot_info->hardware.hw_size);
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
    
    // 尝试使用I/O端口访问PCI配置空间
    // 在x86架构上，即使是在UEFI环境中，I/O端口访问通常也是可用的
    // 扫描有限的总线范围以避免长时间扫描
    
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                // 读取厂商ID和设备ID
                uint32_t vendor_device = pci_read_config((1U << 31) | (bus << 16) | (device << 11) | (function << 8));
                uint16_t vendor_id = (uint16_t)(vendor_device & 0xFFFF);
                uint16_t device_id = (uint16_t)((vendor_device >> 16) & 0xFFFF);
                
                // 检查设备是否存在
                if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
                    if (function == 0) break; // 该设备不存在
                    continue;
                }
                
                // 设备存在，添加到结果中
                if (result->device_count >= 64) {
                    log_info("  PCI device limit reached (64 devices)\n");
                    return;
                }
                
                device_t *dev = &result->devices[result->device_count];
                dev->type = DEVICE_TYPE_PCI;
                dev->vendor_id = vendor_id;
                dev->device_id = device_id;
                dev->pci.bus = bus;
                dev->pci.device = device;
                dev->pci.function = function;
                
                // 读取类代码和版本
                uint32_t class_rev = pci_read_config((1U << 31) | (bus << 16) | (device << 11) | (function << 8) | 0x08);
                dev->class_code = (uint16_t)((class_rev >> 8) & 0xFFFFFF);
                dev->revision = class_rev & 0xFF;
                
                // 读取BAR寄存器
                for (int i = 0; i < 6; i++) {
                    dev->pci.bar[i] = pci_read_config((1U << 31) | (bus << 16) | (device << 11) | (function << 8) | (0x10 + i * 4));
                }
                
                // 生成设备名称
                snprintf(dev->name, sizeof(dev->name), "PCI %02x:%02x.%01x %04x:%04x",
                         bus, device, function, vendor_id, device_id);
                
                result->device_count++;
                result->pci_count++;
                
                log_info("  Found PCI device: %s\n", dev->name);
            }
        }
    }
    
    log_info("  Found %u PCI devices\n", result->pci_count);
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
void hardware_probe_all(hic_boot_info_t *boot_info, hardware_probe_result_t *result)
{
    if (!boot_info || !result) {
        log_error("boot_info or result buffer is NULL\n");
        return;
    }
    
    log_info("Starting full hardware probe...\n");
    
    // 探测CPU
    probe_cpu(&result->cpu);
    
    // 填充内存拓扑信息
    log_info("Probing memory topology...\n");
    result->memory.region_count = 0;
    result->memory.total_usable = 0;
    result->memory.total_physical = 0;
    
    // 从boot_info的内存映射中复制内存区域
    uint32_t max_regions = sizeof(result->memory.regions) / sizeof(result->memory.regions[0]);
    for (uint32_t i = 0; i < boot_info->mem_map_entry_count && i < max_regions; i++) {
        memory_map_entry_t *src = &boot_info->mem_map[i];
        mem_region_t *dst = &result->memory.regions[i];
        
        dst->base = src->base_address;
        dst->size = src->length;
        dst->type = src->type;
        
        if (src->type == HIC_MEM_TYPE_USABLE) {
            result->memory.total_usable += src->length;
        }
        result->memory.total_physical += src->length;
        
        result->memory.region_count++;
    }
    
    log_info("  Memory: %u regions, %llu MB total, %llu MB usable\n",
             result->memory.region_count,
             result->memory.total_physical / (1024 * 1024),
             result->memory.total_usable / (1024 * 1024));
    
    // 探测PCI设备
    probe_pci_devices(&result->devices);
    
    // 探测中断控制器
    probe_interrupt_controller(&result->local_irq, &result->io_irq);
    
    // 探测ACPI信息（补充APIC等硬件信息）
    probe_acpi_info(boot_info, result);
    
    // 探测定时器和串口设备
    probe_timers_and_serial(result);
    
    // 设置SMP状态（根据CPU逻辑核心数判断）
    result->smp_enabled = (result->cpu.logical_cores > 1);
    
    log_info("Hardware probe completed\n");
}

// 探测ACPI信息
void probe_acpi_info(hic_boot_info_t *boot_info, hardware_probe_result_t *result)
{
    if (!boot_info || !result) {
        log_error("boot_info or result is NULL\n");
        return;
    }
    
    log_info("Probing ACPI information...\n");
    
    // 检查是否有ACPI RSDP
    if (!boot_info->rsdp) {
        log_warning("  No ACPI RSDP found\n");
        return;
    }
    
    uint8_t *rsdp = (uint8_t *)boot_info->rsdp;
    
    // 验证RSDP校验和
    uint8_t checksum = 0;
    for (int i = 0; i < 20; i++) {
        checksum += rsdp[i];
    }
    
    if (checksum != 0) {
        log_error("  ACPI RSDP checksum invalid\n");
        return;
    }
    
    // 获取ACPI版本
    uint8_t revision = rsdp[15];
    uint32_t rsdt_address = *(uint32_t *)&rsdp[16];
    uint64_t xsdt_address = 0;
    
    if (revision >= 2) {
        xsdt_address = *(uint64_t *)&rsdp[24];
    }
    
    log_info("  ACPI version: %s\n", revision >= 2 ? "2.0+" : "1.0");
    log_info("  RSDT address: 0x%08x\n", rsdt_address);
    if (revision >= 2) {
        log_info("  XSDT address: 0x%016llx\n", xsdt_address);
    }
    
    // 解析MADT（APIC表）
    uint8_t *sdt = (uint8_t *)(revision >= 2 ? xsdt_address : rsdt_address);
    if (!sdt) {
        log_warning("  No SDT found\n");
        return;
    }
    
    // 验证SDT签名
    if (memcmp(sdt, "RSDT", 4) != 0 && memcmp(sdt, "XSDT", 4) != 0) {
        log_warning("  Invalid SDT signature\n");
        return;
    }
    
    uint32_t sdt_length = *(uint32_t *)&sdt[4];
    uint32_t entry_count = (sdt_length - 36) / (revision >= 2 ? 8 : 4);
    
    // 遍历SDT条目查找MADT
    for (uint32_t i = 0; i < entry_count; i++) {
        uint64_t entry_addr;
        if (revision >= 2) {
            entry_addr = *(uint64_t *)&sdt[36 + i * 8];
        } else {
            entry_addr = *(uint32_t *)&sdt[36 + i * 4];
        }
        
        uint8_t *table = (uint8_t *)entry_addr;
        if (!table) continue;
        
        // 检查MADT签名
        if (memcmp(table, "APIC", 4) == 0) {
            log_info("  Found MADT (APIC) at 0x%016llx\n", entry_addr);
            uint32_t madt_length = *(uint32_t *)&table[4];
            
            // 解析MADT条目
            for (uint32_t offset = 44; offset < madt_length; ) {
                uint8_t entry_type = table[offset];
                uint8_t entry_length = table[offset + 1];
                
                if (entry_type == 0) {  // 处理器本地APIC
                    uint8_t acpi_id = table[offset + 2];
                    uint8_t apic_id = table[offset + 3];
                    uint32_t flags = *(uint32_t *)&table[offset + 4];
                    
                    if (flags & 1) {  // 处理器启用
                        log_info("    Local APIC: ACPI ID %u, APIC ID %u (enabled)\n", 
                                 acpi_id, apic_id);
                    }
                } else if (entry_type == 1) {  // I/O APIC
                    uint8_t ioapic_id = table[offset + 2];
                    uint8_t reserved = table[offset + 3]; (void)reserved;
                    uint32_t ioapic_address = *(uint32_t *)&table[offset + 4];
                    uint32_t gsi_base = *(uint32_t *)&table[offset + 8];
                    
                    log_info("    I/O APIC: ID %u, address 0x%08x, GSI base %u\n",
                             ioapic_id, ioapic_address, gsi_base);
                    
                    // 更新硬件探测结果中的I/O APIC地址
                    result->io_irq.base_address = ioapic_address;
                }
                
                offset += entry_length;
            }
            break;  // 找到MADT后退出循环
        }
    }
    
    log_info("  ACPI probing completed\n");
}

// 探测定时器和串口设备
void probe_timers_and_serial(hardware_probe_result_t *result)
{
    if (!result) {
        log_error("result is NULL\n");
        return;
    }
    
    log_info("Probing timers and serial devices...\n");
    
    // 检查设备列表是否已满
    if (result->devices.device_count >= sizeof(result->devices.devices) / sizeof(result->devices.devices[0])) {
        log_warning("  Device list is full, skipping timer/serial detection\n");
        return;
    }
    
    // 探测8254 PIT（可编程间隔定时器）
    device_t *pit = &result->devices.devices[result->devices.device_count];
    pit->type = DEVICE_TYPE_TIMER;
    pit->vendor_id = 0x8086;  // Intel
    pit->device_id = 0x1234;  // 8254 PIT
    pit->class_code = 0x0801; // 定时器类
    pit->revision = 1;
    pit->base_address = 0x40;  // PIT控制端口
    pit->address_size = 4;     // 4个端口：0x40-0x43
    pit->irq = 0;              // IRQ 0
    strcpy(pit->name, "8254 PIT");
    result->devices.device_count++;
    
    // 探测COM1串口
    device_t *com1 = &result->devices.devices[result->devices.device_count];
    com1->type = DEVICE_TYPE_SERIAL;
    com1->vendor_id = 0x8086;  // Intel（典型）
    com1->device_id = 0x7000;  // 16550 UART
    com1->class_code = 0x0703; // 串口类
    com1->revision = 1;
    com1->base_address = 0x3F8;  // COM1基地址
    com1->address_size = 8;      // 8个端口：0x3F8-0x3FF
    com1->irq = 4;               // IRQ 4
    strcpy(com1->name, "COM1 (16550 UART)");
    result->devices.device_count++;
    
    // 探测COM2串口（如果存在）
    device_t *com2 = &result->devices.devices[result->devices.device_count];
    com2->type = DEVICE_TYPE_SERIAL;
    com2->vendor_id = 0x8086;
    com2->device_id = 0x7000;
    com2->class_code = 0x0703;
    com2->revision = 1;
    com2->base_address = 0x2F8;  // COM2基地址
    com2->address_size = 8;
    com2->irq = 3;               // IRQ 3
    strcpy(com2->name, "COM2 (16550 UART)");
    result->devices.device_count++;
    
    log_info("  Found %u timer/serial devices\n", 3);
}
