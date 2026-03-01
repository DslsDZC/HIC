<!--
SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>

SPDX-License-Identifier: CC-BY-4.0
-->

# HIC (Hierarchical Isolation Kernel) Architecture Reference Document

**Author**: DslsDZC
**Email**: dsls.dzc@gmail.com

## Minimalist High-Performance Design

HIC pursues extreme simplicity and performance, with the following core constraints:

1. **Minimalist Capability System**:
   - Simplified handle format: Domain ID (16bit) + Capability ID (48bit)
   - Verification speed < 5ns (approximately 13 instructions @ 3GHz)
   - Inline fast verification, no function call overhead
   - 64-byte cache line alignment to avoid false sharing

2. **Privileged Memory Access Channel**:
   - Privileged-1 services can bypass the capability system for direct memory access
   - Absolute prohibition of access to Core-0 kernel's own memory region
   - Ultra-fast path: < 2ns access check
   - Designed for high-performance drivers and critical system services

3. **Lock-Free Design**:
   - Core-0 layer executes on a single core, disables interrupts to guarantee atomicity
   - No traditional locking mechanisms (spinlocks, mutexes)
   - Uses memory barriers to ensure ordering

4. **Performance Targets**:
   - Regular capability verification: 4-5 ns
   - Privileged memory access: < 2 ns
   - System call: 20-30 ns
   - Interrupt handling: 0.5-1 μs
   - Context switch: 120-150 ns

## 1. Design Philosophy and Fundamental Goals

HIC (Hierarchical Isolation Kernel) aims to construct an operating system kernel that can adapt to systems ranging from resource-constrained embedded devices to complex general-purpose computing systems, all through a unified architectural paradigm. Its core objective is to achieve the ternary unification of extreme performance, strong security isolation, and dynamic extensibility within a single architectural framework.

**Core Design Principles:**

1. **Complete Separation of Mechanism and Policy**:
   - The kernel core (Core-0) provides only atomicized, policy-free execution primitives, isolation mechanisms, and a capability system.
   - All system policies and functional implementations are provided by loadable modular services at the Privileged-1 layer, customizable through **build-time configuration** or **runtime dynamic management**.

2. **Logical Three-Level Privilege Architecture**:
   - Constructs a logical three-level control flow: Untrusted Applications (Ring 3), Modular Privileged Service Sandboxes (Logical Ring 1), and the Kernel Core & Arbiter (Logical Ring 0).
   - This model unifies isolation and performance by utilizing the Memory Management Unit (MMU) and a software capability system at the highest physical privilege level (e.g., x86 Ring 0) to create strongly isolated "in-kernel processes".

3. **Unified Architecture, Elastic Deployment**:
   - The same architecture supports two deployment modes simultaneously:
     - **Static Synthesis Mode**: For embedded, real-time, or high-security scenarios, all service modules are compiled and linked into a single, deterministic kernel image at build time.
     - **Dynamic Modular Mode**: For general-purpose computing, desktop, or server scenarios, core services are statically compiled, while extended services (like device drivers, file systems, protocol stacks) can be securely installed, updated, and unloaded at runtime from network repositories.

4. **Strong Backward Compatibility and API Evolution**:
   - The system kernel and Privileged-1 services provide a stable user-mode ABI (Application Binary Interface).
   - Through modular services and user-mode compatibility libraries, complete backward compatibility with older APIs is transparently provided while supporting new APIs, ensuring the long-term stability of the application ecosystem.

## 2. Detailed Three-Level Model Architecture (Physical Space Direct Mapping Scheme)

This chapter elaborates on the core architectural model of HIC, particularly the design of the Privileged-1 layer employing physical space direct mapping and allocation constraints.

### 2.1 Core-0 Layer: Kernel Core and Arbiter

- **Physical Execution Environment**: CPU's highest privilege mode (x86 Ring 0, ARMv8-A EL1 or EL2).
- **Logical Role**: The system's Trusted Computing Base (TCB), the ultimate arbiter of resource access, and the enforcer of isolation mechanisms.
- **Design Goal**: Minimalist, formally verifiable. Its code size (excluding architecture-specific assembly and auto-generated data) is strictly targeted to be within 10,000 lines of C code.

**Core Responsibilities and Implementation Details:**

1. **Physical Resource Management and Allocation**:
   - Manages global bitmaps for all physical memory frames, CPU time slices, hardware interrupt lines, etc.
   - Employs a **physical memory direct allocation strategy**, avoiding the overhead of traditional virtual memory management. Allocates independent, contiguous physical memory regions for each isolation domain (including itself, each Privileged-1 service, and each Application), isolating them via the MMU.
   - Core-0's own code and data reside in a fixed physical memory region marked as inaccessible by the page tables of all other domains.

2. **Capability System Kernel**:
   - Maintains a global, protected capability table. Each entry (capability) is an unforgeable kernel object describing specific access permissions (e.g., read, write, execute) to a system resource (e.g., a range of physical memory, a range of hardware I/O ports, an IPC endpoint).
   - Each isolation domain is associated with a **capability space** – an array of capability indices (handles) that the domain "holds". A domain can only request resource operations via handles within its capability space.
   - Provides system calls for transferring, deriving (creating subsets of permissions), and revoking capabilities between domains. All operations undergo strict permission and ownership checks.

3. **Execution Control and Scheduling**:
   - Manages Thread Control Blocks (TCBs) for all threads. Each thread is bound to a specific isolation domain.
   - Implements a predictable, real-time-friendly scheduler. Scheduling decisions can be based on build-time configured static priorities or simple round-robin strategies.
   - Handles all exceptions and hardware interrupts. Interrupts are first delivered to Core-0, which, based on the build-time configured interrupt routing table, directly calls the interrupt handler function registered by the corresponding Privileged-1 service (via protected entry points).

