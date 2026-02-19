# HIC内核GDB自动调试脚本
set pagination off
set architecture i386:x86-64

# 连接到QEMU
target remote :1234

# 等待一小会儿让QEMU稳定
shell sleep 2

# 中断目标
interrupt

# 清除断点
delete breakpoints

# 在内核入口点设置断点
hbreak *0x100000
break kernel_start
break kernel_entry

# 继续执行到内核入口点
continue

# 等待到达断点
shell sleep 1

# 中断并显示状态
interrupt

# 显示寄存器
echo [=== 寄存器状态 ===]
info registers rip rax rbx rcx rdx rsi rdi rbp rsp r8-r15

# 显示当前指令
echo [=== 当前指令 ===]
x/10i $rip

# 显示栈内容
echo [=== 栈内容 ===]
x/20gx $rsp

# 继续执行到kernel_start
echo [=== 继续到kernel_start ===]
continue

# 等待到达kernel_start
shell sleep 1

# 中断并显示kernel_start状态
interrupt

echo [=== kernel_start 寄存器状态 ===]
info registers rip rax rbx rcx rdx rsi rdi rbp rsp r8-r15

echo [=== kernel_start 当前指令 ===]
x/10i $rip

echo [=== kernel_start 栈内容 ===]
x/20gx $rsp

# 退出
quit