<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC Architecture Portability Design Document

## 1. Design Philosophy: Portability as a First-Class Citizen

The HIC (Hierarchical Isolation Kernel) architecture treats **portability** as a core design goal, not an afterthought. Its design philosophy is: through a unified architectural paradigm, achieve **cross-platform consistency** from embedded microcontrollers to data center servers, while allowing deep optimization on specific platforms.

## 2. Multi-Layered Hardware Abstraction System

### 2.1 Core Hardware Abstraction Layer (CHAL)

CHAL is the minimal abstraction layer between Core-0 and the specific hardware, providing only architecturally essential atomic operations:

1. **Memory Management Primitives**
   - Page table operations (create, map, set permissions)
   - TLB management (flush, invalidate)
   - Physical memory allocator interface

2. **Execution Context Management**
   - Context save/restore primitives
   - Privilege level switching mechanism
   - Exception/Interrupt stack frame layout

3. **Atomic and Synchronization Primitives**
   - Memory barriers (full, load-store, store-store)
   - Atomic operations (CAS, LL/SC variants)
   - Basic spinlock implementation

The design principle of CHAL is **"minimal but complete"**: each interface has clear cross-platform semantics, but allows different architectures to implement it in the most efficient way.

---

#### 2.1.1 Zero-Cost Abstraction Principle

All interfaces of CHAL must adhere to the following three iron rules to ensure that cross-platform abstractions **introduce no runtime overhead**:

1. **Compile-Time Polymorphism, No Runtime Polymorphism**
   - CHAL interfaces must be provided as **C++ templates/policy classes** or **C structure function tables (fully inlined at build time)**.
   - Virtual functions, function pointers (except hardware callbacks), and runtime architecture detection branches are prohibited.
   - Architecture differences are resolved through **template specialization** or **separate compilation units**; call sites are always direct calls or inlined.

2. **Zero Indirection**
   - After instantiation, the machine code of any abstract interface must be equivalent to hand-written architecture-specific code.
   - Compiler optimizations (inlining, constant propagation) must be able to eliminate all abstraction boundaries.

3. **Hardware Features Only Exposed Upwards**
   - Do not attempt to **unify** capabilities that cannot be unified (e.g., page table formats of different MMUs). Instead, **unify operation semantics**, with concrete implementations completed by AAL in the most direct way.
   - For example: do not provide a cross-architecture "page table walker"; instead require AAL to implement primitives like `map_page`, `unmap_page`, `get_pte`, and let upper layers build policies based on these primitives.

**Implementation Verification**: After each architecture port, the disassembly of core scheduling paths, interrupt dispatch paths, and domain switch paths must be checked to ensure no indirect calls and no redundant conditional branches.

---

#### 2.1.2 Physical Memory Region Descriptor

AAL must provide Core-0 with a **list of physical memory regions** in the following format:

```c
struct mem_region {
    uintptr_t base;      // Physical start address
    size_t size;         // Region size
    uint32_t type;       // REGULAR, RESERVED, DEVICE, RECLAIMABLE, ...
    uint32_t flags;      // Cache attributes, NUMA node, etc.
};
```

**Implementation Requirements**:
- This list is generated **at build time** via the hardware description file (`platform.yaml`) or **at boot time** via UEFI/Device Tree/ATAGS.
- Core-0's physical page allocator **only allocates from REGULAR type regions**; other regions are only used for capability delegation (device memory) or are excluded.
- The no-MMU variant must treat the **entire available contiguous space** as a single REGULAR region.

This design ensures the allocator does not assume the whole address space is available and has **zero runtime discovery overhead**.

---

### 2.2 Architecture Adaptation Layer (AAL)

AAL provides optimized implementations for different processor families:

1. **x86-64 Adaptation Layer**
   - 4-level/5-level page table management
   - MSR (Model Specific Register) access abstraction
   - x2APIC/APIC interrupt controller interface
   - SMAP/SMEP security extension support

2. **ARMv8-A Adaptation Layer**
   - Stage 1/Stage 2 page tables (with virtualization support)
   - System register (SCTLR, TCR, MAIR) abstraction
   - GICv3/GICv4 interrupt controller
   - PAN (Privileged Access Never) and UAO (User Access Override) support

3. **RISC-V Adaptation Layer**
   - Sv39/Sv48 page table management
   - PLIC/CLINT interrupt controller abstraction
   - Privilege mode (M/S/U) switching
   - Custom CSR extension encapsulation

