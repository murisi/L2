.text
.global words
.global bytes
.global get
.global getb
.global set
.global setb
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

words:
movq $8, %rax
ret

bytes:
movq $1, %rax
ret

get:
movq 0(%rdi), %rax
ret

getb:
xorq %rax, %rax
movb 0(%rdi), %al
ret

set:
movq %rsi, 0(%rdi)
ret

setb:
movq %rsi, %rax
movb %al, 0(%rdi)
ret

add:
movq %rdi, %rax
addq %rsi, %rax
ret

subtract:
movq %rdi, %rax
subq %rsi, %rax
ret

multiply:
movq %rdi, %rax
mulq %rsi
ret

divide:
xorq %rdx, %rdx
movq %rdi, %rax
divq %rsi
ret

rem:
xorq %rdx, %rdx
movq %rdi, %rax
divq %rsi
movq %rdx, %rax
ret

equals:
xorq %rax, %rax
subq %rsi, %rdi
setz %al
negq %rax
ret

greaterthan:
xorq %rax, %rax
subq %rdi, %rsi
setc %al
negq %rax
ret

lesserthan:
xorq %rax, %rax
subq %rsi, %rdi
setc %al
negq %rax
ret

leftshift:
movq %rdi, %rax
movq %rsi, %rcx
shl %cl, %rax
ret

rightshift:
movq %rdi, %rax
movq %rsi, %rcx
shr %cl, %rax
ret

and:
movq %rdi, %rax
andq %rsi, %rax
ret

or:
movq %rdi, %rax
orq %rsi, %rax
ret

not:
movq %rdi, %rax
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

l2rt_end:
