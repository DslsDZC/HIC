# HIK (Hierarchical Isolation Kernel) Architecture Reference Documentation

Author: DslsDZC
Email: dsls.dzc@gmail.com

## 1. Design Philosophy and Fundamental Goals

HIK (Hierarchical Isolation Kernel) aims to construct an operating system kernel capable of adapting from resource-constrained embedded devices to feature-rich general-purpose computing systems through a unified architectural paradigm. Its core goal is to achieve the trinity of ultimate performance, strong security isolation, and dynamic extensibility within a single architectural framework.

Core Design Principles:

1. **Complete Separation of Mechanism and Policy:**
   · The kernel core (Core-0) provides only atomic, policy-free execution primitives, isolation mechanisms, and the capability system.
   · All system policies and functional implementations are provided by loadable modular services in the **Privileged-1 layer**, customizable via **build-time configuration** or **runtime dynamic management**.
2. **Logical Three-Tier Privilege Architecture:**
   · Constructs a logical three-tier control flow: Untrusted Applications (Ring 3), Modular Privileged Service Sandboxes (Logical Ring 1), Kernel Core and Arbiter (Logical Ring 0).
   · This model unifies isolation and performance by leveraging the Memory Management Unit (MMU) and a software capability system at the highest physical privilege level (e.g., x86 Ring 0) to create strongly isolated "in-kernel processes."
3. **Unified Architecture, Flexible Deployment:**
   · The same architecture simultaneously supports two deployment modes:
     · **Static Synthesis Mode:** For embedded, real-time, or high-security scenarios. All service modules are compiled and linked into a single, deterministic kernel image at build time.
     · **Dynamic Modular Mode:** For general-purpose, desktop, or server scenarios. Core services are statically compiled, while extension services (e.g., device drivers, filesystems, protocol stacks) can be securely installed, updated, and unloaded at runtime via network repositories.
4. **Strong Backward Compatibility and API Evolution:**
   · The system kernel and Privileged-1 services provide a stable userspace ABI (Application Binary Interface).
   · Through modular services and userspace compatibility libraries, full compatibility for legacy APIs is provided transparently while supporting new APIs, ensuring long-term stability of the application ecosystem.

## 2. Detailed Three-Tier Model Architecture (Direct Physical Memory Mapping Scheme)

This chapter details the core architectural model of HIK, especially the design of the Privileged-1 layer using direct physical memory mapping and allocation limits.

### 2.1 Core-0 Layer: Kernel Core and Arbiter

· **Physical Execution Environment:** CPU's highest privilege mode (x86 Ring 0, ARMv8-A EL1 or EL2).
· **Logical Role:** The system's Trusted Computing Base (TCB), final arbiter of resource access, enforcer of isolation mechanisms.
· **Design Goal:** Minimalist, amenable to formal verification. Its code size (excluding architecture-specific assembly and auto-generated data) is strictly limited to a target of less than 10,000 lines of C code.

Core Responsibilities and Implementation Details:

1. **Physical Resource Management and Allocation:**
   · Manages global bitmaps for all physical resources: memory frames, CPU time slices, hardware interrupt lines, etc.
   · Adopts a **direct physical memory allocation** strategy, avoiding traditional virtual memory management overhead. Allocates independent, contiguous physical memory regions for each isolation domain (including itself, each Privileged-1 service, each Application) and isolates them via the MMU.
   · Core-0's own code and data reside in a fixed physical memory region marked as inaccessible in the page tables of all other domains.
2. **Capability System Kernel:**
   · Maintains a global, protected **capability table**. Each entry (capability) is an unforgeable kernel object describing specific access permissions (e.g., read, write, execute) to a system resource (e.g., a range of physical memory, a range of hardware I/O ports, an IPC endpoint).
   · Each isolation domain is associated with a **"capability space"** – an array of capability indices that are the domain's "held" capability handles. A domain can only request resource operations via the handles in its capability space.
   · Provides system calls for transferring, deriving (creating subsets of permissions), and revoking capabilities between domains. All operations undergo strict permission and ownership checks.
3. **Execution Control and Scheduling:**
   · Manages all thread control blocks (TCBs). Each thread is bound to a specific isolation domain.
   · Implements a predictable, real-time-friendly scheduler. Scheduling decisions can be based on build-time configured static priorities or simple round-robin policies.
   · Handles all exceptions and hardware interrupts. Interrupts are first delivered to Core-0, which, based on a build-time configured **interrupt routing table**, directly calls the corresponding interrupt handler function registered by a Privileged-1 service (via a protected entry point).
4. **Isolation Enforcement:**
   · When creating a Privileged-1 service domain, Core-0 configures its page table to map only the following regions:
     a) The physical memory region containing the service's own code and private data segments.
     b) Shared physical memory regions or device MMIO regions explicitly authorized by capabilities.
   · A service domain cannot modify its own page table. Any illegal memory access triggers a page fault or access permission exception, which Core-0 converts into a service fault signal.
   · Control flow transfer between services must go through **synchronous call gates** defined by Core-0. Call gates verify that the initiator holds the target service endpoint capability and perform stack switching to ensure a controlled return address.

### 2.2 Privileged-1 Layer: Privileged Service Sandboxes (Direct Physical Memory Mapping)

