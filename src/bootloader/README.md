# HIC UEFI Bootloader

HIC (Hierarchical Isolation Core) 的第一引导层 - UEFI引导加载程序。

## 项目结构

```
bootloader/
├── include/           # 头文件
│   ├── efi.h         # UEFI定义和结构
│   ├── boot_info.h   # HIC启动信息结构
│   ├── kernel_image.h # HIC内核映像格式
│   ├── crypto.h      # 加密算法（SHA-384, RSA）
│   ├── string.h      # 字符串操作
│   ├── console.h     # 控制台输出
│   └── stdlib.h      # 标准库函数
├── src/              # 源代码
│   ├── main.c        # 主程序和UEFI入口
│   ├── console.c     # 控制台实现
│   ├── string.c      # 字符串函数
│   ├── stdlib.c      # 标准库函数
│   └── crypto/       # 加密算法实现
│       ├── sha384.c         # SHA-384哈希
│       ├── rsa.c            # RSA签名验证
│       └── image_verify.c   # 内核映像验证
├── lib/              # 外部库（如有）
├── tools/            # 构建工具
├── Makefile          # 构建系统
└── README.md         # 本文件
```

## 功能特性

### 1. UEFI环境初始化
- 初始化UEFI系统表和引导服务
- 初始化控制台输出
- 获取加载的映像协议

### 2. 配置文件加载
- 从`\EFI\HIC\boot.conf`读取引导配置
- 支持内核路径、命令行、超时等配置

### 3. 内核映像加载
- 从文件系统加载HIC内核映像
- 支持FAT32/EXT2等文件系统
- 解析内核映像头部和段表

### 4. 签名验证
- SHA-384哈希计算
- RSA-3072签名验证（PKCS#1 v2.1 PSS）
- Ed25519签名验证
- 支持公钥管理和安全启动

### 5. 内存映射获取
- 获取UEFI内存映射
- 转换为HIC格式
- 保留内核所需内存区域

### 6. 启动信息结构(BIS)
- 构建标准化的启动信息
- 包含内存映射、ACPI表、内核入口等
- 传递给内核Core-0

### 7. 调试支持
- VGA文本模式输出
- 串口调试输出（COM1/COM2）
- 日志级别控制
- 内存日志缓冲区

## 构建方法

### 前置要求

1. **交叉编译工具链**
   ```bash
   # Ubuntu/Debian
   sudo apt install gcc-mingw-w64-x86-64
   
   # Arch Linux
   sudo pacman -S mingw-w64-gcc
   ```

2. **OVMF固件**（用于QEMU测试）
   ```bash
   # Ubuntu/Debian
   sudo apt install ovmf
   
   # Arch Linux
   sudo pacman -S edk2-ovmf
   ```

### 编译

```bash
# 进入bootloader目录
cd bootloader

# 编译
make clean
make all

# 输出文件
# bin/bootx64.efi - UEFI引导加载程序
```

### 安装

```bash
# 挂载EFI分区
sudo mount /dev/sda1 /mnt/efi

# 安装到EFI分区
sudo make install EFI_MOUNT=/mnt/efi
```

## 测试方法

### QEMU测试（文本模式）

```bash
# 创建测试磁盘
mkdir -p test_disk/EFI/HIC
cp bin/bootx64.efi test_disk/EFI/HIC/

# 在QEMU中运行
make qemu
```

### QEMU测试（图形模式）

```bash
make qemu-gui
```

### 创建USB启动盘

```bash
# 创建USB镜像
make usb-image USB_IMAGE=hic-boot.img

# 写入USB设备
sudo dd if=hic-boot.img of=/dev/sdX bs=1M status=progress
```

## 配置文件示例

`boot.conf` 文件格式：

```
# HIC Bootloader Configuration

# 内核路径
kernel=\EFI\HIC\kernel.hic

# 启动参数
cmdline=quiet debug=1

# 启动超时（秒）
timeout=5

# 调试输出
debug=1
```

## HIC内核映像格式

### 映像头部