4. **Isolation Enforcement**:
   - When creating a Privileged-1 service domain, Core-0 configures its page tables, mapping **only** the following regions:
     a) The physical memory region containing the service's own code and private data segments.
     b) Shared physical memory regions or device MMIO regions explicitly authorized by capabilities.
   - Service domains cannot modify their own page tables. Any illegal memory access triggers a page fault or access violation exception, which Core-0 converts into a service fault signal.
   - Control flow transfer between services must occur through synchronous call gates defined by Core-0. The call gate verifies that the caller holds the capability for the target service endpoint, performs a stack switch, and ensures the return address is controllable.

### 2.2 Privileged-1 Layer: Privileged Service Sandboxes (Physical Space Direct Mapping)

- **Physical Execution Environment**: Same as Core-0 (x86 Ring 0, ARMv8-A EL1/EL2).
- **Logical Role**: Modular providers of system functionality. Each service is an independent, mutually untrusting "kernel-mode process".
- **Key Design: No Traditional Virtual Address Space**
  - **Physical Memory Direct Mapping**: Each Privileged-1 service is allocated one or more contiguous physical memory regions. The service's code and data run and are accessed directly at these physical addresses, without requiring additional virtual address translation (beyond the MMU page table mappings required by the CPU, but these mappings are **identity mapped**, i.e., virtual address equals physical address offset). This eliminates virtual memory management overhead and makes memory access latency deterministic.
  - **Physical Space Isolation**: The physical memory regions of different services do not overlap in address space and are mutually isolated via MMU page table permission settings. A service cannot access the physical memory of another service or Core-0 unless explicitly authorized via the capability system for sharing.
  - **Physical Resource Allocation Limits**: The total physical memory, CPU time, I/O ports, etc., that each service can use are subject to hard quota limits enforced at build time or module installation time through the capability system. This is crucial for service isolation, ensuring a single service cannot exhaust system resources.
  - **Privileged Memory Access Channel**: Services marked as privileged (`DOMAIN_FLAG_PRIVILEGED`) can use a privileged channel to directly access physical memory (excluding Core-0's own memory), bypassing the capability system. This provides zero-overhead memory access for high-performance drivers (e.g., network, graphics). Privileged access checks require only 2-3 instructions, < 2ns latency.

**Core Characteristics and Implementation Details:**

1. **Independent Physical Address Space**:
   - Each service (e.g., network driver, file system, display service) runs in an independent physical memory region allocated by Core-0.
   - The service's view of the address space is contiguous and physical, simplifying its internal memory management and avoiding the non-determinism and overhead of virtual memory management.

2. **Capability-Based Resource Access**:
   - Upon startup, a service receives an initial set of capabilities from Core-0, configured at build time or dynamically passed by a parent service.
   - All access by a service to hardware (e.g., `in/out` instructions, MMIO access) or shared memory with other domains is verified by Core-0 (or under its supervision) via the capability system before the instruction executes.

3. **Fault Isolation and Recovery**:
   - A service crash (illegal instruction, page fault, division by zero, etc.) is confined to its own physical address space. After trapping the exception, Core-0 will:
     a) Terminate all threads within the faulting service domain.
     b) Reclaim all capabilities and physical resources (memory, devices) held by that domain.
     c) Send a notification event to a pre-designated "Monitor Service" responsible for system management.
   - The Monitor Service can decide, based on policy, whether to restart a new service instance and recover state from persistent storage (if the service design supports it).

4. **Efficient Communication Mechanisms**:
   - **Communication with Applications**: Utilizes shared physical memory regions, authorized by capabilities established at build time or initialization, for batched, zero-copy data exchange. **Synchronization strictly uses lock-free ring buffers and memory barriers, without any locking mechanisms**.
   - **Communication with Core-0**: Initiated via `syscall` instructions, handled by Core-0 for capability verification and request routing. Core-0 guarantees atomicity by disabling interrupts.
   - **Inter-Service Communication**: Primarily through two methods:
     a) **Indirect Communication**: Message relay via Core-0 (for control flow).
     b) **Direct Communication**: Two services hold capabilities for a shared physical memory region, enabling zero-copy data transfer (for data flow). **Lock-free ring buffers implement the producer-consumer pattern**.

### 2.3 Application-3 Layer: Application Layer

- **Physical Execution Environment**: CPU's lower privilege mode (x86 Ring 3, ARMv8-A EL0).
- **Logical Role**: Execution environment for untrusted user code.
- **Core Characteristics**:
  - Runs within a standard process abstraction, possessing an independent virtual address space (managed by Core-0 via the MMU).
  - All requests for system resources must be made via operating system APIs (ultimately targeting Privileged-1 layer services) and are authorized through Core-0's capability system.
  - Applications have no direct memory access to each other or to privileged services; communication occurs via IPC channels established by the kernel.

## 3. In-Depth Analysis of Core Security Mechanisms

### 3.1 Dynamic Lifecycle and Secure Transfer of the Capability System

Capabilities are the sole carriers of authorization in HIC. Their security does not depend on storage location, but on interpretation by the kernel (Core-0).

1. **Creation**: Capabilities are either initially generated by the hardware synthesis system at build time based on configuration, or dynamically created by Core-0 at runtime in response to resource allocation requests (e.g., `mmap`).
2. **Storage**: The "master copy" of a capability is stored in a protected global table within Core-0. The capability space held by each domain only stores obfuscated indices (handles) pointing to entries in this global table.
3. **Transfer and Revocation**:
   - **Sending**: Domain A requests to transfer capability C to domain B. Core-0 checks: (a) Does A hold C? (b) According to security policy, is B eligible to receive such a capability? (c) Should permission attenuation be applied (e.g., downgrading read-write to read-only)? Upon successful verification, Core-0 creates a new handle in B's capability space, pointing to the same global capability object C (or a derived version of it).
   - **Receiving**: B only obtains a local handle; it cannot learn the handle value of this capability in A or other domains, nor can it directly manipulate the global table.
   - **Revocation**: When a domain is destroyed or a capability is explicitly revoked, Core-0 removes the capability entry from the global table and immediately invalidates all handles pointing to it across all domains. Subsequent access attempts using invalid handles are denied.

