# HIC 抽象规范 (Abstract Specification)

> 本文档用高阶逻辑描述 HIC 内核"做什么"，定义状态、操作和安全属性，作为形式化验证的基础。

---

## 1. 核心系统抽象

### 1.1 域 (Domain)

**抽象定义：**
```
Domain :: {
  d_id          : domain_id,           -- 唯一标识符
  d_type        : domain_type,         -- Core0 | Privileged | User
  d_caps        : capability_handle set, -- 能力句柄集合
  d_mem_quota   : nat,                 -- 内存配额（字节数）
  d_cpu_quota   : nat,                 -- CPU 时间配额（逻辑核心数 × 时间片）
  d_io_quota    : nat,                 -- I/O 带宽限制
  d_pt_config   : page_table_config,   -- 页表/MPU 配置
  d_privileged  : bool,                -- 特权内存访问通道标记
  d_audit_cap   : option capability_handle, -- 审计域能力
  d_state       : domain_state         -- Created | Running | Suspended | Destroyed
}
```

**不变量：**
- `d_id` 在系统范围内唯一
- `d_type = Core0` 当且仅当 `d_id = 0`
- `d_mem_quota` ≤ 系统可用物理内存总量
- `∀ cap ∈ d_caps. valid_capability(cap) ∧ cap_owner(cap) = d_id`

---

### 1.2 能力 (Capability)

**能力句柄结构：**
```
capability_handle :: word64  -- 16位域ID + 48位能力ID
```

**能力类型定义：**
```
capability :: 
  | PhysicalMemoryCap {
      base    : physical_addr,
      size    : nat,
      perms   : permission_set  -- {Read, Write, Execute}
    }
  | LogicalCoreCap {
      core_id    : logical_core_id,
      quota      : nat,         -- 保证的最小时间比例（百分比）
      priority   : priority,
      exclusive  : bool,        -- 是否独占
      migrate    : migrate_policy -- Allow | Forbid | CostHint(n)
    }
  | IpcEndpointCap {
      endpoint_id : endpoint_id,
      perms       : ipc_permission -- {Send, Receive, Call}
    }
  | InterruptCap {
      irq_num   : irq_vector,
      handler   : virtual_addr,
      target_domain : domain_id
    }
  | DomainCap {
      target_domain : domain_id,
      perms         : domain_permission -- {CreateThread, TransferCap, ...}
    }
  | SharedMemoryCap {
      shm_id    : shm_id,
      offset    : nat,
      size      : nat,
      perms     : permission_set
    }
  | ModuleCap {
      module_id : module_id,
      version   : version,
      perms     : module_permission
    }
```

**能力派生关系：**
```
derives_from :: capability → capability → bool
-- 子能力的权限 ⊆ 父能力的权限
-- 子能力的资源范围 ⊆ 父能力的资源范围
```

**不变量：**
- 能力不可伪造：`∀ cap. valid_capability(cap) → cap ∈ global_cap_table`
- 权限单调性：`∀ parent child. derives_from(child, parent) → perms(child) ⊆ perms(parent)`
- 撤销传播：`revoke(cap) → ∀ child. derives_from(child, cap) → invalidate(child)`

---

### 1.3 逻辑核心 (Logical Core)

**抽象定义：**
```
LogicalCore :: {
  lc_id         : logical_core_id,
  lc_phys_map   : option physical_core_id, -- 当前物理核心映射
  lc_owner      : domain_id,               -- 持有域
  lc_type       : core_type,               -- Performance | Efficiency | GPU | DSP | ...
  lc_exclusive  : bool,                    -- 独占性标记
  lc_quota      : nat,                     -- 保证配额（百分比）
  lc_migrate    : migrate_policy,
  lc_affinity   : physical_core_id set,    -- 亲和性集合
  lc_ready_q    : thread_id list,          -- 就绪队列
  lc_time_used  : nat,                     -- 已用时间
  lc_time_remain: nat                      -- 剩余配额
}
```

