/**
 * HIK BIOS Bootloader汇编入口
 * 
 * BIOS启动流程：
 * 1. BIOS POST
 * 2. 加载Bootloader到0x7C00
 * 3. 跳转到boot_start
 * 4. 切换到32位保护模式
 * 5. 调用C代码bios_main()
 */

.section .text
.global _start
.global boot_start
.global protected_mode_entry
.global long_mode_entry

/* BIOS启动签名 */
.set BOOT_SIGNATURE, 0xAA55

/* 启动磁盘标识 */
.set BOOT_DRIVE, 0x80

/* 内存地址 */
.set BOOTLOADER_BASE,    0x7C00
.set KERNEL_BASE,         0x100000
#define GDT_BASE            0x800

/* GDT描述符 */
.set GDT_NULL,            0
.set GDT_CODE_64,         1
.set GDT_DATA_64,         2
.set GDT_CODE_32,         3
.set GDT_DATA_32,         4
.set GDT_CODE_LONG,       5
.set GDT_DATA_LONG,       6

.code16
_start:
boot_start:
    /* 禁用中断 */
    cli
    
    /* 保存寄存器 */
    pusha
    push ds
    push es
    
    /* 设置数据段 */
    xor ax, ax
    mov ds, ax
    mov es, ax
    
    /* 设置栈 */
    mov sp, BOOTLOADER_BASE
    
    /* 保存引导驱动器号 */
    mov byte [boot_drive], dl
    
    /* 打印启动消息 */
    call print_hello
    
    /* 加载更多扇区 */
    mov ax, 0x8000      /* 目标段地址 */
    mov es, ax
    xor bx, bx          /* 目标偏移 */
    mov ah, 0x02        /* 读扇区 */
    mov al, 0x04        /* 读取4个扇区 */
    mov ch, 0x00        /* 柱号 */
    mov cl, 0x02        /* 扇区号 (从第2扇区开始) */
    mov dh, 0x00        /* 磁头号 */
    mov dl, [boot_drive] /* 驱动器号 */
    int 0x13
    
    jc disk_error
    cmp al, 0x04        /* 检查是否读取了4个扇区 */
    jne disk_error
    
    /* 切换到保护模式 */
    call switch_to_protected_mode
    
    /* 永远不会到达这里 */
    cli
    hlt

/* 打印Hello消息 */
print_hello:
    mov si, hello_msg
    call print_string
    ret

/* 打印字符串 (SI=字符串地址) */
print_string:
    lodsb
    cmp al, 0
    je print_string_done
    mov ah, 0x0E        /* BIOS teletype输出 */
    int 0x10
    jmp print_string
print_string_done:
    ret

/* 磁盘错误处理 */
disk_error:
    mov si, error_msg
    call print_string
    jmp $

/* 切换到保护模式 */
switch_to_protected_mode:
    /* 检查A20线 */
    call check_a20
    test ax, ax
    jz a20_ok
    
    /* 启用A20线 */
    call enable_a20
a20_ok:
    
    /* 设置GDT */
    lgdt [gdt_descriptor]
    
    /* 设置PE位 (保护模式启用) */
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    /* 跳转到32位代码 */
    jmp dword GDT_CODE_32:protected_mode_entry_32

/* 检查A20线 */
check_a20:
    pushf                /* 先保存标志寄存器 */
    push ds              /* 保存数据段 */
    push es              /* 保存附加段 */
    push si              /* 保存源索引 */
    push di              /* 保存目标索引 */
    
    cli                  /* 禁用中断 */
    
    xor ax, ax
    mov es, ax           /* ES = 0 */
    mov di, 0x1120       /* DI = 0x1120 */
    mov ax, di
    mov ds, ax           /* DS = 0x1120 */
    mov si, 0x0120       /* SI = 0x0120 */
    
    mov al, [ds:si]      /* 读取原始值 */
    push ax              /* 保存原始值到栈 */
    
    not al               /* 取反 */
    mov [ds:si], al      /* 写入取反后的值 */
    
    pushf                /* 保存当前标志 */
    pop di               /* 恢复到DI */
    
    mov al, [es:di]      /* 从ES:DI读取值 */
    
    pop ax               /* 恢复原始值 */
    mov [ds:si], al      /* 恢复原始值 */
    
    cli                  /* 禁用中断 */
    
    pop di               /* 恢复目标索引 */
    pop si               /* 恢复源索引 */
    pop es               /* 恢复附加段 */
    pop ds               /* 恢复数据段 */
    popf                 /* 恢复标志寄存器 */
    
    ret

/* 启用A20线 */
enable_a20:
    push ax              /* 保存AX寄存器 */
    in al, 0x92          /* 从端口0x92读取 */
    or al, 2             /* 设置A20位 */
    out 0x92, al         /* 写回端口0x92 */
    pop ax               /* 恢复AX寄存器 */
    ret

/* 数据段 */
.section .data
.align 4

hello_msg:
    .asciz "HIK BIOS Bootloader v1.0\r\n"

error_msg:
    .asciz "Disk error!\r\n"

boot_drive:
    .byte 0

/* GDT描述符 */
gdt_descriptor:
    .word gdt_end - gdt - 1
    .long GDT_BASE

/* GDT表 */
gdt:
    /* NULL描述符 */
    .quad 0
    
    /* 64位代码段 */
    .quad 0x00af9a000000ffff
    
    /* 64位数据段 */
    .quad 0x00cf92000000ffff
    
    /* 32位代码段 */
    .quad 0x00cf9a000000ffff
    
    /* 32位数据段 */
    .quad 0x00cf92000000ffff
    
    /* 64位长模式代码段 */
    .quad 0x00af9a000000ffff
    
    /* 64位长模式数据段 */
    .quad 0x00cf92000000ffff
    
gdt_end:

/* 启动签名 */
.word BOOT_SIGNATURE