4. **Embedded Architecture Adaptation Layer** (ARMv7-M, RISC-V Machine mode, etc.)
   - MPU (Memory Protection Unit) management
   - Simplified interrupt controller (NVIC)
   - No-MMU memory protection mechanisms

---

### 2.3 Isolation Mechanism Abstraction Layer (IMAL)

IMAL is the key layer for unifying **Scheme 1 (Static Page Table Hardening)** and the **no-MMU variant** under a common interface in HIC.

#### 2.3.1 Primitive Set

```c
// Create an independent isolation domain
domain_t *imal_domain_create(void);

// Destroy an isolation domain
void imal_domain_destroy(domain_t *dom);

// Map physical memory within a domain, setting permissions
int imal_map(domain_t *dom, vaddr_t va, paddr_t pa, size_t size, perm_t perm);

// Unmap
int imal_unmap(domain_t *dom, vaddr_t va, size_t size);

// Switch current execution flow to the target domain (returns in the new domain)
void imal_switch_to(domain_t *dom);

// Global flush and domain-specific flush
void imal_tlb_flush_all(void);
void imal_tlb_flush_domain(domain_t *dom);
void imal_tlb_flush_va(vaddr_t va);
```

#### 2.3.2 Architecture Variant Implementations

- **MMU variant**: Implements isolation via page tables; `domain_t` points to the root page table PA; switching writes to TTBR/CR3.
- **MPU variant**: Implements isolation via MPU regions; `domain_t` contains region configurations; switching reloads MPU registers.
- **Safe variant**: No hardware isolation; `imal_map` only records permissions; `imal_switch_to` is a no-op; relies on compiler/interpreter.

**Key Design**: All IMAL primitives are **statically bound** at the call site to the specific variant implementation, selected at compile time via `#ifdef` or templates. The upper-layer capability system is **completely reused**, unaware of the underlying mechanism differences.

---

### 2.4 Platform Features Layer (PFL)

PFL encapsulates platform-specific hardware components:

1. **System Bus and Interconnect**
   - PCIe/PCI enumeration and configuration space access
   - ACPI/Device Tree parsing and abstraction
   - SoC internal bus (e.g., AXI, AHB) adaptation

2. **Peripheral Controllers**
   - Unified clock and power management interface
   - DMA engine abstraction
   - Standard peripheral interfaces like GPIO, I2C, SPI

3. **Advanced Accelerators**
   - GPU computing interface abstraction
   - Neural network accelerator unified model
   - Cryptographic engine hardware abstraction

## 3. Build-Time Architecture Selection Mechanism

### 3.1 Layered Configuration System

HIC adopts a three-layer configuration model:

```
Application Requirements Layer (user specifies)
    ↓
Architecture Feature Layer (automatically derived)
    ↓
Platform Constraint Layer (hardware description)
```

1. **Feature-Driven Configuration**: Developers specify requirements through high-level semantics (e.g., "needs virtualization support", "requires real-time capability"), and the build system automatically selects corresponding architectural features and optimizations.

2. **Constraint Propagation**: Constraints from the hardware description file (`platform.yaml`) – such as memory layout, interrupt controller type – propagate upwards, guiding configuration choices.

### 3.2 Conditional Compilation and Template Instantiation

HIC uses **static polymorphism** rather than runtime branches to implement cross-platform code:

1. **Concept-Based Templates**: Core algorithms (e.g., scheduler, capability system) are defined as templates, constrained by concepts.

2. **Policy Class Pattern**: Architecture-specific optimization strategies are injected as template parameters, e.g.:
   ```cpp
   // Pseudo-code example
   template<typename MMUPolicy, typename SchedulerPolicy>
   class Core0;
   ```

3. **Build-Time Instantiation**: The final kernel image is a template **explicitly instantiated** for the specific hardware, with no runtime architecture detection overhead.

## 4. Binary Interface Standardization

### 4.1 Cross-Architecture ABI Design

HIC defines three layers of ABI:

1. **Kernel Internal ABI** (KABI)
   - Stable interface between Core-0 and Privileged-1 services
   - Independent of architecture, but allows architecture-optimized implementations
   - Versioned, supporting long-term evolution

2. **Inter-Service Communication ABI** (SCABI)
   - IPC format between Privileged-1 services
   - Data representation (endianness, alignment) standardized
   - Supports self-describing message formats

3. **User Space ABI** (UABI)
   - Stable interface between applications and the system
   - Architecture-specific calling convention encapsulation
   - Supports dynamic linking and version compatibility

### 4.2 Architecture-Neutral Data Representation

1. **Fixed-Width Types**: Uses explicitly sized integers (`uint32_t`, etc.) to avoid architecture differences.

2. **Endian-Transparent Serialization**: All cross-domain messages use little-endian byte order, with automatic conversion if necessary.

3. **Alignment Requirement Abstraction**: Alignment requirements are marked via attributes, verified and optimized at compile time.

## 5. Unified Memory Model

### 5.1 Consistent Memory View

HIC provides a unified memory model through its architecture abstraction layers:

1. **Address Space Identifier (ASID) Management**
   - Cross-architecture unified ASID allocation strategy
   - Dynamic adaptation to TLB tag bit-width

2. **Cache Coherence Abstraction**
   - Unified memory type semantics (WB/WC/UC)
   - Standardized interface for cache maintenance operations (clean, invalidate)

3. **DMA Coherence Model**
   - Abstraction of device-accessible memory
   - Transparent use of IOMMU/SMMU
   - Buffer synchronization primitives

### 5.2 Physical Memory Layout Adaptation

1. **Boot-Time Discovery**: Unified kernel boot protocol supporting multiple boot standards (UEFI, DTB, ATAGS).

2. **Memory Region Normalization**: Maps platform-specific memory regions (e.g., reserved regions, device memory) to standard classifications.

3. **NUMA Abstraction**: Unified multi-node memory management, hiding architecture-specific topology differences.

## 6. Interrupt and Exception Handling Abstraction

### 6.1 Unified Interrupt Model

HIC defines an architecture-independent interrupt handling framework:

1. **Interrupt Descriptor**: Unified representation of an interrupt source, containing:
   - Interrupt type (edge/level, message-signaled/MSI)
   - Priority/affinity information
   - Handler metadata

2. **Interrupt Controller Abstraction**: Unified programming interface supporting:
   - Interrupt enable/disable
   - Priority configuration
   - Affinity setting
   - EOI (End of Interrupt) operation

3. **Nested Interrupt Handling**: Unified support for nested interrupts, including:
   - Interrupt priority masking
   - Interrupt stack management
   - Preemption control

---

#### 6.1.1 Static Interrupt Routing Table

HIC interrupt handling follows the principle of **determined at build time, zero lookup at runtime**:

1. **Build-Time Binding**
   The hardware synthesis system parses `platform.yaml`, specifying the handling service and entry point for each interrupt source. It generates an **interrupt routing table** (architecture-specific format).

2. **Runtime Installation**
   CHAL provides `interrupt_install(vector, handler, domain)`, implemented by AAL as:
   - x86: Write to IDT entry (interrupt gate, DPL=0)
   - ARM: Write to vector table, set VBAR; configure interrupt routing to the corresponding CPU interface via GIC
   - RISC-V: Write to `mtvec`/`stvec`; configure interrupt enable and priority via PLIC

3. **Direct Dispatch**
   When an interrupt triggers, the hardware directly jumps to the **pre-installed handler function**, whose entry point belongs to the target Privileged-1 service domain. Core-0 intervenes only in exceptional cases (e.g., illegal interrupt number).

**Performance Expectation**: The latency from the hardware interrupt signal to the first instruction of the service's ISR, on mainstream architectures, is **≤1 microsecond** (including hardware stack push and address jump).

---

### 6.2 Exception Handling Framework

1. **Unified Exception Classification**: Maps architecture-specific exceptions to standard categories (page fault, illegal instruction, breakpoint, etc.).

2. **Exception Context Abstraction**: Provides an architecture-independent view of the exception context, including:
   - Snapshot of general-purpose registers
   - Fault address / instruction pointer
   - Fault reason encoding

3. **Recovery Mechanisms**: Standardized exception recovery protocol supporting:
   - Transparent handling of user-mode faults
   - Graceful degradation of kernel-mode services
   - Isolation and recovery from hardware errors

## 7. Power Management and Power Control

### 7.1 Cross-Platform Power State Model

HIC defines a set of standard power states:

1. **CPU Power States** (C-states): Unified abstraction from C0 (active) to Cn (deep sleep).

2. **Device Power States** (D-states): Standardization of device-specific low-power modes.

3. **System Power States** (S-states): Standard definitions from S0 (on) to S5 (mechanical off).

### 7.2 Power Management Interface

1. **Frequency/Voltage Scaling**: Unified DVFS (Dynamic Voltage and Frequency Scaling) interface.

