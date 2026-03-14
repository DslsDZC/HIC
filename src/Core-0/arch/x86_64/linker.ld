/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * HIC内核链接脚本（x86_64）
 * 内存布局：0x100000 开始
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

/* 内核基地址 */
KERNEL_BASE = 0x100000;

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

    /* ==================== 静态服务代码段（全自动） ==================== */
    /* 每个服务使用独立的段名，在链接脚本中显式定义起始/结束符号 */
    /* 必须放在内核主段内，确保被 bootloader 加载 */
    
    /* 
     * 静态服务段命名规则：
     * - 代码段: .static_svc.<name>.text
     * - 只读数据: .static_svc.<name>.rodata  
     * - 数据段: .static_svc.<name>.data
     * - BSS段: .static_svc.<name>.bss
     */
    
    .static_services ALIGN(4K) : AT(ADDR(.static_services)) {
        __static_services_start = .;
        
        /* verifier 服务段 */
        __start_static_svc_verifier_text = .;
        KEEP(*(.static_svc.verifier.text))
        KEEP(*(.static_svc.verifier.rodata))
        __stop_static_svc_verifier_text = .;
        
        /* fat32_service 服务段 */
        __start_static_svc_fat32_service_text = .;
        KEEP(*(.static_svc.fat32_service.text))
        KEEP(*(.static_svc.fat32_service.rodata))
        __stop_static_svc_fat32_service_text = .;
        
        /* init_launcher 服务段 */
        __start_static_svc_init_launcher_text = .;
        KEEP(*(.static_svc.init_launcher.text))
        KEEP(*(.static_svc.init_launcher.rodata))
        __stop_static_svc_init_launcher_text = .;
        
        /* 通配符匹配其他静态服务段 */
        *(.static_svc.*)
        
        /* 兼容旧格式 */
        KEEP(*(.static_service.code))
        KEEP(*(.static_service.text))
        KEEP(*(.static_service.rodata))
        *(.static_service.*)
        __static_services_end = .;
    }

    /* 静态服务数据段 */
    .static_services_data ALIGN(4K) : AT(ADDR(.static_services_data)) {
        __static_services_data_start = .;
        
        /* verifier 数据段 */
        __start_static_svc_verifier_data = .;
        KEEP(*(.static_svc.verifier.data))
        __stop_static_svc_verifier_data = .;
        
        /* fat32_service 数据段 */
        __start_static_svc_fat32_service_data = .;
        KEEP(*(.static_svc.fat32_service.data))
        __stop_static_svc_fat32_service_data = .;
        
        /* init_launcher 数据段 */
        __start_static_svc_init_launcher_data = .;
        KEEP(*(.static_svc.init_launcher.data))
        __stop_static_svc_init_launcher_data = .;
        
        /* 通配符 */
        *(.static_svc.*.data)
        KEEP(*(.static_service.data))
        *(.static_service.data.*)
        __static_services_data_end = .;
    }

    /* 静态服务 BSS 段 */
    .static_services_bss ALIGN(4K) : AT(ADDR(.static_services_bss)) {
        __static_services_bss_start = .;
        
        /* verifier BSS 段 */
        __start_static_svc_verifier_bss = .;
        KEEP(*(.static_svc.verifier.bss))
        __stop_static_svc_verifier_bss = .;
        
        /* fat32_service BSS 段 */
        __start_static_svc_fat32_service_bss = .;
        KEEP(*(.static_svc.fat32_service.bss))
        __stop_static_svc_fat32_service_bss = .;
        
        /* init_launcher BSS 段 */
        __start_static_svc_init_launcher_bss = .;
        KEEP(*(.static_svc.init_launcher.bss))
        __stop_static_svc_init_launcher_bss = .;
        
        /* 通配符 */
        *(.static_svc.*.bss)
        KEEP(*(.static_service.bss))
        *(.static_service.bss.*)
        __static_services_bss_end = .;
    }

    /* 静态服务模块描述符段 */
    .static_modules ALIGN(4K) : AT(ADDR(.static_modules)) {
        __static_modules_start = .;
        KEEP(*(.static_modules))
        __static_modules_end = .;
    }

    /* 栈段（预留16KB） */
    .stack ALIGN(4K) : AT(ADDR(.stack)) {
        __stack_start = .;
        . += 16K;
        __stack_end = .;
    }

    /* 大型 BSS 段（放在内核主段之后） */
    .lbss ALIGN(4K) : AT(ADDR(.lbss)) {
        *(.lbss)
        *(.lbss.*)
    }

    /* 内核结束 */
    _kernel_end = .;

    /* 内核结束（在添加AP启动代码之前） */
    _kernel_end_before_ap = .;
    
    /* AP启动代码段（链接到0x8000，但加载到内核中） */
    .ap_trampoline 0x8000 : AT(ALIGN(_kernel_end_before_ap, 4K)) {
        __ap_trampoline_start = .;
        KEEP(*(.ap_trampoline))
        __ap_trampoline_end = .;
    }
    
    /* 最终内核结束地址 */
    _kernel_end = ALIGN(_kernel_end_before_ap, 4K) + SIZEOF(.ap_trampoline);

    /* 丢弃调试信息 */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
