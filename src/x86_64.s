.global getb
.global get1b
.global get2b
.global get4b
.global get8b
.global get
.global setb
.global set1b
.global set2b
.global set4b
.global set8b
.global set
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
.global land
.global lor
.global lnot
.global allocate
.global syscall

.text
jmp l2rt_end

get:
get8b:
movq 0(%rcx), %rax
ret

getb:
get1b:
xorq %rax, %rax
movb 0(%rcx), %al
ret

get2b:
xorq %rax, %rax
movw 0(%rcx), %ax
ret

get4b:
xorq %rax, %rax
movl 0(%rcx), %eax
ret

set:
set8b:
movq %rdx, 0(%rcx)
ret

setb:
set1b:
movq %rdx, %rax
movb %al, 0(%rcx)
ret

set2b:
movq %rdx, %rax
movw %ax, 0(%rcx)
ret

set4b:
movq %rdx, %rax
movl %eax, 0(%rcx)
ret

add:
movq %rcx, %rax
addq %rdx, %rax
ret

subtract:
movq %rcx, %rax
subq %rdx, %rax
ret

multiply:
movq %rcx, %rax
mulq %rdx
ret

divide:
movq %rdx, %r8
xorq %rdx, %rdx
movq %rcx, %rax
divq %r8
ret

rem:
movq %rdx, %r8
xorq %rdx, %rdx
movq %rcx, %rax
divq %r8
movq %rdx, %rax
ret

equals:
xorq %rax, %rax
subq %rdx, %rcx
setz %al
ret

greaterthan:
xorq %rax, %rax
subq %rcx, %rdx
setc %al
ret

lesserthan:
xorq %rax, %rax
subq %rdx, %rcx
setc %al
ret

leftshift:
movq %rcx, %rax
movq %rdx, %rcx
shl %cl, %rax
ret

rightshift:
movq %rcx, %rax
movq %rdx, %rcx
shr %cl, %rax
ret

land:
movq %rcx, %rax
andq %rdx, %rax
ret

lor:
movq %rcx, %rax
orq %rdx, %rax
ret

lnot:
movq %rcx, %rax
notq %rax
ret

allocate:
/* All sanctioned by L2 ABI: */
movq 48(%rsi), %r15
movq 40(%rsi), %r12
movq 32(%rsi), %rbx
movq 24(%rsi), %r13
movq 16(%rsi), %r14
movq 0(%rsi), %rbp
subq %rdi, %rsp
andq $0xFFFFFFFFFFFFFFF8, %rsp
movq %rsp, 56(%rsi)
jmp *8(%rsi)

/*
 * Do syscall with the syscall number being provided by the first argument to
 * this function and its six arguments being provided by arguments 2 to 7 of
 * this function.
 */
syscall:
movq %rcx, 8(%rsp)
movq %rdx, 16(%rsp)
movq %r8, 24(%rsp)
movq %r9, 32(%rsp)

movq 8(%rsp), %rax
movq 32(%rsp), %rdx
movq 40(%rsp), %r10
movq 48(%rsp), %r8
movq 56(%rsp), %r9

movq %rdi, 8(%rsp)
movq %rsi, 32(%rsp)
movq 16(%rsp), %rdi
movq 24(%rsp), %rsi
syscall
movq 8(%rsp), %rdi
movq 32(%rsp), %rsi
ret

l2rt_end:
ret