```c
typedef struct {
    char     magic[8];              // "HIC_IMG"
    uint16_t arch_id;               // 1=x86_64
    uint16_t version;               // 版本号
    uint64_t entry_point;           // 入口点偏移
    uint64_t image_size;            // 映像总大小
    uint64_t segment_table_offset;  // 段表偏移
    uint64_t segment_count;         // 段表项数
    uint64_t config_table_offset;   // 配置表偏移
    uint64_t config_table_size;     // 配置表大小
    uint64_t signature_offset;      // 签名偏移
    uint64_t signature_size;        // 签名大小
    uint8_t  reserved[64];          // 预留
} hic_image_header_t;
```

### 段类型

- `HIC_SEGMENT_TYPE_CODE`    - 代码段
- `HIC_SEGMENT_TYPE_DATA`    - 数据段
- `HIC_SEGMENT_TYPE_RODATA`  - 只读数据段
- `HIC_SEGMENT_TYPE_BSS`     - BSS段
- `HIC_SEGMENT_TYPE_CONFIG`  - 配置段

## 启动流程

```
1. UEFI固件
   ↓
2. bootx64.efi (本引导加载程序)
   - 初始化UEFI环境
   - 加载配置文件
   - 加载内核映像
   - 验证签名
   - 准备启动信息
   ↓
3. 退出UEFI启动服务
   ↓
4. 跳转到内核入口点
   - 传递启动信息结构
   ↓
5. HIC Core-0内核
```

## 安全特性

### 1. 签名验证
- RSA-3072 + SHA-384
- Ed25519 + SHA-512
- 支持公钥证书链

### 2. 安全启动
- 支持UEFI安全启动
- 平台密钥(PK)管理
- 内核签名密钥(KSK)
- 恢复密钥(RK)

### 3. 内存保护
- 独立的物理内存区域
- MMU页表隔离
- 能力系统授权

## 调试

### 串口输出

```bash
# 查看串口输出
sudo minicom -D /dev/ttyS0

# 或使用screen
sudo screen /dev/ttyS0 115200
```

### 日志级别

```c
log_set_level(LOG_LEVEL_DEBUG);  // 设置调试级别

// 日志输出
log_error("Error message\n");
log_warn("Warning message\n");
log_info("Info message\n");
log_debug("Debug message\n");
log_trace("Trace message\n");
```

## 故障排查

### 编译错误

1. 确保已安装mingw-w64交叉编译工具链
2. 检查CFLAGS是否正确
3. 确保所有头文件路径正确

### 运行时错误

1. 检查OVMF固件是否正确安装
2. 确认内核映像路径正确
3. 查看串口输出获取详细错误信息
4. 检查内存映射是否正确

### 签名验证失败

1. 确认公钥正确配置
2. 检查签名算法是否匹配
3. 验证内核映像完整性
4. 检查时间戳和版本号

## 开发路线图

### 已完成
- [x] UEFI环境初始化
- [x] 配置文件加载
- [x] 内核映像加载
- [x] SHA-384哈希实现
- [x] 基础签名验证框架
- [x] 内存映射获取
- [x] 启动信息结构
- [x] 控制台输出
- [x] 串口调试

### 待完成
- [ ] 完整的RSA-3072实现
- [ ] 完整的Ed25519实现
- [ ] 多重引导支持
- [ ] 图形模式支持
- [ ] 网络启动（PXE/TFTP）
- [ ] A/B分区更新
- [ ] 恢复模式
- [ ] TPM 2.0集成

## 贡献指南

1. 遵循代码风格规范
2. 添加必要的注释
3. 确保代码可编译
4. 测试所有修改
5. 更新文档

## 许可证

本项目遵循 HIC 项目许可证。

## 联系方式

- 作者：DslsDZC
- 邮箱：dsls.dzc@gmail.com
- 项目：https://github.com/DslsDZC/HIC

## 参考文献

- [UEFI Specification](https://uefi.org/specifications)
- [Platform Initialization (PI) Specification](https://uefi.org/specifications)
- [PKCS#1 v2.1: RSA Cryptography Standard](https://www.rfc-editor.org/rfc/rfc3447)
- [RFC 8032: Ed25519](https://www.rfc-editor.org/rfc/rfc8032)
- [FIPS 180-4: SHA Standard](https://csrc.nist.gov/publications/detail/fips/180/4/final)