2. **Power Domain Management**: Abstracted control of platform power domains.

3. **Idle State Prediction**: Intelligent idle decision framework based on usage patterns.

## 8. Portable Abstraction for Virtualization Support

### 8.1 Unified Hardware Virtualization Interface

1. **VM Control Structure Abstraction**: Unified management of VMCS (x86) / VTTBR (ARM).

2. **Virtual Exception Injection**: Architecture-independent emulation of guest exceptions.

3. **Shadow Page Table/EPT Support**: Unified management of second-level address translation.

### 8.2 Para-Virtualization Interface

1. **Frontend/Backend Protocol**: Standard protocol for virtual device communication.

2. **Shared Memory Communication**: Zero-copy abstraction for inter-VM communication.

3. **Virtual Interrupt Controller**: Cross-platform virtual interrupt distribution.

## 9. Performance Monitoring and Debugging

### 9.1 Unified Performance Counters

1. **Event Abstraction Layer**: Maps architecture-specific performance events to standard categories (cache misses, branch mispredictions, etc.).

2. **Sampling and Analysis**: Cross-platform performance sampling infrastructure.

3. **Performance Monitoring Unit (PMU) Abstraction**: Unified PMU programming interface.

### 9.2 Debugging Support

1. **Hardware Breakpoints/Watchpoints**: Unified management of debug registers.

2. **Tracing and Profiling**: Architecture-independent instruction/data tracing.

3. **Core Dump Format**: Cross-platform unified core dump format.

## 10. Security Extension Abstraction

### 10.1 Unifying Hardware Security Features

1. **Memory Encryption**: Unified interface for memory encryption (e.g., AMD SEV, Intel SGX).

2. **Trusted Execution Environment (TEE)**: Cross-platform TEE abstraction (ARM TrustZone, Intel TDX, etc.).

3. **Control Flow Integrity (CFI)**: Standardization of hardware CFI support.

### 10.2 Cryptographic Acceleration

1. **Unified Cryptographic Primitives**: Abstraction of hardware-accelerated AES, SHA, RSA, etc.

2. **Random Number Generation**: Cross-platform hardware RNG interface.

3. **Key Management**: Secure key storage and usage abstraction.

## 11. Build and Deployment Toolchain

### 11.1 Cross-Compilation Support

1. **Toolchain Abstraction**: Unified compiler/linker interface supporting multiple toolchains.

2. **Target Description File**: Machine-readable description of target platform capabilities.

3. **Code Generation Strategy**: Automatic selection of optimization strategies based on target architecture.

### 11.2 Image Generation and Packaging

1. **Multi-Format Support**: Generate kernel images suitable for different bootloaders.

2. **Module Packaging**: Cross-platform module format (`.hicmod`) generation.

3. **Secure Signing**: Architecture-independent cryptographic signature verification chain.

---

#### 11.2.1 .hicmod Multi-Architecture Layout

To enable binary module deployment across architectures, `.hicmod` adopts a **multi-section structure**:

```
.hicmod File Layout
    Module Header: Magic, Version, UUID, Signature, Number of Architectures, etc.
    Metadata Section: Name, Description, Dependencies, Exported Endpoints, etc. (architecture-neutral)
    Architecture Section 1: Target Architecture ID (x86_64), Code Section, Data Section, Entry Offset
    Architecture Section 2: Target Architecture ID (aarch64), ...
    ...
```

**Loading Process**:
1. The Module Manager reads the module header and obtains the list of supported architectures.
2. Queries the current platform architecture ID and matches the corresponding architecture section.
3. Verifies the digital signature (either over the entire module file or per-section signatures).
4. Loads the code and data from the matched section into a new sandbox and performs relocation.
5. If no matching architecture is found, loading is rejected.

**Advantages**:
- **Zero installation-time compilation**, suitable for embedded and real-time scenarios.
- **Controlled storage overhead**: A module containing 2~3 mainstream architecture binaries increases size by about 2–3x, which is acceptable for modern storage.
- **Simplified security chain**: Signature verification is performed once, independent of the target platform's compiler.

**Future Extension**: Support **thin modules** (containing only IR) as an alternative, configurable by build strategy.

---

## 12. Testing and Verification Framework

### 12.1 Unit Test Portability

1. **Simulator Layer**: Lightweight hardware simulation for rapid testing.

2. **Architecture Emulation**: Run full test suites on emulators like QEMU.