### 3.2 Unified API Access Model and Secure Communication

All cross-domain interactions go through standardized interfaces, eliminating privileged backdoors.

1. **API Gateway**: Each Privileged-1 service registers one or more "service endpoint capabilities" with Core-0. Applications or other services initiate requests by invoking the `ipc_call(cap_endpoint, message)` system call.
2. **Call Path**:
   - Core-0 intercepts the call and verifies the caller holds the capability for the target endpoint.
   - Core-0 securely switches the call context (registers, message content) to the target service domain.
   - After the target service processes the request, it returns, and Core-0 switches the result context back to the caller.
3. **Secure Establishment of Shared Memory Channels**:
   - The establishment process must be arbitrated by Core-0. For example, Application A requests to establish shared memory with Service B.
   - Core-0 allocates physical memory and maps it into A's virtual address space and B's physical address space, potentially with different permissions (e.g., A write-only, B read-only).
   - Core-0 creates two "memory capabilities" granted to A and B respectively, precisely describing their specific access permissions. Any access attempt exceeding these permissions by either party triggers Core-0's exception handling.

### 3.3 Security Auditing and Tamper-Proof Logging

Auditing is crucial for post-incident accountability and intrusion detection.

1. **Log Storage**: During system initialization, Core-0 allocates a physical memory region as an append-only audit log buffer. Core-0 maps this buffer read-only to itself and a dedicated, highly trusted "Audit Service".
2. **Log Recording**: Core-0 appends structured records to the end of the buffer when executing any critical security operations (capability verification success/failure, service creation/destruction, privileged calls). Writes are atomic.
3. **Tamper-Proofing**:
   - No other service (including most Privileged-1 services) can obtain write capability to this buffer.
   - The Audit Service periodically reads log blocks, computes hashes, potentially encrypts them, and writes them to persistent storage. Even if runtime memory is physically tampered with by an attacker, the persisted encrypted logs guarantee post-incident detectability.
4. **Deterministic Service Recovery**: After a service crash, its recovery is driven by the "Monitor Service". Core-0 provides atomic resource reclamation primitives. The Monitor Service decides, based on pre-set policies (e.g., restart count, dependencies), whether and how to restart a new instance. The new instance starts from an initial state or loads from a persistent snapshot, ensuring system state remains predictable.

## 4. Build-Time Hardware Synthesis System

This is a crucial compilation phase that transforms hardware uncertainty into software determinism.

**Input**: A machine-readable hardware description file (e.g., `platform.yaml`), containing:

- CPU count, memory topology, cache information.
- List of all devices enumerated via PCI/Device Tree, along with their MMIO addresses, interrupt numbers (e.g., GSI), and DMA ranges.
- List of required system services and their dependencies.
- Security policies: initial capability assignments, service resource quotas, communication permissions.

**Processing Flow**:

1. **Parsing and Conflict Resolution**: The system parses the description file, checks for resource conflicts (e.g., two devices claiming the same interrupt), and resolves them according to predefined strategies (reallocation or error reporting).
2. **Privileged Service Code Generation**:
   - Selects or generates device driver code from template libraries, optimized specifically for the exact device models listed in the description file (e.g., unrolling loops, inlining specific register operations).
   - Links the driver code with the corresponding service framework (e.g., network protocol stack service framework), compiling them into independent binary modules. These modules will later be loaded as Privileged-1 services.
3. **Generation of System Static Configuration**:
   - **Memory Layout Table**: Determines the precise physical memory layout for each service, the kernel core, and shared memory regions.
   - **Interrupt Routing Table**: Specifies, for each hardware interrupt number, the Privileged-1 service and its handler function entry point responsible for it.
   - **Initial Capability Allocation Table**: Defines the initial set of capabilities each service domain receives upon system startup.
   - **Device Initialization Sequence**: Defines the order of initialization calls for all device drivers during the system boot phase, resolving inter-device dependencies.
4. **Final Image Synthesis**: Links the Core-0 kernel, all generated Privileged-1 service binary modules, and all the configuration data tables mentioned above, producing a single, directly bootable kernel image.

**Output Characteristics**: The resulting image is fully customized for the specific hardware, containing optimally optimized code paths and deterministic resource bindings, with zero runtime "discovery" overhead.

## 5. Dynamic Device Support and Modular Systems

Building upon build-time determinism, HIC provides a controlled dynamic extension mechanism to handle real-world hot-plug requirements (e.g., USB, NVMe SSDs).

1. **Dynamic Resource Pool**:
   - At build time, a portion of total physical resources (e.g., a set of MSI-X interrupt vectors, a reserved range of PCIe BAR space) is marked as a "dynamic pool" and not allocated to any static service.
   - A special "Hot-Plug Coordination Service" is created and holds the top-level capability to manage this dynamic pool.

2. **Secure Driver Modules**:
   - Driver modules exist as signed binary files. Signatures are provided by the system builder or a trusted third party.
   - The module format includes declarations of required resources (needed interrupts, DMA memory) and its external interfaces.

3. **Hot-Plug Workflow**:
   - **Discovery**: A device is inserted, triggering an interrupt. Based on the interrupt routing table, Core-0 dispatches this interrupt event to the "Hot-Plug Coordination Service".
   - **Loading and Verification**: The coordination service, based on the device ID, loads the corresponding driver module from trusted storage, verifying its digital signature and integrity.
   - **Creating a Service Sandbox**: The coordination service requests Core-0 to create a completely new, empty Privileged-1 service address space (allocate physical memory and configure page tables).
   - **Resource Allocation**: The coordination service allocates the required interrupts, memory, etc., from its managed dynamic pool and grants the corresponding capabilities to the newly created service sandbox via Core-0.
   - **Initialization and Registration**: The coordination service loads the verified driver module's code and data into the new sandbox's physical memory and starts its initialization routine. Upon successful initialization, the new driver service registers the functionalities it provides (e.g., a new block device node) with the system.
   - **Removal**: When a device is unplugged, the coordination service notifies the driver service to perform cleanup, then requests Core-0 to destroy that service sandbox and reclaim all dynamic resources allocated to it.

