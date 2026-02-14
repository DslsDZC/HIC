# x86_64引导加载程序设计

基于HIK架构需求，以下是x86_64平台引导加载程序的详细设计方案：

## 一、总体架构

### 1.1 多阶段引导流程
```
UEFI/BIOS → HIK Bootloader (多阶段) → HIK Core-0
    │               │
    ├─ UEFI模式     ├─ BIOS模式
    │   (UEFI应用)   │   (MBR+引导扇区)
    └─ 安全启动      └─ 传统启动

## 三、BIOS引导模式（传统兼容）

### 3.1 多阶段设计
```
主引导记录(MBR) → 第一阶段加载器 → 第二阶段加载器 → 内核
    (512字节)     (1-2KB)          (16-32KB)
```

### 3.2 第一阶段（MBR/VBR）

**MBR结构（实模式）：**
```
0x0000: 引导代码 (446字节)
0x01BE: 分区表 (64字节)
0x01FE: 签名 (0x55AA)
```

**执行流程：**
1. BIOS加载MBR到`0x7C00`
2. 切换到保护模式（32位）
3. 从活动分区加载VBR（卷引导记录）
4. VBR加载第二阶段加载器到`0x10000`

### 3.3 第二阶段加载器（32位保护模式）

**主要功能：**
1. **初始化保护模式**
   - 设置GDT、IDT
   - 启用A20地址线
   - 设置临时页表

2. **探测硬件**
   - 通过BIOS INT 0x15获取内存映射
   - 探测VESA VBE显示模式
   - 初始化PS/2键盘控制器

3. **加载内核映像**
   - 从文件系统（FAT32/EXT2）读取
   - 支持从硬盘、USB、光盘启动
   - 实现简单的文件系统驱动

4. **验证内核**
   - 计算映像哈希（SHA-256）
   - 与内置公钥比较（简化验证）

5. **切换到长模式**
   - 设置4级分页
   - 进入64位长模式
   - 加载64位GDT

6. **传递信息**
   - 构建Multiboot2兼容信息结构
   - 包含内存映射、命令行、模块信息

7. **跳转到内核**
   - 设置栈指针
   - 禁用中断
   - 跳转到内核入口点

## 四、引导信息结构（Boot Information Structure）

### 4.1 通用结构（UEFI和BIOS通用）
```c
struct hik_boot_info {
    uint32_t magic;           // 0x48 0x49 0x4B 0x21 ("HIK!")
    uint32_t version;         // 结构版本
    uint64_t flags;           // 特性标志位
    
    // 内存信息
    struct memory_map *mem_map;
    uint64_t mem_map_size;
    uint64_t mem_map_desc_size;
    
    // ACPI信息
    void *rsdp;              // ACPI根系统描述指针
    void *xsdp;              // ACPI扩展系统描述指针（UEFI）
    
    // 固件信息
    union {
        struct {
            EFI_SYSTEM_TABLE *system_table;
            EFI_HANDLE image_handle;
        } uefi;
        struct {
            void *bios_data_area;  // BDA指针
            uint32_t vbe_info;     // VESA信息块
        } bios;
    } firmware;
    
    // 内核映像信息
    void *kernel_base;
    uint64_t kernel_size;
    uint64_t entry_point;
    
    // 命令行
    char cmdline[256];
    
    // 设备树（x86通常为空）
    void *device_tree;
    uint64_t device_tree_size;
    
    // 模块信息（用于动态模块加载）
    struct module_info *modules;
    uint64_t module_count;
};
```

### 4.2 内存映射描述符
```c
struct memory_map_entry {
    uint64_t base_address;
    uint64_t length;
    uint32_t type;           // 1=可用, 2=保留, 3=ACPI, 4=NVS, 5=坏内存
    uint32_t attributes;
};
```

## 五、内核映像格式

### 5.1 文件头结构
```
偏移量   大小    描述
0x00     4      魔数 "HIK_IMG"
0x04     2      架构ID (1=x86_64)
0x06     2      版本 (主版本.次版本)
0x08     8      入口点偏移
0x10     8      映像总大小
0x18     8      段表偏移
0x20     8      段表项数
0x28     8      配置表偏移
0x30     8      配置表大小
0x38     8      签名偏移
0x40     8      签名大小
0x48     64     预留
```

### 5.2 段表项
```c
struct segment_entry {
    uint32_t type;      // 1=代码, 2=数据, 3=只读数据, 4=BSS
    uint32_t flags;     // 加载标志
    uint64_t file_offset;
    uint64_t memory_offset;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
};
```

## 六、安全启动实现

### 6.1 签名验证流程
```
加载映像 → 计算哈希 → 提取签名 → 验证签名 → 加载内核
    │          │           │          │
    │          SHA-384     │          RSA-3072验证
    │                      │
    │                 PKCS#1 v2.1签名
    └───────── 公钥证书链验证 ────────┘
```

### 6.2 密钥管理
- **平台密钥(PK)**：存储在UEFI固件或TPM
- **内核签名密钥(KSK)**：用于签名内核映像
- **恢复密钥(RK)**：用于恢复模式
- **开发密钥(DK)**：仅用于开发构建

## 七、恢复与调试支持

### 7.1 恢复模式
- **内核故障检测**：通过启动计数器检测连续失败
- **自动回滚**：加载备份内核映像
- **恢复控制台**：通过串口或网络提供诊断接口
- **网络恢复**：通过TFTP/PXE重新下载内核

### 7.2 调试接口
- **串口输出**：COM1 (0x3F8) 或 COM2 (0x2F8)
- **内存日志缓冲区**：固定物理地址的循环缓冲区
- **QEMU/GDB支持**：通过0xE9端口输出和调试器接口
- **错误代码显示**：通过VGA文本模式显示错误信息

## 八、构建系统集成

### 8.1 构建流程
```
1. 编译引导加载程序
   $ make bootloader ARCH=x86_64 MODE=uefi
   
2. 生成内核映像
   $ make kernel
   
3. 签名内核映像
   $ make sign-kernel KEY=production
   
4. 创建启动介质
   $ make install TARGET=usb DISK=/dev/sdb
```

### 8.2 输出文件
- **UEFI**：`bootx64.efi`（引导加载程序） + `kernel.hik`（内核）
- **BIOS**：`boot.bin`（引导扇区） + `loader.bin`（第二阶段）
- **ISO映像**：`hik-x86_64.iso`（用于虚拟机测试）

## 九、测试与验证

### 9.1 测试环境
- **QEMU/KVM**：用于虚拟化测试
- **物理硬件**：Intel/AMD服务器和工作站
- **CI/CD集成**：自动化构建和启动测试

### 9.2 验证要点
1. **启动时间**：从UEFI到内核入口的总时间
2. **内存占用**：引导加载程序的内存使用量
3. **兼容性**：在不同硬件平台的兼容性
4. **安全性**：签名验证的可靠性