· **Physical Execution Environment:** Same as Core-0 (x86 Ring 0, ARMv8-A EL1/EL2).
· **Logical Role:** Modular providers of system functionality. Each service is an independent, mutually distrusting "kernel-mode process."
· **Key Design: No Traditional Virtual Address Space**
  · **Direct Physical Memory Mapping:** Each Privileged-1 service is allocated one or more contiguous blocks of physical memory. The service's internal code and data run and are accessed directly at these physical addresses, without extra virtual address translation (except for the page table mapping required by the CPU MMU, which is an **identity mapping**, i.e., virtual address equals physical address offset). This eliminates virtual memory management overhead and ensures deterministic memory access latency.
  · **Physical Space Isolation:** The physical memory regions of different services do not overlap in address and are isolated from each other via MMU page table permissions. A service cannot access the physical memory of another service or Core-0 unless explicitly authorized via the capability system to share.
  · **Physical Resource Allocation Limits:** A hard quota limit for physical memory, CPU time, I/O ports, etc., is imposed on each service via the capability system at build time or module installation time. This is key to service isolation, ensuring a single service cannot exhaust system resources.

Core Features and Implementation Details:

1. **Independent Physical Address Space:**
   · Each service (e.g., network driver, filesystem, display service) runs in an independent physical memory region allocated by Core-0.
   · The service's view of the address space is contiguous and physical, simplifying internal memory management and avoiding the non-determinism and performance overhead of virtual memory management.
2. **Capability-Based Resource Access:**
   · Upon startup, a service receives an initial capability set from Core-0, configured at build time or dynamically passed from a parent service.
   · All service access to hardware (e.g., in/out instructions, MMIO access) or shared memory with other domains is validated by Core-0 (or under its supervision) via the capability system before instruction execution.
3. **Fault Isolation and Recovery:**
   · Service crashes (illegal instruction, page fault, division by zero, etc.) are confined to its own physical address space. After Core-0 captures the exception:
     a) It terminates all threads within that service domain.
     b) It reclaims all capabilities and physical resources (memory, devices) held by the domain.
     c) It sends a notification event to a pre-designated **"Monitor Service"** responsible for system management.
   · The Monitor Service can decide based on policy whether to restart a new service instance and restore state from persistent storage (if the service design supports it).
4. **Efficient Communication Mechanisms:**
   · **Communication with Applications:** Batched, zero-copy data exchange via shared physical memory regions established at build time or initialization and authorized by capabilities. Synchronization typically uses lock-free ring buffers and memory barriers.
   · **Communication with Core-0:** Triggered via the `syscall` instruction, handled by Core-0 for capability verification and request routing.
   · **Inter-Service Communication:** Primarily two methods:
     a) **Indirect Communication:** Messages routed through Core-0 (for control flow).
     b) **Direct Communication:** Two services perform zero-copy data transfer via a shared physical memory block for which they both hold capabilities (for data flow).

### 2.3 Application-3 Layer: Application Layer

· **Physical Execution Environment:** CPU low-privilege mode (x86 Ring 3, ARMv8-A EL0).
· **Logical Role:** Execution environment for untrusted user code.
· **Core Features:**
  · Runs in a standard process abstraction with an independent virtual address space (managed by Core-0 via the MMU).
  · All requests for system resources must be initiated through operating system APIs (ultimately pointing to Privileged-1 layer services) and authorized by the Core-0 capability system.
  · Applications have no direct memory access to each other or to privileged services; communication is via IPC channels established by the kernel.

## 3. In-Depth Analysis of Core Security Mechanisms

### 3.1 Dynamic Lifecycle and Secure Transfer of Capabilities

Capabilities are the carriers of all authorization in HIK. Their security does not rely on storage location but on interpretation by the kernel (Core-0).

1. **Generation:** Capabilities are initially generated at build time by the hardware synthesis system based on configuration, or dynamically created by Core-0 at runtime in response to resource allocation requests (e.g., mmap).
2. **Storage:** The "master copy" of capabilities is stored in a protected global table inside Core-0. Each domain's held "capability space" only stores obfuscated indices (handles) pointing to entries in this global table.
3. **Transfer and Revocation:**
   · **Send:** Domain A requests to transfer capability C to domain B. Core-0 checks: (a) if A holds C; (b) if B is eligible to receive such a capability according to security policy; (c) if permission attenuation is needed (e.g., read-write capability attenuated to read-only). If verification passes, Core-0 creates a new handle in B's capability space pointing to the same global capability object C (or a derived version of it).
   · **Receive:** B only obtains a local handle; it cannot know the handle value of this capability in A or other domains, nor can it directly manipulate the global table.
   · **Revocation:** When a domain is destroyed or a capability is explicitly revoked, Core-0 removes the capability entry from the global table and immediately invalidates all handles pointing to it in all domains. Subsequent access via invalid handles is denied.

### 3.2 Unified API Access Model and Secure Communication

All cross-domain interactions go through standardized interfaces, eliminating privileged backdoors.

1. **API Gateway:** Each Privileged-1 service registers one or more **"service endpoint capabilities"** with Core-0. Applications or other services initiate requests by calling the `ipc_call(cap_endpoint, message)` system call.
2. **Call Path:**
   · Core-0 intercepts the call and verifies that the caller holds the target endpoint capability.
   · Core-0 securely switches the call context (registers, message content) to the target service domain.
   · After the target service completes processing and returns, Core-0 switches the result context back to the caller.
3. **Secure Establishment of Shared Memory Channels:**
   · The establishment process must be arbitrated by Core-0. For example, Application A requests shared memory with Service B.
   · Core-0 allocates physical memory and maps it into A's virtual address space and B's physical address space, possibly with different permissions (A write-only, B read-only).
   · Core-0 creates two **"memory capabilities"** granted to A and B respectively, precisely describing their respective access permissions. Any access beyond these permissions triggers Core-0's exception handler.

### 3.3 Security Auditing and Tamper-Resistant Logging

Auditing is key for post-facto accountability and intrusion detection.

1. **Log Storage:** During system initialization, Core-0 allocates a block of physical memory as an **append-only** audit log buffer. Core-0 maps this buffer read-only to itself and a dedicated, highly trusted **"Audit Service."**
2. **Log Recording:** When performing any critical security operation (capability verification success/failure, service creation/destruction, privileged calls), Core-0 atomically appends a structured log entry to the end of the buffer.
3. **Tamper Resistance:**
   · No other service (including most Privileged-1 services) can obtain write capability to this buffer.
   · The Audit Service periodically reads log blocks, computes hashes, and may write encrypted logs to persistent storage. Even if runtime memory is physically tampered with by an attacker, the persisted encrypted logs ensure post-facto detection.
4. **Deterministic Service Recovery:** Service recovery after a crash is driven by the **"Monitor Service."** Core-0 provides atomic resource reclamation primitives. The Monitor Service decides whether and how to restart a new instance based on preset policies (e.g., restart count, dependencies). The new instance starts from an initial state or loads from a persistent snapshot, ensuring system state remains predictable.

## 4. Build-Time Hardware Synthesis System

This is a key compilation stage that transforms hardware uncertainty into software determinism.

**Input:** A machine-readable hardware description file (e.g., `platform.yaml`), containing:
· Number of CPUs, memory topology, Cache information.
· List of all devices enumerated via PCI/device tree, with their MMIO addresses, interrupt numbers (e.g., GSI), DMA ranges.
· Required list of system services and their dependencies.
· Security policies: initial capability allocation, service resource quotas, communication permissions.

**Processing Flow:**

1. **Parsing and Conflict Resolution:** The system parses the description file, checks for resource conflicts (e.g., two devices claiming the same interrupt), and resolves them according to predetermined policies (reassignment or error).
2. **Generate Privileged Service Code:**
   · Selects or generates device driver code from a template library, optimized for the specific device models specified in the description file (e.g., loop unrolling, inlining specific register operations).
   · Links the driver code with the corresponding service framework (e.g., network protocol stack service framework), compiling it into an independent binary module that will later be loaded as a Privileged-1 service.
3. **Generate System Static Configuration:**
   · **Memory Layout Table:** Determines the exact physical memory layout for each service, kernel core, and shared memory area.
   · **Interrupt Routing Table:** Specifies, for each hardware interrupt number, the Privileged-1 service that handles it and its handler function entry point.
   · **Initial Capability Allocation Table:** Defines the initial capability set each service domain receives at system startup.
   · **Device Initialization Sequence:** Defines the initialization call order for all device drivers during system startup, resolving inter-device dependencies.
4. **Final Image Synthesis:** Links the Core-0 kernel, all generated Privileged-1 service binary modules, and all the aforementioned configuration data tables to produce a single, bootable kernel image file.

**Output Characteristics:** This image is fully customized for specific hardware, containing optimized code paths and deterministic resource bindings, with no runtime "discovery" overhead.

## 5. Dynamic Device Support and Modular System

Building upon build-time determinism, HIK provides a controlled dynamic extension mechanism to handle real-world hot-plugging needs (e.g., USB, NVMe SSD).

1. **Dynamic Resource Pool:**
   · At build time, a portion of total physical resources (e.g., a set of MSI-X interrupt vectors, a reserved range of PCIe BAR space) is marked as a **"Dynamic Pool"** and not allocated to any static service.
   · A special **"Hot-Plug Coordination Service"** is created and holds the top-level capability for managing this dynamic pool.
2. **Secure Driver Modules:**
   · Driver modules exist as signed binary blobs. Signatures are provided by the system builder or trusted third parties.
   · The module format includes its resource requirements (needed interrupts, DMA memory) and external interfaces.
3. **Hot-Plug Workflow:**
   · **Discovery:** Device insertion triggers an interrupt. Core-0, based on the interrupt routing table, dispatches this interrupt event to the "Hot-Plug Coordination Service."
   · **Loading and Verification:** The coordination service, based on device ID, loads the corresponding driver module from trusted storage and verifies its digital signature and integrity.
   · **Create Service Sandbox:** The coordination service requests Core-0 to create a new, empty Privileged-1 service address space (allocating physical memory and configuring the page table).
   · **Resource Allocation:** The coordination service allocates required resources (interrupts, memory, etc.) from its managed dynamic pool and grants the corresponding capabilities to the newly created service sandbox via Core-0.
   · **Initialization and Registration:** The coordination service loads the verified driver module code and data into the new sandbox's physical memory and starts its initialization routine. After successful initialization, the new driver service registers its provided functionality (e.g., a new block device node) with the system.
   · **Removal:** Upon device removal, the coordination service notifies the driver service to clean up, then requests Core-0 to destroy that service sandbox and reclaim all dynamic resources allocated to it.