**不变量：**
- 每个逻辑核心在任何时刻最多运行一个线程
- `lc_time_used ≤ lc_quota`
- `lc_ready_q` 中的线程都允许调度到该核心

---

### 1.4 线程 (Thread)

**抽象定义：**
```
Thread :: {
  t_id          : thread_id,
  t_domain      : domain_id,
  t_state       : thread_state,  -- Running | Ready | Blocked | Waiting
  t_bound_core  : option logical_core_id,
  t_context     : register_context,
  t_stack_ptr   : addr,
  t_entry_point : addr
}
```

**不变量：**
- `t_domain` 必须是有效的域 ID
- 若 `t_state = Running`，则恰好有一个逻辑核心正在执行该线程

---

### 1.5 物理内存

**物理内存区域：**
```
MemoryRegion :: {
  mr_base   : physical_addr,
  mr_size   : nat,
  mr_type   : memory_type -- Available | Reserved | Device | ACPI
}
```

**物理帧分配器：**
```
PhysicalFrameAllocator :: {
  free_frames  : physical_frame set,
  allocated    : physical_frame → domain_id  -- 分配映射
}
```

**不变量：**
- 所有已分配帧都在某个域的能力范围内
- 不同域的内存区域不重叠（除非通过共享内存显式授权）

---

### 1.6 中断

**中断路由表：**
```
InterruptRoutingTable :: irq_vector → (domain_id × handler_addr)
```

**中断状态：**
```
InterruptState :: {
  enabled  : irq_vector set,
  pending  : irq_vector set
}
```

**不变量：**
- 中断仅投递到配置的目标域
- 中断处理期间不发生地址空间切换

---

### 1.7 全局调度器

**调度状态：**
```
SchedulerState :: {
  current_core  : logical_core_id option,
  run_queue     : logical_core_id list,  -- 按优先级排序
  time_slice    : nat
}
```

**调度操作：**
```
schedule :: SchedulerState → SchedulerState × logical_core_id option
-- 选择下一个要运行的逻辑核心

context_switch :: Thread → Thread → unit
-- 切换线程上下文
```

**不变量：**
- 每个逻辑核心在任何时刻最多由一个线程运行
- 核心的配额不会被超额使用
- 调度决策尊重核心的独占性和亲和性约束

---

## 2. 安全与隔离抽象

### 2.1 能力系统的安全属性

**不可伪造性：**
```
∀ domain cap. 
  cap ∈ domain.d_caps → 
  ∃ entry ∈ global_cap_table. entry.handle = cap
```

**权限最小化（派生单调性）：**
```
∀ parent child.
  derives_from(child, parent) →
  perms(child) ⊆ perms(parent) ∧
  resource_range(child) ⊆ resource_range(parent)
```

**撤销传播性：**
```
∀ cap child.
  derives_from(child, cap) ∧ revoked(cap) →
  invalidated(child)
```

**所有权唯一性：**
```
∀ frame.
  |{domain | frame ∈ domain.owned_frames}| ≤ 1
```

---

### 2.2 隔离性

**内存隔离：**
```
∀ d1 d2 frame.
  d1 ≠ d2 ∧ frame ∈ d1.owned_frames ∧ frame ∈ d2.owned_frames →
  ∃ shm_cap. is_shared_memory_cap(shm_cap) ∧
             authorized(d1, shm_cap) ∧ authorized(d2, shm_cap)
```

**计算隔离：**
```
∀ d1 d2 lc.
  lc.owner = d1 ∧ d1 ≠ d2 →
  time_consumed_by(d2, lc) = 0
```

**中断隔离：**
```
∀ irq d.
  irq_target(irq) = d →
  ∀ d'. d' ≠ d → cannot_receive(d', irq)
```

---

### 2.3 完整性

**数据完整性：**
```
∀ domain frame.
  write(frame) → has_write_capability(domain, frame)
```

