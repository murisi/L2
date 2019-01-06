.globl syscall
.globl setjump
.globl longjump
.globl _start
.text
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
	movq 16(%rsp), %rdi
	movq 24(%rsp), %rsi
	movq 32(%rsp), %rdx
	movq 40(%rsp), %rcx
	movq 48(%rsp), %r8
	movq 56(%rsp), %r9
	syscall
	ret

setjump:
	movq %rbp, 0(%rcx)
	movq 0(%rsp), %rdx
	movq %rdx, 8(%rcx)
	movq %r14, 16(%rcx)
	movq %r13, 24(%rcx)
	movq %rbx, 32(%rcx)
	movq %r12, 40(%rcx)
	movq %r15, 48(%rcx)
	movq %rdi, 56(%rcx)
	movq %rsi, 64(%rcx)
	movq %rsp, 72(%rcx)
	ret

longjump:
	movq 0(%rcx), %rbp
	movq 16(%rcx), %r14
	movq 24(%rcx), %r13
	movq 32(%rcx), %rbx
	movq 40(%rcx), %r12
	movq 48(%rcx), %r15
	movq 56(%rcx), %rdi
	movq 64(%rcx), %rsi
	movq 72(%rcx), %rsp
	jmp *8(%rcx)

_start:
	movq 0(%rsp), %rcx
	leaq 8(%rsp), %rdx
	andq $-16, %rsp
	subq $32, %rsp
	call main
	addq $32, %rsp
	movq %rax, %rdi
	mov $60, %rax
	syscall