**Key Design**: Dynamically loaded drivers operate under the exact same security model as statically compiled drivers – they run in independent Privileged-1 sandboxes, constrained by the capability system. Dynamism does not compromise the core isolation principles.

## 6. Modular Service Architecture and Rolling Updates

### 6.1 Service Module Format and Secure Distribution

- **Module Format (`.hicmod`)**:
  - **Module Header**: Contains magic number, format version, module UUID, semantic version string, offset to API descriptors, code segment size, data segment size, offset to signature information.
  - **Metadata Segment**: Contains module name, description, author, list of exported service endpoints (name, ID, version), declared resource requirements, list of dependent modules (UUID + version constraint).
  - **Code Segment**: Contains relocatable machine code (architecture-specific, e.g., x86-64, ARM64) with entry points conforming to the Privileged-1 service ABI.
  - **Data Segment**: Contains read-only global data and initialized read-write data.
  - **Signature Segment**: Contains a cryptographic hash (e.g., SHA-384) of the header, metadata, code, and data segments, along with a digital signature from the issuer or developer.

- **Secure Distribution and Repository**:
  - The system maintains one or more trusted module repositories (local or network). Repositories provide module indexes, metadata, and `.hicmod` files for download.
  - Each module is uniquely identified in the repository by `UUID@version`.
  - Before installation, a module's digital signature must be verified to ensure it was published by a trusted party and has not been tampered with.

### 6.2 Module Manager Service

This is a core system service running at the Privileged-1 layer, responsible for the full lifecycle management of modules.

**Core Functions**:

1. **Repository Interaction**: Fetches module indexes from configured repositories, supports querying and downloading modules.
2. **Dependency Resolution**: When installing or updating a module, computes the complete dependency graph, resolves version conflicts, and ensures system consistency.
3. **Security Verification**: Invokes cryptographic primitives provided by Core-0 to verify module signatures and integrity.
4. **Loading and Instantiation**:
   a. Requests Core-0 to create a new, empty Privileged-1 service sandbox.
   b. Loads the verified module code and data into the sandbox's physical memory.
   c. Based on the module's metadata, requests Core-0 to allocate the declared resources (converted into capabilities).
   d. Calls the module's initialization entry point to start the service.
   e. Registers the endpoints provided by the service in Core-0's system service registry.
5. **Upgrade and Rollback**:
   - **Rolling Update (Hot Upgrade)**: For services supporting state migration, the Module Manager can:
     - Start a new sandbox for the new module version in parallel.
     - Gradually migrate client connections and internal state from the old instance to the new one using the service's built-in state migration protocol.
     - After migration is complete, terminate the old instance. This process is transparent to applications, achieving zero-downtime updates.
   - **Atomic Replacement (Cold Upgrade)**: For stateless services or those requiring a simple restart, the Module Manager can schedule a restart, atomically replacing the old module with the new one after verification.
   - **Rollback Mechanism**: Before each installation or upgrade, the old module version's binary and configuration are securely archived. If the new module fails to start or encounters critical faults during operation, the Module Manager can automatically or on-command roll back to the last known good version.
6. **Garbage Collection**: Uninstalls old module versions that are no longer depended upon by any other module, reclaiming storage space.

### 6.3 API Version Management and Coexistence of Multiple Implementations

To ensure long-term compatibility, the system supports the coexistence of multiple API versions for the same service.

**Implementation Mechanisms**:

1. **Versioned Service Endpoints**: When registering an endpoint, a service module declares the API version it implements (e.g., `filesystem.v1.open`, `filesystem.v2.open_with_flags`).
2. **Client Binding**:
   - An application (or a compatibility library) queries Core-0's registry for available endpoint versions upon its first call to a service.
   - It can explicitly request a specific version or request the "default" version (typically the latest stable release).
   - Based on the binding policy, Core-0 directs the client's capability authorization to the service instance corresponding to the chosen version.
3. **Parallel Multiple Instances**: `filesystem.v1` and `filesystem.v2` can run simultaneously as two independent Privileged-1 service sandboxes, each with its own state and resources. They can share the underlying block device driver (via capability delegation) but are completely isolated in terms of file system logic.
4. **Bridges and Adapters**: For complex API evolution, specialized adapter service modules can be developed. For example, a `posix-legacy-adapter` service module could receive traditional POSIX system calls and translate them into internal calls to new `filesystem.v2` and `vfs.v2` services, thus transparently supporting legacy applications.

### 6.4 Differentiated Deployment for Embedded and General-Purpose Systems

Through its build and configuration toolchain, the HIC architecture supports two extreme deployment models:

1. **Embedded/Static Synthesis Mode**:
   - **Process**: The developer writes a `system.yaml` configuration file listing all required service modules (including drivers, protocol stacks, etc.) and their versions. The build system (Hardware Synthesis System) statically links these modules together with Core-0 to produce a single, monolithic kernel image.
   - **Characteristics**:
     - **No Runtime Module Loading**: All services are ready immediately after system startup, with no dynamic loading overhead.
     - **Extreme Determinism**: Memory layout, interrupt assignments, and call paths are entirely determined at build time, suitable for functional safety (FuSa) certification.
     - **Minimal Storage Footprint**: No module repository, no extra versions, contains only essential code.
     - **Fast Boot Time**: No need for runtime dependency resolution or module loading.
2. **General-Purpose/Dynamic Modular Mode**:
   - **Process**: A base system image is released, containing Core-0 and a minimal set of statically linked core services (e.g., Module Manager, base drivers, network stack). After system startup, the Module Manager service automatically runs, downloading and loading other service modules (e.g., graphics drivers, sound card drivers, file system format support, advanced API services) on-demand from network repositories.
   - **Characteristics**:
     - **High Flexibility**: Users can dynamically install required drivers and functionalities based on their hardware and peripherals.
     - **Updateability**: Bug fixes or feature upgrades for individual services can be accomplished by replacing the module, without needing to replace the entire kernel or restart critical services (supports rolling updates).
     - **Storage Efficiency**: The base image is small; additional functionality is fetched on-demand.
     - **Ecosystem-Friendly**: Facilitates independent development and distribution of driver modules by hardware vendors.

## 7. Operational Workflow for Rolling Updates and System Evolution

**Scenario**: Safely upgrading the system's network protocol stack from `netstack.v1.2.0` to `netstack.v1.3.0`.

1. **Update Preparation**:
   - The Module Manager fetches the module file, signature, and metadata for `netstack.v1.3.0` from the repository.
   - Verifies the signature and integrity.
   - Resolves dependencies: confirms `netstack.v1.3.0` is compatible with other currently loaded modules.
   - Notifies potentially affected dependent services (e.g., firewall service, web server) to prepare for connection migration.
2. **New Instance Creation and Warm-up**:
   - The Module Manager requests Core-0 to create a new Privileged-1 sandbox.
   - Loads and initializes the `netstack.v1.3.0` module.
   - Allocates temporary network ports and buffer resources (capabilities) for the new instance.
   - The new instance starts its internal services but does not immediately take over external network traffic. It may establish a control connection with the old instance to synchronize routing tables, connection states, etc. (if the protocol supports it).
3. **Traffic Migration (Hot Upgrade)**:
   - The Module Manager coordinates between the old `v1.2.0` instance and the new `v1.3.0` instance.
   - For each active network connection (e.g., TCP socket), the old instance migrates the connection descriptor and state information to the new instance via a secure channel. This process may involve Core-0 remapping capabilities (e.g., transferring the capability for a memory region containing network packet buffers from the old instance to the new instance).
   - Client applications are unaware of the migration process. There might be occasional duplicate packet processing, handled by the protocol stack's sequence number mechanisms.
4. **Switchover and Cleanup**:
   - Once all connections (or critical connections) are migrated, the Module Manager updates Core-0's service registry, pointing the default endpoint for `netstack` to the `v1.3.0` instance.
   - New client connections will now be routed to the new instance.
   - The Module Manager signals the old `v1.2.0` instance to begin its shutdown process, waiting for remaining connections to time out or close.
   - Core-0 completely reclaims the old instance's resources, and its module binary can be marked for potential unloading.
5. **Rollback Contingency**:
   - Throughout the entire process, the `netstack.v1.2.0` instance and module files are retained.
   - If the new instance encounters a critical failure during initialization or migration, the Module Manager can abort the upgrade, switch traffic back to the old instance, and log the error.

## 8. Resource Management and Protection Mechanisms

### 8.1 Build-Time Resource Planning and Static Guarantees

- **Service-Level Quotas**: In the build configuration, hard upper limits are defined for each Privileged-1 service: maximum physical memory usage, CPU time share, maximum I/O operations per second.
- **Worst-Case Execution Time (WCET) Analysis**: For critical paths in Core-0 (e.g., scheduler, interrupt handling) and real-time critical service code, WCET can be determined through static analysis or measurement, serving as a basis for system schedulability analysis.
- **Dependency Analysis**: The build system analyzes communication dependencies between services to ensure resource quotas meet the cascading demands of service chains.

### 8.2 Runtime Resource Management

- **Stream Processing and Backpressure**: Data-processing services (e.g., video decoding) employ a producer-consumer pipeline model. When a consumer cannot keep up, it propagates a "backpressure" signal backward through the pipeline, causing the producer to pause, preventing unbounded buffer growth and memory exhaustion.
- **Incremental Allocation**: Services can request resources in phases. For example, a network service might initially request only a small buffer pool, then dynamically request more as the connection count grows. This improves resource utilization.
- **Pressure-Aware Scheduling**: Core-0 monitors system load (e.g., run queue length, memory pressure). Under high pressure, it can dynamically lower the priority or scheduling frequency of non-critical background tasks, ensuring response times for critical tasks (e.g., interrupt handling, control plane services).

### 8.3 Memory Optimization

- **Layering by Access Pattern**:
  - **Hot Data**: Active working set, kept uncompressed, mapped in TLB-friendly regions.
  - **Warm Data**: May be accessed again, can use lightweight compression (e.g., LZ4).
  - **Cold Data**: Rarely accessed, can use high-ratio compression (e.g., Zstandard), transparently decompressed on access.
- **Transparent Compression Service**: Exists as a Privileged-1 service. Other services can "swap" memory pages to it for compression/decompression. The compression service holds the capability for the compressed data pages, the original service holds the capability for the decompressed data pages, and Core-0 manages the switching between them.

### 8.4 Resource Flooding and Denial-of-Service Attack Protection

Resource Flooding is a typical Denial-of-Service attack where an attacker exhausts critical system resources (e.g., memory, file descriptors, connection count) to cripple the system. HIC employs a multi-layered defense mechanism against such attacks:

1. **Hard Upper Limits Based on Build-Time Quotas**:
   - Each Privileged-1 service and Application is assigned explicit resource quotas upon creation. For example, a network service may hold a maximum of 1024 connection descriptors and a 64MB buffer pool. These are unbreakable hard limits enforced by Core-0 during resource allocation.
   - This static quota mechanism fundamentally prevents a single compromised or faulty service from exhausting global resources.

2. **Hierarchical Resource Quotas and Delegation**:
   - When a service needs to allocate resources for multiple clients (e.g., a web server allocating buffers per connection), it must "subdivide" quotas from its own allocation for each client. Core-0 supports capability subdivision and delegation, allowing a service to create sub-capabilities with smaller limits for each client.
   - Thus, even if a client behaves abnormally (e.g., slow reading causing buffer backlog), it only exhausts the sub-quota allocated to it, without impacting the service itself or other clients.

