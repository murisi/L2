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
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  movq %r8, %r10
  movq %r9, %r8
  movq 8(%rsp), %r9
  syscall
  ret

setjump:
  movq %rbp, 0(%rdi)
  movq 0(%rsp), %rsi
  movq %rsi, 8(%rdi)
  movq %r14, 16(%rdi)
  movq %r13, 24(%rdi)
  movq %rbx, 32(%rdi)
  movq %r12, 40(%rdi)
  movq %r15, 48(%rdi)
  movq %rsp, 56(%rdi)
  ret

longjump:
  movq 0(%rdi), %rbp
  movq 16(%rdi), %r14
  movq 24(%rdi), %r13
  movq 32(%rdi), %rbx
  movq 40(%rdi), %r12
  movq 48(%rdi), %r15
  movq 56(%rdi), %rsp
  jmp *8(%rdi)

_start:
  movq 0(%rsp), %rdi
  leaq 8(%rsp), %rsi
  andq $-16,%rsp
  call main
  movq %rax, %rdi
  mov $60, %rax
  syscall
