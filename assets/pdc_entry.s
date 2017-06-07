.comm	argc,4,4
.comm	argv,4,4
.globl	main
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
movl $0, %esi
