.comm argc,4,4
.comm argv,4,4
.globl main
main:
pushl %ebp
movl %esp, %ebp
movl 8(%ebp), %eax
movl %eax, argc
movl 12(%ebp), %eax
movl %eax, argv
pushl %esi
pushl %edi
pushl %ebx
jmp thunk_end
.globl	get_pc_thunk
.hidden	get_pc_thunk
get_pc_thunk:
movl (%esp), %ebx
ret
thunk_end:
call get_pc_thunk
addl $_GLOBAL_OFFSET_TABLE_, %ebx
