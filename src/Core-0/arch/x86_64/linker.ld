/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核链接脚本（x86_64）
 * 
 * 内存布局：
 * 0x100000     - 内核代码段开始
 * ...          - 内核各段
 * _kernel_end  - 内核结束
 * 
 * 静态模块段（在内核外，固定地址，可卸载）：
 * STATIC_MODULES_BASE - 静态模块段开始
 * ...                  - 静态模块代码/数据/BSS
 * _static_modules_end  - 静态模块段结束
 * 
 * 卸载静态模块后，内存可纳入动态池
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/* 内核基地址 */
KERNEL_BASE = 0x100000;

/* 静态模块段基地址（在内核之后，预留 16MB 给内核） */
STATIC_MODULES_BASE = 0x1100000;  /* 17MB 开始，预留 16MB 给内核 */

/* 内核虚拟地址（如果使用高半内核映射） */
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    /* 内核代码段（从物理地址 0x100000 开始） */
    . = KERNEL_BASE;

    .text ALIGN(4K) : AT(ADDR(.text)) {
        *(.text.start)  /* 确保 _start 在最前面 */
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        *(.rodata)
        *(.rodata.*)
    }

    /* 初始化代码段（启动后可丢弃） */
    .init ALIGN(4K) : AT(ADDR(.init)) {
        __init_start = .;
        *(.init)
        *(.init.*)
        __init_end = .;
    }

    /* 数据段 */
    .data ALIGN(4K) : AT(ADDR(.data)) {
        *(.data)
        *(.data.*)
    }

    /* BSS段（零初始化数据） */
    .bss ALIGN(4K) : AT(ADDR(.bss)) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4K);
        __bss_end = .;
    }

    /* 栈段（预留16KB） */
    .stack ALIGN(4K) : AT(ADDR(.stack)) {
        __stack_start = .;
        . += 16K;
        __stack_end = .;
    }

    /* 大型 BSS 段（限制在内核段内） */
    .lbss ALIGN(4K) : AT(ADDR(.lbss)) {
        *(.lbss)
        *(.lbss.*)
    }

    /* ==================== 内核结束 ==================== */
    _kernel_end = .;
    _kernel_size = _kernel_end - KERNEL_BASE;

    /* 断言：内核不能超过静态模块基地址 */
    ASSERT(_kernel_end <= STATIC_MODULES_BASE, 
           "Kernel size exceeds limit! Static modules region would be overwritten.")

    /* AP启动代码段（链接到0x8000，但加载到内核中） */
    .ap_trampoline 0x8000 : AT(ALIGN(_kernel_end, 4K)) {
        __ap_trampoline_start = .;
        KEEP(*(.ap_trampoline))
        __ap_trampoline_end = .;
    }
    
    _kernel_final_end = ALIGN(_kernel_end, 4K) + SIZEOF(.ap_trampoline);

    /* ==================== 静态模块段（内核外，可卸载） ==================== */
    /* 
     * 静态模块特性：
     * - 固定地址，在内核内存之外
     * - 可卸载，内存可重用
     * - init_launcher 用完后可释放，纳入动态池
     */
    . = STATIC_MODULES_BASE;
    _static_modules_region_start = .;

    /* 静态模块描述符段 */
    .static_modules ALIGN(4K) : AT(ADDR(.static_modules)) {
        __static_modules_start = .;
        KEEP(*(.static_modules))
        __static_modules_end = .;
    }

    /* 静态模块代码段 */
    .static_services_text ALIGN(4K) : AT(ADDR(.static_services_text)) {
        _static_modules_text_start = .;
        
        /* @STATIC_MODULES_TEXT_START@ - 自动生成 */
        /* verifier 服务段 */
        __start_static_svc_verifier_text = .;
        KEEP(*(.static_svc.verifier.text))
        KEEP(*(.static_svc.verifier.rodata))
        __stop_static_svc_verifier_text = .;

        /* ide_driver 服务段 */
        __start_static_svc_ide_driver_text = .;
        KEEP(*(.static_svc.ide_driver.text))
        KEEP(*(.static_svc.ide_driver.rodata))
        __stop_static_svc_ide_driver_text = .;

        /* fat32_service 服务段 */
        __start_static_svc_fat32_service_text = .;
        KEEP(*(.static_svc.fat32_service.text))
        KEEP(*(.static_svc.fat32_service.rodata))
        __stop_static_svc_fat32_service_text = .;

        /* memory_service 服务段 */
        __start_static_svc_memory_service_text = .;
        KEEP(*(.static_svc.memory_service.text))
        KEEP(*(.static_svc.memory_service.rodata))
        __stop_static_svc_memory_service_text = .;

        /* device_manager 服务段 */
        __start_static_svc_device_manager_text = .;
        KEEP(*(.static_svc.device_manager.text))
        KEEP(*(.static_svc.device_manager.rodata))
        __stop_static_svc_device_manager_text = .;

        /* security_monitor 服务段 */
        __start_static_svc_security_monitor_text = .;
        KEEP(*(.static_svc.security_monitor.text))
        KEEP(*(.static_svc.security_monitor.rodata))
        __stop_static_svc_security_monitor_text = .;

        /* init_launcher 服务段（最后加载，可卸载） */
        __start_static_svc_init_launcher_text = .;
        KEEP(*(.static_svc.init_launcher.text))
        KEEP(*(.static_svc.init_launcher.rodata))
        __stop_static_svc_init_launcher_text = .;

        /* @STATIC_MODULES_TEXT_END@ */
        
        /* 通配符匹配其他静态服务段 */
        *(.static_svc.*)
        *(.static_service.*)
        _static_modules_text_end = .;
    }

    /* 静态模块数据段 */
    .static_services_data ALIGN(4K) : AT(ADDR(.static_services_data)) {
        _static_modules_data_start = .;
        
        /* @STATIC_MODULES_DATA_START@ - 自动生成 */
        __start_static_svc_verifier_data = .;
        KEEP(*(.static_svc.verifier.data))
        __stop_static_svc_verifier_data = .;

        __start_static_svc_ide_driver_data = .;
        KEEP(*(.static_svc.ide_driver.data))
        __stop_static_svc_ide_driver_data = .;

        __start_static_svc_fat32_service_data = .;
        KEEP(*(.static_svc.fat32_service.data))
        __stop_static_svc_fat32_service_data = .;

        __start_static_svc_memory_service_data = .;
        KEEP(*(.static_svc.memory_service.data))
        __stop_static_svc_memory_service_data = .;

        __start_static_svc_device_manager_data = .;
        KEEP(*(.static_svc.device_manager.data))
        __stop_static_svc_device_manager_data = .;

        __start_static_svc_security_monitor_data = .;
        KEEP(*(.static_svc.security_monitor.data))
        __stop_static_svc_security_monitor_data = .;

        __start_static_svc_init_launcher_data = .;
        KEEP(*(.static_svc.init_launcher.data))
        __stop_static_svc_init_launcher_data = .;

        /* @STATIC_MODULES_DATA_END@ */
        
        *(.static_svc.*.data)
        _static_modules_data_end = .;
    }

    /* 静态模块 BSS 段 */
    .static_services_bss ALIGN(4K) : AT(ADDR(.static_services_bss)) {
        _static_modules_bss_start = .;
        
        /* @STATIC_MODULES_BSS_START@ - 自动生成 */
        __start_static_svc_verifier_bss = .;
        KEEP(*(.static_svc.verifier.bss))
        __stop_static_svc_verifier_bss = .;

        __start_static_svc_ide_driver_bss = .;
        KEEP(*(.static_svc.ide_driver.bss))
        __stop_static_svc_ide_driver_bss = .;

        __start_static_svc_fat32_service_bss = .;
        KEEP(*(.static_svc.fat32_service.bss))
        __stop_static_svc_fat32_service_bss = .;

        __start_static_svc_memory_service_bss = .;
        KEEP(*(.static_svc.memory_service.bss))
        __stop_static_svc_memory_service_bss = .;

        __start_static_svc_device_manager_bss = .;
        KEEP(*(.static_svc.device_manager.bss))
        __stop_static_svc_device_manager_bss = .;

        __start_static_svc_security_monitor_bss = .;
        KEEP(*(.static_svc.security_monitor.bss))
        __stop_static_svc_security_monitor_bss = .;

        __start_static_svc_init_launcher_bss = .;
        KEEP(*(.static_svc.init_launcher.bss))
        __stop_static_svc_init_launcher_bss = .;

        /* @STATIC_MODULES_BSS_END@ */
        
        *(.static_svc.*.bss)
        _static_modules_bss_end = .;
    }

    /* 静态模块段结束 */
    _static_modules_region_end = .;
    _static_modules_size = _static_modules_region_end - _static_modules_region_start;

    /* 丢弃调试信息 */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