**Key Design:** Dynamically loaded drivers operate under the exact same security model as statically compiled drivers—they run in independent Privileged-1 sandboxes constrained by the capability system. Dynamism does not compromise core isolation principles.

## 6. Modular Service Architecture and Rolling Updates

### 6.1 Service Module Format and Secure Distribution

· **Module Format (.hikmod):**
  · **Module Header:** Contains magic number, format version, module UUID, semantic version number, API descriptor offset, code segment size, data segment size, signature information offset.
  · **Metadata Segment:** Contains module name, description, author, list of exported service endpoints (name, ID, version), declared resource requirements, list of dependent modules (UUID + version constraints).
  · **Code Segment:** Contains relocatable machine code (architecture-specific, e.g., x86-64, ARM64), with entry points conforming to the Privileged-1 Service ABI.
  · **Data Segment:** Contains read-only global data and initialized read-write data.
  · **Signature Segment:** Contains a cryptographic hash (e.g., SHA-384) of the header, metadata, code, and data segments, and the digital signature of the publisher or developer.
· **Secure Distribution and Repository:**
  · The system maintains one or more trusted module repositories (local or network). Repositories provide module indices, metadata, and .hikmod file downloads.
  · Each module is uniquely identified in a repository by its `UUID@version`.
  · Before installation, a module's digital signature must be verified to ensure it is published by a trusted party and untampered.

### 6.2 Module Manager Service

This is a core system service running in the Privileged-1 layer, responsible for the full lifecycle management of modules.

Core Functions:

1. **Repository Interaction:** Fetches module indices from configured repositories, supports querying and downloading modules.
2. **Dependency Resolution:** When installing or updating modules, computes the complete dependency graph, resolves version conflicts, and ensures system consistency.
3. **Security Verification:** Invokes cryptographic primitives provided by Core-0 to verify module signature and integrity.
4. **Loading and Instantiation:**
   a. Requests Core-0 to create a new, empty Privileged-1 service sandbox.
   b. Loads the verified module code and data into the sandbox's physical memory.
   c. Requests Core-0 to allocate declared resources for it (translated into capabilities) based on the module's metadata.
   d. Calls the module's initialization entry point to start the service.
   e. Registers the service's provided endpoints in Core-0's system service registry.
5. **Upgrade and Rollback:**
   · **Rolling Update (Hot Upgrade):** For services supporting state migration, the Module Manager can:
     · Start a new sandbox for the new version module in parallel.
     · Gradually migrate client connections and internal state from the old instance to the new instance using the service's built-in state migration protocol.
     · Terminate the old instance after migration completes. This process is transparent to applications, achieving zero-downtime updates.
   · **Atomic Replacement (Cold Upgrade):** For stateless or simple restart services, the Module Manager can schedule a restart, atomically replacing the old module after the new module is verified.
   · **Rollback Mechanism:** Before each installation or upgrade, the old version module's binary and configuration are securely archived. If the new module fails to start or encounters serious runtime faults, the Module Manager can automatically or on instruction roll back to the previous known-good version.
6. **Garbage Collection:** Unloads old module versions no longer depended upon by any module, reclaiming storage space.

### 6.3 API Version Management and Coexistence of Multiple Implementations

To ensure long-term compatibility, the system supports the coexistence of multiple API version implementations for the same service.

Implementation Mechanism:

1. **Versioned Service Endpoints:** Service modules must declare the API version number they implement when registering endpoints (e.g., `filesystem.v1.open`, `filesystem.v2.open_with_flags`).
2. **Client Binding:**
   · An application (or compatibility library), when first calling a service, queries Core-0's registry for available endpoint versions.
   · It can explicitly request a specific version or request the "default" version (usually the latest stable).
   · Core-0, based on binding policy, authorizes the client's capability to point to the corresponding version's service instance.
3. **Multi-Instance Parallelism:** `filesystem.v1` and `filesystem.v2` can run simultaneously as two independent Privileged-1 service sandboxes, each with its own state and resources. They can share underlying block device drivers (via capability authorization) but are completely isolated in filesystem logic.
4. **Bridging and Adapters:** For complex API evolutions, specialized adapter service modules can be developed. For example, a `posix-legacy-adapter` service module can receive traditional POSIX system calls and translate them into internal calls to new `filesystem.v2` and `vfs.v2` services, transparently supporting legacy applications.

### 6.4 Differentiated Deployment for Embedded and General-Purpose Systems

The HIK architecture, through build and configuration toolchains, supports two extreme deployment modes:

1. **Embedded / Static Synthesis Mode:**
   · **Process:** Developers write a `system.yaml` configuration file listing all required service modules (including drivers, protocol stacks, etc.) and their versions. The build system (hardware synthesis system) statically links these modules with Core-0 to generate a single, solidified kernel image.
   · **Characteristics:**
     · **No runtime module loading:** All services are ready upon system boot, no dynamic loading overhead.
     · **Ultimate determinism:** Memory layout, interrupt assignment, call paths are entirely determined at build time, suitable for Functional Safety (FuSa) certification.
     · **Minimized storage footprint:** No module repositories, no extra versions, only necessary code.
     · **Fast boot speed:** No runtime dependency resolution and module loading.
