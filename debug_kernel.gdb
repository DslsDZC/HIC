target remote :1234
break *0x100013
continue
echo \n=== At breakpoint ===\n
x/5i $pc
info registers rip rax rdx rsp
stepi
echo \n=== After stepi ===\n
x/5i $pc
info registers rip rax rdx rsp
stepi
echo \n=== After stepi 2 ===\n
x/5i $pc
info registers rip rax rdx rsp
stepi
echo \n=== After stepi 3 ===\n
x/5i $pc
info registers rip rax rdx rsp
stepi
echo \n=== After stepi 4 ===\n
x/5i $pc
info registers rip rax rdx rsp
stepi
echo \n=== After stepi 5 ===\n
x/5i $pc
info registers rip rax rdx rsp