**控制流完整性：**
```
∀ domain target.
  jump_to(domain, target) →
  ∃ ipc_cap. ipc_cap ∈ domain.d_caps ∧
             ipc_cap.target = target ∧
             Call ∈ ipc_cap.perms
```

---

### 2.4 信息流安全

**无干扰模型：**
```
∀ d_high d_low.
  security_level(d_high) > security_level(d_low) →
  actions(d_high) ⋪ observations(d_low)
  
-- 其中 ⋪ 表示"不干扰"关系
```

**数据流策略：**
```
∀ cap source target.
  data_flow(source, target, cap) →
  security_level(source) ≤ security_level(target)
```

---

### 2.5 资源配额保证

**内存配额：**
```
∀ domain.
  actual_allocated(domain) ≤ domain.d_mem_quota
```

**CPU 配额：**
```
∀ domain.
  cpu_time_consumed(domain) ≤ 
  Σ lc ∈ owned_cores(domain). lc.lc_quota
```

---

## 3. 动态特性抽象

### 3.1 滚动更新 (Rolling Update)

**更新原语：**
```
export_state :: domain_id → snapshot
-- 导出域状态快照

import_state :: snapshot → domain_id → bool
-- 导入状态到目标域

prepare_migration :: domain_id → ready_state
-- 准备迁移

atomic_switch :: target_domain → unit
-- 原子切换服务端点

rollback :: unit → unit
-- 回滚到上一个稳定状态
```

**状态快照：**
```
Snapshot :: {
  capabilities  : capability set,
  memory_image  : (addr × byte list) list,
  thread_states : thread_state list
}
```

**原子性保证：**
```
∀ update_op.
  update_op completes ∨ update_op rollbacks
-- 更新操作要么完全成功，要么完全回滚
```

---

### 3.2 模块加载与卸载

**模块格式：**
```
Module :: {
  mod_id       : module_id,
  version      : version,
  binaries     : (arch × binary) list,
  metadata     : module_metadata,
  signature    : crypto_signature,
  dependencies : (module_id × version_constraint) list
}
```

**模块操作：**
```
load_module :: Module → domain_id → result
-- 加载模块到指定域

unload_module :: module_id → result
-- 卸载模块

verify_module :: Module → bool
-- 验证模块签名和依赖
```

**不变量：**
- 模块签名验证通过后才能加载
- 所有依赖项必须满足版本约束

---

## 4. 硬件与架构抽象

### 4.1 核心硬件抽象层 (CHAL)

**内存操作：**
```
read_physical  :: physical_addr → byte
write_physical :: physical_addr → byte → unit
read_mmio      :: physical_addr → word
write_mmio     :: physical_addr → word → unit
```

**页表/MPU 操作：**
```
create_page_table  :: unit → page_table
map_page           :: page_table → virtual_addr → physical_addr → permission_set → unit
unmap_page         :: page_table → virtual_addr → unit
flush_tlb          :: unit → unit
set_mpu_region     :: region_id → physical_addr → size → permission_set → unit
```

**中断控制：**
```
enable_irq   :: irq_vector → unit
disable_irq  :: irq_vector → unit
register_isr :: irq_vector → handler → unit
```

**原子操作：**
```
cas        :: addr → expected → new → bool
mem_fence  :: fence_type → unit
```

**上下文切换：**
```
save_context    :: thread_id → unit
restore_context :: thread_id → unit
```

---

### 4.2 多架构支持

**架构标识：**
```
Architecture :: X86_64 | ARMv8_A | RISC_V | ...
```

**架构特定抽象：**
```
PageTableFormat :: architecture → page_table_format
PrivilegeSwitch :: architecture → privilege_switch_method
InterruptController :: architecture → interrupt_controller_interface
```

---

### 4.3 AMP 异构核心

**物理核心类型：**
```
PhysicalCoreType :: 
  | PerformanceCore    -- 高性能核
  | EfficiencyCore     -- 能效核
  | GPUCore            -- 图形处理器
  | NPUCore            -- 神经网络处理器
  | DSPCore            -- 数字信号处理器
```

