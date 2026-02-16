set architecture i386:x86-64
target remote :1234

# 内核入口点断点
break *0x100000
commands
  printf "\n\n========== 内核入口点到达 ==========\n"
  printf "RIP=0x%016x  RSP=0x%016x\n", $rip, $rsp
  printf "RAX=0x%016x  RBX=0x%016x\n", $rax, $rbx
  printf "RCX=0x%016x  RDX=0x%016x\n", $rcx, $rdx
  printf "RSI=0x%016x  RDI=0x%016x\n", $rsi, $rdi
  printf "\n反汇编代码:\n"
  x/10i $rip
  printf "\n继续执行...\n"
  continue
end

# kernel_start函数断点
break kernel_start
commands
  printf "\n\n========== kernel_start函数 ==========\n"
  printf "RDI(boot_info)=0x%016x\n", $rdi
  printf "\n检查boot_info结构（前64字节）:\n"
  x/16gx $rdi
  printf "\n继续执行...\n"
  continue
end

# kernel_entry函数断点
break kernel_entry
commands
  printf "\n\n========== kernel_entry函数 ==========\n"
  printf "进入完整内核初始化流程\n"
  printf "\n继续执行...\n"
  continue
end

# privileged_service_init断点
break privileged_service_init
commands
  printf "\n\n========== 特权服务管理器初始化 ==========\n"
  printf "Privileged-1层初始化\n"
  printf "\n继续执行...\n"
  continue
end

# 异常捕获 - 捕获所有异常
catch signal
commands
  printf "\n\n========== 发生信号/异常 ==========\n"
  printf "RIP=0x%016x  RSP=0x%016x\n", $rip, $rsp
  printf "RAX=0x%016x  RBX=0x%016x\n", $rax, $rbx
  printf "\n异常处代码:\n"
  x/10i $rip
  printf "\n停止调试\n"
  quit
end

continue