2. **General-Purpose / Dynamic Modular Mode:**
   · **Process:** Releases a base system image containing Core-0 and a set of core statically linked services (e.g., Module Manager, base drivers, network stack). After boot, the Module Manager service runs automatically, downloading and loading other service modules on-demand from network repositories (e.g., graphics drivers, sound card drivers, filesystem format support, advanced API services).
   · **Characteristics:**
     · **Highly flexible:** Users can dynamically install required drivers and features based on hardware and peripherals.
     · **Updatability:** Bug fixes or feature upgrades for individual services can be done by replacing modules, without replacing the entire kernel or restarting critical services (supports rolling updates).
     · **Storage efficiency:** Base image is small; additional features are acquired on-demand.
     · **Ecosystem friendly:** Facilitates independent development and distribution of driver modules by hardware vendors.

## 7. Operational Procedure for Rolling Updates and System Evolution

Scenario: Securely upgrading the system's network protocol stack from `netstack.v1.2.0` to `netstack.v1.3.0`.

1. **Update Preparation:**
   · The Module Manager fetches the `netstack.v1.3.0` module file, signature, and metadata from the repository.
   · Verifies the signature and integrity.
   · Resolves dependencies: confirms `netstack.v1.3.0` is compatible with other modules in the current system.
   · Notifies potentially affected dependent services (e.g., firewall service, web server) to prepare for connection migration.
2. **New Instance Creation and Warm-up:**
   · Module Manager requests Core-0 to create a new Privileged-1 sandbox.
   · Loads and initializes the `netstack.v1.3.0` module.
   · Allocates temporary network ports and buffer resources (capabilities) for the new instance.
   · The new instance starts internal services but does not immediately take over external network traffic. It may establish a control connection with the old instance to synchronize routing tables, connection states, etc. (if supported by the protocol).
3. **Traffic Migration (Hot Upgrade):**
   · The Module Manager coordinates between the old v1.2.0 instance and the new v1.3.0 instance.
   · For each active network connection (e.g., TCP socket), the old instance migrates the connection descriptor and state information to the new instance via a secure channel. This process may involve Core-0 remapping capabilities (e.g., transferring a memory capability containing network packet buffers from the old instance to the new instance).
   · During migration, client applications are unaware. A few packets might be processed twice, handled by the protocol stack's sequence number mechanism.
4. **Switchover and Cleanup:**
   · After all connections or critical connections are migrated, the Module Manager updates Core-0's service registry to point the default `netstack` endpoint to the v1.3.0 instance.
   · New client connections are routed to the new instance.
   · The Module Manager notifies the old v1.2.0 instance to enter a shutdown process, waiting for remaining connections to time out or close.
   · Old instance resources are fully reclaimed by Core-0, and its module binary can be marked for unloading.
5. **Rollback Contingency:**
   · Throughout the process, the `netstack.v1.2.0` instance and module file are always preserved.
   · If the new instance encounters a serious fault during initialization or migration, the Module Manager can abort the upgrade, switch traffic back to the old instance, and log the error.

## 8. Resource Management and Protection Mechanisms

### 8.1 Build-Time Resource Planning and Static Guarantees

· **Service-Level Quotas:** Define hard upper limits in the build configuration for each Privileged-1 service: maximum physical memory usage, CPU time share, maximum I/O operations per second.
· **Worst-Case Execution Time (WCET) Analysis:** Static analysis or measurements determine WCET for Core-0's critical paths (e.g., scheduler, interrupt handling) and real-time critical service code, serving as the basis for system schedulability analysis.
· **Dependency Analysis:** The build system analyzes communication dependencies between services, ensuring resource quotas satisfy the cascading needs of service chains.

### 8.2 Runtime Resource Management

· **Streaming Processing and Backpressure:** For data processing services (e.g., video decoding), adopt a producer-consumer pipeline model. When the consumer cannot keep up, a "backpressure" signal is passed back through the pipeline, causing the producer to pause, preventing unlimited buffer growth and memory exhaustion.
· **Incremental Allocation:** Services can request resources in phases. For example, a network service might initially request a small buffer pool at startup, then dynamically request more as connections grow. This improves resource utilization.
· **Stress-Aware Scheduling:** Core-0 monitors system load (e.g., run queue length, memory pressure). Under high pressure, it can dynamically lower the priority or scheduling frequency of non-critical background tasks to ensure response times for critical tasks (e.g., interrupt handling, control plane services).

### 8.3 Memory Optimization

· **Layering by Access Pattern:**
  · **Hot Data:** Active working set, kept uncompressed, mapped in TLB-friendly regions.
  · **Warm Data:** Possibly accessed again, can use lightweight compression (e.g., LZ4).
  · **Cold Data:** Rarely accessed, can use high-ratio compression (e.g., Zstandard), transparently decompressed upon access.
· **Transparent Compression Service:** Exists as a Privileged-1 service. Other services can "swap" memory pages to it for compression/decompression. The compression service holds capabilities for compressed data pages; the original service holds capabilities for decompressed data pages; Core-0 manages the switching between them.