**逻辑核心映射约束：**
```
∀ lc pc.
  map(lc, pc) →
  type_compatible(lc.lc_type, pc.pc_type)
```

---

## 5. 高级服务抽象

### 5.1 IPC 与共享内存

**IPC 调用：**
```
ipc_call :: capability_handle → ipc_params → ipc_result
-- 同步调用门
-- 参数通过寄存器或共享内存传递
```

**共享内存通道：**
```
SharedMemoryChannel :: {
  shm_id     : shm_id,
  size       : nat,
  participants : domain_id set,
  buffer     : ring_buffer
}
```

**消息格式：**
```
Message :: {
  header   : message_header,  -- 自描述、版本化
  payload  : byte list
}
```

---

### 5.2 服务端点注册与查询

**端点表：**
```
EndpointTable :: service_name → capability_handle
```

**端点操作：**
```
register_endpoint :: service_name → capability_handle → result
query_endpoint    :: service_name → capability_handle option
```

---

## 6. 启动与引导抽象

### 6.1 启动阶段

**启动信息块：**
```
BootInfo :: {
  memory_map   : MemoryRegion list,
  acpi_tables  : addr list,
  device_tree  : option device_tree,
  boot_params  : boot_parameter list
}
```

**启动序列：**
```
BootSequence :: 
  Reset → Bootloader → Core0Init → BuildInitialDomains → LoadPrivilegedServices → RunUserApps
```

---

### 6.2 初始状态

**初始域：**
```
InitialDomains :: {
  core0_domain    : Domain,  -- d_id = 0, d_type = Core0
  privileged_svcs : Domain list,
  root_caps       : capability_handle set
}
```

**信任链：**
```
TrustChain :: 
  HardwareRoot → BootloaderVerified → KernelVerified → ServicesVerified
```

---

## 7. 系统状态与操作语义

### 7.1 全局系统状态

```
SystemState :: {
  domains           : domain_id → Domain,
  threads           : thread_id → Thread,
  logical_cores     : logical_core_id → LogicalCore,
  cap_table         : capability_handle → capability,
  phys_mem          : physical_addr → byte,
  frame_allocator   : PhysicalFrameAllocator,
  interrupt_table   : InterruptRoutingTable,
  interrupt_state   : InterruptState,
  scheduler         : SchedulerState,
  endpoint_table    : EndpointTable,
  modules           : module_id → Module
}
```

### 7.2 状态转换关系

```
(σ, op) → σ'

-- σ 为当前状态
-- op 为操作
-- σ' 为操作后的状态
```

---

## 8. 核心不变量汇总

| 不变量名称 | 形式化描述 |
|-----------|-----------|
| **能力空间一致性** | `∀ d cap. cap ∈ d.d_caps → valid(cap) ∧ owner(cap) = d.d_id` |
| **内存映射一致性** | `∀ mapping. mapping.perms ⊆ cap_perms(mapping.source_cap)` |
| **调度队列完整性** | `∀ lc t. t ∈ lc.lc_ready_q → t.bound_core ∈ {None, Some lc.lc_id}` |
| **配额不超限** | `∀ lc. lc.lc_time_used ≤ lc.lc_quota` |
| **域隔离性** | `∀ d1 d2. d1 ≠ d2 → disjoint(d1.memory, d2.memory) ∨ explicitly_shared` |
| **中断路由有效性** | `∀ irq. irq_target(irq) ∈ valid_domains` |
| **模块依赖一致性** | `∀ mod. loaded(mod) → ∀ dep. satisfies(mod.dependencies, dep)` |

---

## 9. 后续精化方向

本抽象规范将逐步精化为：

1. **可执行规范**：用函数式语言描述具体算法
2. **实现规范**：C 代码级的数据结构和操作
3. **汇编规范**：架构特定的底层实现

每个精化层次都需要证明与上一层的正确性保持关系。

---

*文档版本：1.0*
*最后更新：2026-03-25*
