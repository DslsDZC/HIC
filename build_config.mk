# HIK内核编译时配置示例
# 通过修改此文件并重新编译来调整内核行为

# ===== 编译时配置选项 =====

# 调试配置
CONFIG_DEBUG ?= 0          # 启用调试支持 (0=禁用, 1=启用)
CONFIG_TRACE ?= 0          # 启用跟踪功能 (0=禁用, 1=启用)
CONFIG_VERBOSE ?= 0        # 启用详细输出 (0=禁用, 1=启用)

# 安全配置
CONFIG_KASLR ?= 1          # 启用内核地址空间布局随机化 (0=禁用, 1=启用)
CONFIG_SMEP ?= 1           # 启用SMEP (禁止从用户态执行内核代码) (0=禁用, 1=启用)
CONFIG_SMAP ?= 1           # 启用SMAP (禁止内核访问用户态内存) (0=禁用, 1=启用)
CONFIG_AUDIT ?= 1          # 启用审计日志 (0=禁用, 1=启用)

# 性能配置
CONFIG_PERF ?= 1           # 启用性能计数器 (0=禁用, 1=启用)
CONFIG_FAST_PATH ?= 1      # 启用快速路径优化 (0=禁用, 1=启用)

# 功能配置
CONFIG_PCI ?= 1            # 启用PCI支持 (0=禁用, 1=启用)
CONFIG_ACPI ?= 1           # 启用ACPI支持 (0=禁用, 1=启用)
CONFIG_SERIAL ?= 1         # 启用串口支持 (0=禁用, 1=启用)

# 内存配置
CONFIG_HEAP_SIZE_MB ?= 128 # 堆大小(MB)
CONFIG_STACK_SIZE_KB ?= 8  # 栈大小(KB)
CONFIG_PAGE_CACHE_PERCENT ?= 20  # 页面缓存百分比

# 调度器配置
CONFIG_SCHEDULER_POLICY ?= priority  # 调度策略 (fifo, rr, priority)
CONFIG_TIME_SLICE_MS ?= 10           # 时间片长度(毫秒)
CONFIG_MAX_THREADS ?= 256            # 最大线程数

# 安全级别
CONFIG_SECURITY_LEVEL ?= standard  # 安全级别 (minimal, standard, strict)

# 能力系统配置
CONFIG_MAX_CAPABILITIES ?= 65536  # 最大能力数量
CONFIG_CAPABILITY_DERIVATION ?= 1  # 启用能力派生 (0=禁用, 1=启用)

# 域配置
CONFIG_MAX_DOMAINS ?= 16          # 最大域数量
CONFIG_DOMAIN_STACK_SIZE_KB ?= 16 # 域栈大小(KB)

# 中断配置
CONFIG_MAX_IRQS ?= 256          # 最大中断数
CONFIG_IRQ_FAIRNESS ?= 1        # 启用中断公平性 (0=禁用, 1=启用)

# 模块配置
CONFIG_MODULE_LOADING ?= 0      # 启用模块加载 (0=禁用, 1=启用)
CONFIG_MAX_MODULES ?= 32        # 最大模块数

# ===== 使用说明 =====

# 编译时配置说明：
#
# 1. 编译时配置在编译阶段确定，需要重新编译才能生效
# 2. 这些配置影响内核的代码生成和功能特性
# 3. 编译时配置通常用于：
#    - 硬编码的安全策略
#    - 性能优化选项
#    - 功能开关
#    - 内存布局调整
#
# 使用方法：
#
# 方法1：直接修改此文件中的配置值
#   make clean && make
#
# 方法2：通过命令行参数传递
#   make CONFIG_DEBUG=1 CONFIG_TRACE=1
#
# 方法3：创建自定义配置文件
#   cp build_config.mk my_config.mk
#   编辑 my_config.mk
#   make -f my_config.mk
#
# ===== 与运行时配置的区别 =====

# 编译时配置 vs 运行时配置：
#
# 编译时配置：
# - 在编译阶段确定
# - 影响代码生成和二进制大小
# - 需要重新编译才能修改
# - 适用于：安全策略、性能优化、功能开关
#
# 运行时配置：
# - 通过引导层参数传递
# - 可在启动时调整，无需重新编译
# - 适用于：日志级别、调试选项、参数调整
#
# 配置优先级：
# 1. 编译时配置（最高优先级，不可覆盖）
# 2. 运行时配置（通过引导层传递）
# 3. 默认配置（代码中的默认值）

# ===== 配置验证 =====

# 编译前会自动验证配置的一致性
# 如果配置不合法，构建会失败并显示错误信息

# 配置验证规则：
# - CONFIG_HEAP_SIZE_MB 必须在 16-4096 范围内
# - CONFIG_STACK_SIZE_KB 必须在 4-64 范围内
# - CONFIG_MAX_THREADS 必须在 1-1024 范围内
# - CONFIG_TIME_SLICE_MS 必须在 1-1000 范围内
# - CONFIG_PAGE_CACHE_PERCENT 必须在 0-50 范围内