### 8.4 Resource Flood and Denial-of-Service Attack Protection

**Resource Flooding** is a typical Denial-of-Service (DoS) attack where an attacker exhausts system critical resources (e.g., memory, file descriptors, connection count) to paralyze the system. HIK addresses such attacks with a multi-layered defense mechanism:

1. **Hard Upper Limits Based on Build-Time Quotas:**
   · Each Privileged-1 service and Application is assigned explicit resource quotas at creation. For example, a network service can hold at most 1024 connection descriptors and 64MB of buffer pool. These are unbreakable hard limits enforced by Core-0 during resource allocation.
   · This static quota mechanism fundamentally prevents a single service from exhausting global resources due to attack or fault.
2. **Hierarchical Resource Quotas and Delegation:**
   · When a service needs to allocate resources for multiple clients (e.g., a web server allocating buffers for each connection), it must "split" its own quota into sub-quotas for each client. Core-0 supports subdivision and delegation of capabilities, so a service can create sub-capabilities with smaller limits for each client.
   · Thus, even if one client behaves abnormally (e.g., slow reading causing buffer buildup), it only exhausts its allocated sub-quota, without affecting the service itself or other clients.
3. **Real-Time Monitoring and Adaptive Rate Limiting:**
   · Core-0 and resource monitoring services continuously track each domain's resource usage. When a domain's resource usage exceeds a threshold (e.g., 80%) of its quota, the monitoring service issues a warning.
   · For resource requests initiated by the Application layer (e.g., memory allocation via system calls), Core-0 can implement adaptive rate-limiting algorithms. For instance, if an application is detected allocating memory at an abnormally high rate, Core-0 can dynamically reduce its allocation rate or add delay to subsequent requests.
4. **Flow Control for Communication Endpoints:**
   · All IPC endpoints and shared memory channels can be configured with flow control policies. This includes:
     · **Credit-Based Flow Control:** The sender must obtain "credits" from the receiver to send data. The receiver grants credits based on its own processing capacity.
     · **Queue Depth-Based Backpressure:** When the receiver's message queue reaches a certain depth, Core-0 can temporarily block the sender or return a "resource temporarily unavailable" error.
   · This prevents fast producers from overwhelming slow consumers and is key to defending against application-layer flooding attacks.
5. **Global Degradation and Isolation in Emergency States:**
   · When overall system resource pressure exceeds a safety threshold (e.g., total free memory below 5%), the system enters an emergency state.
   · Core-0 can then trigger the following global protection measures:
     a) **Prioritize Critical Services:** Suspend or terminate non-critical services and tasks based on build-time defined priorities.
     b) **Reject New Requests:** Return failure for all non-critical new resource allocation requests.
     c) **Attack Source Isolation:** If the resource monitoring service identifies a suspected attack source through algorithms (e.g., an application triggering many capability verification failures in a short time), it can request Core-0 to temporarily freeze that application domain for in-depth inspection.
6. **Timeliness of Capability Reclamation:**
   · When a service or application crashes, Core-0 ensures all its held capabilities and resources are reclaimed atomically and immediately. This prevents resources from "leaking" due to process crashes, which could be exploited by attackers (gradually exhausting resources by repeatedly crashing processes).

This combined defense mechanism ensures that when facing resource flooding attacks, the HIK system can limit the impact scope (via quotas), provide early warning (via monitoring), maintain core functionality (via priority guarantees), and achieve rapid recovery (via atomic reclamation).

## 9. Performance Optimization System and Expected Metrics

HIK's performance advantages stem from deep optimization of critical paths in the architectural design.

1. **System Call / Service Call Latency:**
   · **Traditional Microkernel:** Requires full privilege-level switching (save/restore many registers) + independent address space switch (page table switch) + IPC message copying. Latency is often in the microsecond range.
   · **HIK (Direct Physical Mapping):** When calling a Privileged-1 service, there is no privilege-level switching, no page table switch (the service and caller may be in the same physical address space, or jump via pre-established, TLB-resident shared pages). The main overhead is Core-0's capability verification (a few memory table lookups) and controlled context jump. Design target: **20-30 nanoseconds**.
2. **Interrupt Handling Latency:**
   · **Traditional System:** Interrupt → kernel generic entry → save context → call driver ISR (may trigger scheduling) → restore context. Latency often ranges from a few microseconds to tens of microseconds.
   · **HIK:** Interrupt → Core-0 simplified entry (save few critical registers) → directly call the ISR registered by the Privileged-1 service based on the static routing table (same privilege-level function call). Design target: **0.5-1 microsecond**.
3. **Thread Switching Latency:**
   · Occurs within Core-0, switching to another thread at the same physical privilege level. Only needs to switch general-purpose registers, stack pointer, and thread-private data pointer; no page table switch unless crossing domains. Design target: **120-150 nanoseconds**.
4. **Communication Bandwidth:**
   · Inter-service communication via shared memory achieves true zero-copy. The upper bandwidth limit depends on memory copy speed or DMA speed, with no additional kernel copying overhead.