3. **Fuzz Testing**: Cross-platform unified fuzzing framework.

### 12.2 Compliance Testing

1. **Architecture Conformance Testing**: Verifies that a HIC implementation conforms to the architecture specification.

2. **ABI Compatibility Testing**: Ensures interface consistency across different architectures.

3. **Performance Portability Testing**: Verifies that performance characteristics are predictable across different platforms.

---

### 12.3 Cross-Architecture Continuous Integration and Performance Gates

The HIC project adheres to the **principle of architecture equality**: every code commit must pass functional and performance tests on **all mainline architectures**.

**Infrastructure**:

1. **Emulation Test Farm**
   - x86_64: QEMU (full system emulation)
   - ARMv8-A: QEMU (`virt` platform)
   - RISC-V: QEMU (`sifive_u`, `virt`)
   - no-MMU: QEMU (ARM Cortex-M3, RISC-V 32-bit)

   Every PR automatically runs complete kernel boot, basic system calls, service loading, and rolling update tests.

2. **Real Hardware Regression Cluster**
   - x86: Multiple generations of Intel/AMD desktops and servers
   - ARM: Raspberry Pi 4, Kunpeng 920 server
   - RISC-V: SiFive HiFive Unmatched, VisionFive
   - Embedded: STM32F4 Discovery (Cortex-M4)

   Full test suites run daily on a timer, with results reported to a dashboard.

3. **Performance Gates**
   Critical path **performance baselines** (empty system call, domain switch, IPC latency, interrupt response) are automatically measured with each commit and compared against a baseline.
   - If a **single commit causes a performance drop >5%** → automatic merge rejection.
   - If a **trending drop (7-day average)** is detected → performance regression flagged.

**Portability Compliance Badge**: Each officially supported architecture must pass the **Architecture Compliance Suite** to receive a certification badge, enabling it to be marked as "production-ready".

---

## 13. Porting Roadmap and Best Practices

### 13.1 Steps for Porting to a New Architecture

1. **Phase 1: CHAL Implementation** - Implement the minimal core abstraction layer.
2. **Phase 2: Boot and Memory** - Implement bootstrapping and basic memory management.
3. **Phase 3: Interrupts and Exceptions** - Implement interrupt and exception handling.
4. **Phase 4: Scheduling and Synchronization** - Implement multi-core scheduling and synchronization primitives.
5. **Phase 5: Device Support** - Add platform-specific device drivers.
6. **Phase 6: Optimization** - Architecture-specific performance optimizations.

### 13.2 Maintenance and Evolution

1. **Architecture Regression Testing**: Every core change is tested on all supported architectures.
2. **Performance Regression Monitoring**: Continuously monitor performance changes on critical paths.
3. **ABI Stability Guarantees**: Strictly manage interface evolution to ensure backward compatibility.

## 14. Real-World Case: Porting Experience from x86-64 to ARMv8-A

### 14.1 Major Differences and Mitigations

1. **Memory Model Differences**
   - x86: TSO (Total Store Order) memory model
   - ARM: Weak memory model, requires explicit barriers
   - Mitigation: Implement architecture-specific memory barrier primitives in CHAL

2. **Interrupt Handling Differences**
   - x86: APIC, based on interrupt vectors
   - ARM: GIC, based on interrupt numbers
   - Mitigation: Unified interrupt descriptor abstraction

3. **Page Table Structure Differences**
   - x86: Fixed 4-level/5-level hierarchy
   - ARM: Flexible page table granularity
   - Mitigation: Generic page table walk algorithm

### 14.2 Performance Tuning Experience

1. **TLB Management Optimization**: ARM requires more aggressive TLB maintenance.
2. **Cache Alignment**: Differences in cache line sizes across architectures.
3. **Branch Prediction**: Tuning for architecture-specific branch predictors.

## 15. Future Extension Directions

### 15.1 Support for Emerging Architectures

1. **RISC-V Extensions**: Support for Vector extension (V), Bit manipulation extension (B), etc.
2. **Custom Accelerators**: Abstraction for Domain-Specific Architectures (DSAs).
3. **Heterogeneous Computing**: Unified management of CPU+GPU+FPGA.

### 15.2 Dynamic Configurability

1. **Runtime Architecture Detection**: Dynamically adapt to different micro-architectural features.
2. **Adaptive Optimization**: Automatic tuning based on runtime performance characteristics.
3. **Hybrid Architecture Support**: Unified management of different architecture cores within the same system.