3. **Real-Time Monitoring and Adaptive Rate Limiting**:
   - Core-0 and resource monitoring services continuously track resource usage per domain. When a domain's resource usage exceeds a certain threshold of its quota (e.g., 80%), the monitoring service issues a warning.
   - For resource requests initiated from the Application layer (e.g., memory allocation via system calls), Core-0 can implement adaptive rate-limiting algorithms. For instance, if an application is detected allocating memory at an abnormally high rate, Core-0 can dynamically slow down its allocation rate or add latency to subsequent requests.

4. **Flow Control for Communication Endpoints**:
   - All IPC endpoints and shared memory channels can be configured with flow control policies, including:
     - **Credit-Based Flow Control**: The sender must obtain "credits" from the receiver to send data; the receiver grants credits based on its processing capacity.
     - **Backpressure Based on Queue Depth**: When a receiver's message queue reaches a certain depth, Core-0 can temporarily block the sender or return a "resource temporarily unavailable" error.
   - This prevents a fast producer from overwhelming a slow consumer and is a key defense against application-layer flood attacks.

5. **Global Degradation and Isolation Under Emergency Conditions**:
   - When overall system resource pressure exceeds a safety threshold (e.g., total free memory below 5%), the system enters an emergency state.
   - At this point, Core-0 can trigger the following global protective measures:
     a) **Prioritize Critical Services**: Suspend or terminate non-critical services and tasks based on build-time defined priorities.
     b) **Reject New Requests**: Return failure for all non-critical new resource allocation requests.
     c) **Attack Source Isolation**: If the resource monitoring service identifies a suspected attack source through algorithms (e.g., an application triggering numerous capability verification failures in a short time), it can request Core-0 to temporarily freeze that application domain for further inspection.

6. **Timeliness of Capability Reclamation**:
   - When a service or application crashes, Core-0 ensures all its held capabilities and resources are reclaimed atomically and immediately. This prevents resources from being "leaked" due to process crashes, which could be exploited by an attacker (gradually exhausting resources by repeatedly crashing processes).

This combination of defense mechanisms ensures that when facing resource flooding attacks, the HIC system can **limit the blast radius** (via quotas), **provide early warnings** (via monitoring), **maintain core functions** (via priority guarantees), and **achieve rapid recovery** (via atomic reclamation).

## 9. Performance Optimization System and Expected Metrics

HIC's performance advantages stem from architectural design that deeply optimizes critical paths.

1. **System Call/Service Call Latency**:
   - **Traditional Microkernel**: Requires a full privilege level switch (save/restore many registers) + independent address space switch (page table switch) + IPC message copy. Latency is typically in microseconds.
   - **HIC (Physical Space Direct Mapping)**: When calling a Privileged-1 service, there is no privilege level switch, no page table switch (the service and caller may be in the same physical address space, or transition via pre-set, TLB-resident shared pages). The primary overhead is Core-0's capability verification (a few memory table lookups) and a controlled context jump. Design target: **20-30 nanoseconds**.
2. **Interrupt Handling Latency**:
   - **Traditional System**: Interrupt → Kernel generic entry point → Save context → Call driver ISR (may trigger scheduling) → Restore context. Latency often ranges from a few microseconds to tens of microseconds.
   - **HIC**: Interrupt → Core-0 simplified entry (save few critical registers) → Directly call the ISR registered by the Privileged-1 service according to the static routing table (same privilege level function call). Design target: **0.5-1 microsecond**.
3. **Thread Switch Latency**:
   - Occurs within Core-0, switching between two threads at the same physical privilege level. Requires only switching general-purpose registers, stack pointer, and thread-private data pointers; no page table switch (unless crossing domains). Design target: **120-150 nanoseconds**.
4. **Communication Bandwidth**:
   - Inter-service communication via shared memory achieves true zero-copy. Bandwidth upper bound depends on memory copy speed or DMA speed, with no additional kernel copy overhead.

**Performance Data Clarification**: The above figures are theoretical estimates based on architectural design. Their achievement depends on: (a) manual optimization or compiler optimization of critical path code; (b) good alignment of critical data structures and control paths with CPU cache characteristics; (c) the hardware itself providing low-latency system call and interrupt response mechanisms. Actual performance requires final validation on specific hardware prototypes using benchmarks (e.g., LMbench, cyclictest).

## 10. Security Auditing and Monitoring System

### 10.1 Real-Time Monitoring

- **Resource Monitoring Service**: A privileged service that periodically reads resource usage statistics (CPU time, memory page allocations, IPC call counts) for each domain from Core-0, providing visualization or alerting interfaces.
- **Anomalous Behavior Detection**: Core-0 can be configured with rules, such as "a service triggers capability verification failures consecutively within a short time." Upon triggering, Core-0 can immediately suspend the service and notify the security service.

### 10.2 Post-Incident Auditing and Analysis

- **Deep Audit Tool**: Can offline parse persisted audit logs to reconstruct security incident timelines and perform threat hunting.
- **Performance Profiling Tool**: Analyzes the latency distribution of system calls using high-precision timestamps in logs to pinpoint performance bottlenecks.

### 10.3 Debugging and Production Support

- **Non-Intrusive Debugging**: A special debugging service with specific capabilities can read another service's memory (if granted the corresponding capability) or receive its log output without stopping the target service.
- **On-Site Error Reporting**: When a service crashes, Core-0 can automatically package parts of its address space (e.g., stack) and send it via a secure channel to developers for debugging assistance.

## 11. Architecture Comparison Summary and Suitability Analysis

| Feature Dimension              | Linux Monolithic Kernel                          | Traditional Microkernel (e.g., seL4)          | Hybrid Kernel (Windows NT)                         | HIC Hierarchical Isolation Kernel                              |
| ------------------------------ | ------------------------------------------------ | ---------------------------------------------- | -------------------------------------------------- | -------------------------------------------------------------- |
| **Driver/Service Location**    | Kernel Space (Shared Ring 0 address space)       | User Space (Ring 3)                            | Hybrid: Critical drivers in Kernel (Ring 0), some services in User (Ring 3) | Privileged Service Sandboxes (Logical Ring 1, Physical Ring 0, independent address spaces) |
| **Core Isolation Mechanism**   | None (all drivers share memory)                  | Hardware Privilege Levels + Inter-Process IPC  | Partial: Kernel drivers vs. user services separated, but kernel drivers still share space | Hardware MMU (Physical Memory Isolation) + Software Capability System (Permission Control) |
| **Performance Critical Path**  | Direct function call, optimal performance        | Privilege switch + IPC copy, high overhead     | Compromise: In-kernel calls fast, cross-privilege calls slower | Same privilege level call + capability check + shared memory, close to monolithic kernel |
| **Driver Failure Impact**      | Kernel crash, entire system down                 | Service process crash, clients using IPC affected | Critical driver crash -> system instability/crash; user service crash -> local impact | Single service sandbox crash, core and other services unaffected, can be quickly restarted |
| **TCB Size**                   | Extremely Large (includes all drivers)           | Very Small (microkernel only)                   | Relatively Large (includes critical drivers and kernel core) | Small (Core-0 + capability system), slightly larger than pure microkernel |
| **Dynamic Support**            | Flexible (runtime kernel modules)                 | Flexible (runtime start user-mode services)    | Flexible (supports kernel modules and user services) | Hybrid (build-time static optimization for core + runtime dynamic sandboxes) |
| **Determinism**                | Low (significant runtime dynamic behavior)       | High (but IPC time affected by load)           | Medium                                                | High (static parts determined at build time, dynamic parts controlled) |
| **Resource DoS Protection**    | Weak (lacks fine-grained quotas)                 | Strong (process resource limits)               | Medium (user processes limited, kernel drivers unlimited) | Strong (hard quotas per service, supports flow control and backpressure) |
| **API Compatibility Evolution**| Relies on kernel ABI and user-mode glibc         | Solved by user-mode libraries                   | User-mode compatibility layer and API sets           | Systematic support: versioned service endpoints + user-mode compatibility libraries |
| **Suitable Scenarios**         | General-purpose servers, desktop, max throughput | Security-critical embedded, military, avionics | Desktop, mobile, general servers (balancing compatibility & performance) | Full spectrum: from IoT embedded to data centers, especially complex systems needing continuous evolution |

### 11.1 In-Depth Comparative Analysis with Hybrid Kernels

Hybrid kernels (like Windows NT and macOS's XNU) attempt to combine the advantages of monolithic and microkernels: placing core functions (scheduling, virtual memory) and performance-critical drivers in kernel space, while running services like file systems and network protocol stacks as user-space services.

**Significant Advantages of HIC over Hybrid Kernels:**

1. **Consistent and Stronger Isolation**:
   - In hybrid kernels, drivers and services running in kernel space still share a single address space; a memory error in one driver can corrupt the entire kernel. Isolation is incomplete.
   - In HIC, **all** drivers and services (regardless of performance criticality) run in independent, MMU-enforced physical memory sandboxes, achieving uniform and complete fault isolation.

2. **Smaller Trusted Computing Base (TCB)**:
   - The TCB of a hybrid kernel includes all code residing in kernel space, which remains very large.
   - HIC's TCB is strictly limited to Core-0 and the core capability system, much smaller than a hybrid kernel, making it easier to formally verify and audit for security.

3. **Better Performance Predictability**:
   - In hybrid kernels, interaction between user-space services and the kernel still requires expensive privilege level switches and context switches.
   - HIC, by running services in sandboxes at the same privilege level as the kernel, eliminates this overhead, making performance more predictable, which is especially beneficial for real-time applications.

4. **Finer Resource Control**:
   - Hybrid kernels lack fine-grained resource limits for kernel-space components.
   - HIC's capability system and quota mechanisms allow precise control over resource usage for every service sandbox, which is crucial for defending against internal attacks and guaranteeing Quality of Service (QoS).

**Potential Challenges of HIC (compared to hybrid kernels):**

1. **Implementation Complexity**: Implementing strong memory isolation at the same physical privilege level requires a carefully designed capability system and Core-0, increasing complexity compared to traditional hybrid kernel architectures.
2. **Compatibility**: Hybrid kernels often have better compatibility with existing drivers designed for monolithic kernels (with some adaptation). HIC requires drivers to be rewritten or adapted for the Privileged-1 service sandbox model, or to provide specific compatibility layers.

## 12. Simplified Design for No-MMU Architectures

The HIC architecture by default assumes the target processor has a full Memory Management Unit (MMU). However, for some extremely resource-constrained embedded microcontrollers (e.g., ARM Cortex-M series) or real-time control systems, an MMU may be absent or unusable. For such cases, HIC offers a simplified design variant that maintains core isolation principles in a no-MMU environment.

### 12.1 Physical Space Flat Mapping Model

When the target platform lacks an MMU, HIC adopts a **physical space flat mapping model**:

1. **Single Physical Address Space**: The entire system operates within a single physical address space, with no virtual address translation. Code and data for all domains (Core-0, Privileged-1 services, Applications) use physical addresses directly.

2. **Static Layout Allocation**: At build time, the hardware synthesis system allocates fixed, non-overlapping physical memory regions for each domain. The locations and sizes of these regions are determined at compile time and hardcoded in the linker script.

**Typical Memory Layout Example** (x86_64 architecture):

| Address Range           | Size     | Purpose                         | Permissions     |
| ----------------------- | -------- | ------------------------------- | --------------- |
| `0x00000000-0x000FFFFF` | 1MB      | Reserved (BIOS, IVT, etc.)      | -               |
| `0x00100000-0x003FFFFF` | 2MB      | Core-0 Kernel Code               | Read-only+Exec  |
| `0x00400000-0x005FFFFF` | 2MB      | Core-0 Kernel Data               | Read-Write      |
| `0x00600000-0x007FFFFF` | 2MB      | Core-0 Kernel Stack              | Read-Write      |
| `0x00800000-0x00FFFFFF` | 8MB      | Privileged-1 Services            | Read-Write+Exec |
| `0x01000000-0x01FFFFFF` | 16MB     | Application-3 Region             | Read-Write+Exec |
| `0x02000000-0x0FFFFFFF` | 224MB    | Shared Memory Region             | Read-Write      |
| `0x10000000-0xFFFFFFFF` | ~3.75GB  | Device Mapped Region             | Read-Write      |

3. **Alternative Memory Protection Mechanisms**:
   - **MPU (Memory Protection Unit)**: If the processor provides an MPU (e.g., ARM Cortex-M), it is used to set up a limited number of protection regions (typically 8-16) for each domain. Core-0 dynamically reconfigures the MPU during domain switches, ensuring each domain can only access its authorized regions.
   - **Pure Software Protection**: Without an MPU, isolation relies on dynamic verification by the capability system. Before each memory access, Core-0 performs a software check to see if the address falls within the domain's authorized range. This mode has lower performance but still provides basic protection.

**Implementation Details** (see `src/Core-0/nommu.h` and `src/Core-0/nommu.c`):

```c
/* Address translation: No translation, virtual address = physical address */
#define NOMMU_VIRT_TO_PHYS(vaddr)  ((phys_addr_t)(vaddr))
#define NOMMU_PHYS_TO_VIRT(paddr)  ((void *)(paddr))

/* Memory region checks */
bool nommu_is_core0_region(phys_addr_t addr, size_t size);
bool nommu_is_privileged_region(phys_addr_t addr, size_t size);
bool nommu_is_application_region(phys_addr_t addr, size_t size);
bool nommu_is_shared_region(phys_addr_t addr, size_t size);
bool nommu_is_device_region(phys_addr_t addr, size_t size);
```

### 12.2 Redefinition of Privilege Levels

On no-MMU ARMv7-M or RISC-V machine mode, typically only one or two privilege levels exist:

1. **Single Privilege Level Mode**:
   - Core-0 and all Privileged-1 services run at the same privilege level (e.g., ARM Handler mode).
   - Isolation relies entirely on static memory layout and runtime verification by the capability system.
   - Applications run at a lower privilege level (e.g., ARM Thread mode), entering the higher privilege level via system calls (traps).
2. **Core Responsibilities Unchanged**: Core-0 remains the system arbiter, responsible for capability verification, scheduling, and exception handling, but its code shares the same address space as privileged services.

### 12.3 Adjustments to Isolation Mechanisms

1. **Weakened Memory Isolation**: Without hardware-enforced isolation, a service fault (e.g., wild pointer) could corrupt the memory of neighboring services or Core-0. Mitigations include:
   - **Guard Pages**: Insert "guard pages" (typically 4KB aligned unmapped regions) between services to detect out-of-bounds accesses (via MPU or software exceptions).
   - **Copy-on-Write (CoW)**: For shared data, employ a copy-on-write mechanism to prevent accidental modification.
2. **Strengthened Capability System**: The capability system becomes the primary isolation mechanism in a no-MMU environment. All resource accesses (including memory reads/writes, device I/O) must pass capability verification, even if the address falls within a legal range.
3. **Communication Mechanisms**: Due to the lack of zero-copy shared memory (all memory is physically contiguous), inter-service communication shifts to message passing via Core-0, requiring data copying under Core-0's control.

### 12.4 Building and Deployment Without MMU

1. **Purely Static Synthesis**: No-MMU systems only support static synthesis mode; all services are linked into a single image at build time.
2. **Link-Time Conflict Detection**: The build system must ensure the physical memory regions of different domains do not overlap and resolve all symbol references during the link stage.
3. **Simplified Interrupt Handling**: Interrupts are dispatched directly by Core-0 to registered service handlers, with no address space switch overhead.
4. **Enhanced Determinism**: With no TLB and no page table walks, the system's timing behavior is more deterministic, suitable for hard real-time scenarios.

**Build Configuration** (see `build_config.mk`):

```makefile
# Architecture configuration
CONFIG_MMU ?= 0             # Enable MMU support (0=disable, 1=enable)
CONFIG_MPU ?= 1             # Enable MPU support (0=disable, 1=enable)
CONFIG_PHYSICAL_MAPPING ?= 1  # Physical memory direct mapping (0=disable, 1=enable)
```

**Build Commands**:

```bash
# Build no-MMU version
make CONFIG_MMU=0 CONFIG_MPU=1

# Build no-MPU version (pure software protection)
make CONFIG_MMU=0 CONFIG_MPU=0
```

**Build System Integration** (see `build/Makefile`):

```makefile
# Include build configuration
-include ../build_config.mk

# Architecture configuration options
ifeq ($(CONFIG_MMU),0)
    CFLAGS += -DCONFIG_MMU=0 -DNOMMU
else
    CFLAGS += -DCONFIG_MMU=1
endif

# no-MMU architecture support
$(OBJ_DIR)/nommu.o: ../src/Core-0/nommu.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I../src/Core-0 -I../src/Core-0/include -I../src/Core-0/lib -I../src/Core-0/arch/x86_64 -I../src/Privileged-1 -c $< -o $@
```

### 12.5 Applicable Scenarios and Limitations

**Applicable Scenarios**:

- Deeply embedded systems (IoT nodes, sensors, actuators)
- Real-time control systems (aerospace, industrial automation)
- Extremely resource-constrained microcontrollers (memory < 1MB)

**Limitations**:

- **Weakened Fault Isolation**: A fault in one service could trigger cascading failures, requiring reliance on high-reliability coding and static verification.
- **Limited Dynamicity**: Cannot support runtime module loading or hot upgrades.
- **Lower Memory Efficiency for Multi-Tasking**: Due to fixed physical memory partitioning, internal fragmentation may occur.
- **Reduced Security Level**: Cannot achieve the highest security assurance levels (e.g., CC EAL6+/EAL7).