**Performance Data Note:** The above metrics are theoretical expectations based on architectural design. Achieving them depends on: (a) manual or compiler optimization of core path code; (b) good alignment of key data structures and control paths with CPU cache characteristics; (c) the hardware itself providing low-latency system call and interrupt response mechanisms. Actual performance must be ultimately validated on specific hardware prototypes using benchmarks (e.g., LMbench, cyclictest).

## 10. Security Auditing and Monitoring System

### 10.1 Real-Time Monitoring

· **Resource Monitoring Service:** A privileged service that periodically reads resource usage statistics (CPU time, memory pages allocated, IPC call counts) for each domain from Core-0 and provides visualization or alerting interfaces.
· **Anomaly Behavior Detection:** Core-0 can be configured with rules, e.g., "a service triggers capability verification failures continuously within a short time." Once triggered, Core-0 can immediately suspend the service and notify security services.

### 10.2 Post-Facto Auditing and Analysis

· **Deep Audit Tools:** Can offline parse persisted audit logs, reconstruct security event timelines, and perform threat hunting.
· **Performance Profiling Tools:** Use high-precision timestamps in logs to analyze system call latency distributions and locate performance bottlenecks.

### 10.3 Debugging and Production Support

· **Non-Intrusive Debugging:** Via a debugging service with special capabilities, it can read memory of other services (if granted the corresponding capability) or receive their log output without stopping the target service.
· **On-Site Error Reporting:** When a service crashes, Core-0 can automatically package part of its address space content (e.g., stack) and send it to developers via a secure channel to aid debugging.

## 11. Architecture Comparison Summary and Applicability Analysis

| Feature Dimension | Linux Monolithic Kernel | Traditional Microkernel (e.g., seL4) | Hybrid Kernel (Windows NT) | **HIK Hierarchical Isolation Kernel** |
| :--- | :--- | :--- | :--- | :--- |
| **Driver/Service Location** | Kernel space (shared Ring 0 address space) | User space (Ring 3) | Mixed: Critical drivers in kernel space (Ring 0), some services in user space (Ring 3) | **Privileged Service Sandboxes (Logical Ring 1, Physical Ring 0 independent address space)** |
| **Core Isolation Mechanism** | None (all drivers share memory) | Hardware privilege level + Inter-process IPC | Partial: Kernel drivers isolated from user services, but drivers within kernel still share space | **Hardware MMU (physical memory isolation) + Software Capability System (permission control)** |
| **Performance Critical Path** | Direct function call, optimal performance | Privilege-level switch + IPC copy, high overhead | Compromise: In-kernel calls fast, cross-privilege calls slow | **Same-privilege call + capability check + shared memory, close to monolithic** |
| **Driver Fault Impact** | Kernel crash, system completely down | Service process crash, affects IPC-connected clients | Critical driver crash causes system instability/crash; user service crash affects locally | **Single service sandbox crash, no impact on core & other services, can restart quickly** |
| **TCB Size** | Very large (includes all drivers) | Very small (only microkernel itself) | Large (includes critical drivers and kernel core) | **Small (Core-0 + capability system), slightly larger than pure microkernel** |
| **Dynamic Support** | Flexible (runtime kernel modules) | Flexible (runtime start user services) | Flexible (supports kernel modules & user services) | **Hybrid (build-time static optimization core + runtime dynamic sandboxes)** |
| **Determinism** | Low (lots of runtime dynamic behavior) | High (but IPC time affected by load) | Medium | **High (build-time determined static part, dynamic part controlled)** |
| **Resource DoS Protection** | Weak (lacks fine-grained quotas) | Strong (process resource limits) | Medium (user processes limited, kernel drivers not) | **Strong (hard quota per service, supports flow control & backpressure)** |
| **API Compatibility Evolution** | Relies on kernel ABI & userland glibc | Userland library solutions | Userland compatibility layer & API sets | **Systematic support: Versioned service endpoints + userland compatibility libraries** |
| **Applicable Scenarios** | General servers, desktops, pursuit of ultimate throughput | Safety-critical embedded, military, avionics | Desktop, mobile, general servers (trade-off compatibility & performance) | **Full spectrum: from IoT embedded to data centers, especially suited for complex systems requiring continuous evolution** |

### 11.1 In-Depth Comparison with Hybrid Kernels

