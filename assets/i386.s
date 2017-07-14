.text
.global get
.global getchar
.global set
.global setchar
.global add
.global subtract
.global multiply
.global divide
.global rem
.global equals
.global greaterthan
.global lesserthan
.global leftshift
.global rightshift
.global and
.global or
.global not
.global allocate

jmp l2rt_end

get:
movl 4(%esp), %eax
movl 0(%eax), %eax
ret

getchar:
movl 4(%esp), %ecx
xorl %eax, %eax
movb 0(%ecx), %al
ret

set:
movl 4(%esp), %ecx
movl 8(%esp), %edx
movl %edx, 0(%ecx)
ret

setchar:
movl 4(%esp), %ecx
movb 8(%esp), %dl
movb %dl, 0(%ecx)
ret

add:
movl 4(%esp), %eax
addl 8(%esp), %eax
ret

subtract:
movl 4(%esp), %eax
subl 8(%esp), %eax
ret

multiply:
movl 4(%esp), %eax
mull 8(%esp)
ret

divide:
xor %edx, %edx
movl 4(%esp), %eax
divl 8(%esp)
ret

rem:
xor %edx, %edx
movl 4(%esp), %eax
divl 8(%esp)
movl %edx, %eax
ret

equals:
xor %eax, %eax
movl 4(%esp), %ecx
subl 8(%esp), %ecx
setz %al
negl %eax
ret

greaterthan:
xor %eax, %eax
movl 8(%esp), %ecx
subl 4(%esp), %ecx
setc %al
negl %eax
ret

lesserthan:
xor %eax, %eax
movl 4(%esp), %ecx
subl 8(%esp), %ecx
setc %al
negl %eax
ret

leftshift:
movl 4(%esp), %eax
movl 8(%esp), %ecx
shl %cl, %eax
ret

rightshift:
movl 4(%esp), %eax
movl 8(%esp), %ecx
shr %cl, %eax
ret

and:
movl 4(%esp), %eax
andl 8(%esp), %eax
ret

or:
movl 4(%esp), %eax
orl 8(%esp), %eax
ret

not:
movl 4(%esp), %eax
notl %eax
ret

allocate:
/* All sanctioned by L2 ABI: */
movl 8(%esp), %ecx
movl 16(%ecx), %ebx
movl 12(%ecx), %esi
movl 8(%ecx), %edi
movl 0(%ecx), %ebp
subl 4(%esp), %esp
andl $0xFFFFFFFC, %esp
movl %esp, 20(%ecx)
jmp *4(%ecx)

l2rt_end:
