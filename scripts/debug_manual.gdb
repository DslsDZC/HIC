# HIC内核GDB手动调试脚本
# 使用方法: gdb -x scripts/debug_manual.gdb build/bin/hic-kernel.elf

set architecture i386:x86-64
target remote :1234

# 在内核入口点设置断点
hbreak *0x100000
break kernel_start
break kernel_entry

# 继续执行
continue

# 在断点处可使用以下命令：
# info registers  - 查看寄存器
# x/10i $rip     - 查看当前指令
# x/20gx $rsp    - 查看栈内容
# stepi          - 单步执行
# continue       - 继续执行