Hybrid kernels (e.g., Windows NT, macOS's XNU) attempt to combine the advantages of monolithic and microkernels: placing core functions (scheduling, virtual memory) and performance-critical drivers in kernel space, while running filesystems, network protocol stacks, etc., as user-space services.

**Significant Advantages of HIK over Hybrid Kernels:**

1. **Consistent and Stronger Isolation:**
   · In hybrid kernels, drivers and services running in kernel space still share the same address space; a memory error in one driver can corrupt the entire kernel. Its isolation is incomplete.
   · In HIK, **all drivers and services** (whether performance-critical or not) run in independent, MMU-strongly-isolated physical memory sandboxes, achieving uniform and complete fault isolation.
2. **Smaller Trusted Computing Base (TCB):**
   · The TCB of a hybrid kernel includes all code in the kernel space, which is still very large.
   · HIK's TCB is strictly limited to Core-0 and the core capability system, **far smaller** than hybrid kernels, making it easier for formal verification and security auditing.
3. **Better Performance Predictability:**
   · In hybrid kernels, interaction between user-space services and the kernel still requires expensive privilege-level switching and context switching.
   · HIK eliminates this overhead by having services run in sandboxes at the same privilege level as the kernel, making performance more predictable, especially beneficial for real-time applications.
4. **Finer-Grained Resource Control:**
   · Hybrid kernels lack fine-grained resource limits for kernel-space components.
   · HIK's capability system and quota mechanism can precisely control resource usage for **every service sandbox**, which is crucial for defending against internal attacks and guaranteeing Quality of Service (QoS).

**Potential Challenges of HIK (compared to Hybrid Kernels):**

1. **Implementation Complexity:** Achieving strong memory isolation at the same physical privilege level requires a carefully designed capability system and Core-0, which is more complex than traditional hybrid kernel architectures.
2. **Compatibility:** Hybrid kernels generally offer better compatibility with existing drivers designed for monolithic kernels (through adaptation). HIK requires rewriting or adapting drivers for the Privileged-1 service sandbox model, or providing specific compatibility layers.

## 12. Simplified Design for Architectures without MMU

The HIK architecture defaults to assuming the target processor has a full-fledged Memory Management Unit (MMU). However, for some extremely resource-constrained embedded microcontrollers (e.g., ARM Cortex-M series) or real-time control systems, an MMU may be absent or disabled. For such cases, HIK provides a simplified design variant that maintains core isolation principles in an MMU-less environment.

### 12.1 Flat Physical Memory Mapping Model

When the target platform lacks an MMU, HIK adopts a **flat physical memory mapping model**:

1. **Single Physical Address Space:** The entire system runs in a single physical address space, with no virtual address translation. Code and data for all domains (Core-0, Privileged-1 services, Applications) directly use physical addresses.
2. **Static Layout Allocation:** At build time, the hardware synthesis system allocates fixed, non-overlapping physical memory regions for each domain. The location and size of these regions are determined at compile time and hardcoded in the linker script.
3. **Memory Protection Alternative Mechanisms:**
   · **MPU (Memory Protection Unit):** If the processor provides an MPU (e.g., ARM Cortex-M), use the MPU to set up a limited number of protection regions (typically 8-16) for each domain. Core-0 dynamically reconfigures the MPU during domain switches, ensuring each domain can only access its authorized regions.
   · **Pure Software Protection:** Without an MPU, rely on dynamic verification by the capability system. Before each memory access, Core-0 checks via software whether the address is within the domain's authorized range. This mode has lower performance but still provides basic protection.

### 12.2 Redefinition of Privilege Levels

In MMU-less ARMv7-M or RISC-V machine mode, there is typically only one or two privilege levels:

1. **Single Privilege Level Mode:**
   · Core-0 and all Privileged-1 services run at the same privilege level (e.g., ARM's Handler mode).
   · Isolation relies entirely on static memory layout and runtime verification by the capability system.
   · Applications run at a lower privilege level (e.g., ARM's Thread mode), trapping into the higher privilege level via system calls.
2. **Core Responsibilities Unchanged:** Core-0 remains the system's arbiter, responsible for capability verification, scheduling, and exception handling, but its own code shares the same address space with privileged services.

### 12.3 Adjustments to Isolation Mechanisms

1. **Weakened Memory Isolation:** Without hardware-enforced isolation, a service fault (e.g., wild pointer) might corrupt memory of adjacent services or Core-0. Therefore:
   · **Guard Regions:** Insert "Guard Pages" between services, typically 4KB-aligned unmapped regions, to detect out-of-bounds access (via MPU or software exception).
   · **Copy-on-Write (CoW):** For shared data, use Copy-on-Write mechanisms to prevent accidental modification.
2. **Strengthened Capability System:** The capability system becomes the primary isolation mechanism in an MMU-less environment. All resource accesses (including memory reads/writes, device I/O) must pass capability verification, even if the address is within a legal range.
3. **Communication Mechanisms:** Due to the lack of zero-copy shared memory (all memory is physically contiguous), inter-service communication switches to message passing via Core-0, with data needing to be copied under Core-0's control.

### 12.4 Build and Deployment without MMU

1. **Purely Static Synthesis:** MMU-less systems only support static synthesis mode; all services are linked into a single image at build time.
2. **Link-Time Conflict Detection:** The build system must ensure physical memory regions of domains do not overlap and resolve all symbol references at the linking stage.
3. **Simplified Interrupt Handling:** Interrupts are directly dispatched by Core-0 to registered service handler functions, with no address space switching overhead.
4. **Enhanced Determinism:** Due to the absence of TLB and page table walks, the system's timing behavior is more deterministic, suitable for hard real-time scenarios.

### 12.5 Applicable Scenarios and Limitations

**Applicable Scenarios:**
· Deep embedded systems (IoT nodes, sensors, actuators)
· Real-time control systems (aerospace, industrial automation)
· Extremely resource-constrained microcontrollers (memory < 1MB)

**Limitations:**
· **Weakened Fault Isolation:** A service fault may cause cascading failures, requiring reliance on high-reliability coding and static verification.
· **Limited Dynamism:** Cannot support runtime module loading and hot upgrades.
· **Lower Memory Efficiency for Multitasking:** Due to fixed physical memory partitions, internal fragmentation may occur.
· **Reduced Security Level:** Cannot achieve high assurance levels like CC EAL6+/7.
