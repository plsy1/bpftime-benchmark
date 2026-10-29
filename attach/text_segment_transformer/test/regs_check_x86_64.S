// int64_t bpftime_test_x86_syscall_with_patterns(int64_t nr, uint64_t *out)
//
// Sets rdi, rsi, rdx, r10, r8, r9, rbx and r12-r15 to 0x5a5a00000000000N
// (N = the slot below), the low and high halves of xmm0-xmm15 to
// 0x111100000000000N and 0x222200000000000N, and the 128-byte red zone below
// rsp (except its top 16 bytes, which the rewritten `call *%rax` and the page
// zero trampoline write) to 0x3333000000000000 + offset, then issues
// `syscall` with rax = nr. The kernel changes only rax, rcx and r11, and
// leaf code may keep data in the red zone across an inline system call, so
// after the rewriting every value must still be the pattern. Stores:
//   out[0] rax, out[1..11] rdi rsi rdx r10 r8 r9 rbx r12 r13 r14 r15,
//   out[12..43] xmm0-xmm15 (low half first), out[44..57] red zone qwords at
//   rsp-24 .. rsp-128.
// Returns rax (the system call's result).
	.text
	.globl	bpftime_test_x86_syscall_with_patterns
	.type	bpftime_test_x86_syscall_with_patterns, @function
	.p2align 4
bpftime_test_x86_syscall_with_patterns:
	pushq	%rbp
	movq	%rsp, %rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rdi			// -48(%rbp): nr
	pushq	%rsi			// -56(%rbp): out
	// rsp is now the stack top whose red zone is checked.

	leaq	xmm_patterns(%rip), %rax
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	movdqu	(16 * \n)(%rax), %xmm\n
	.endr

	.irp	i, 3,4,5,6,7,8,9,10,11,12,13,14,15,16
	movabsq	$(0x3333000000000000 + \i), %rax
	movq	%rax, -(8 * \i)(%rsp)
	.endr

	movabsq	$0x5a5a000000000001, %rdi
	movabsq	$0x5a5a000000000002, %rsi
	movabsq	$0x5a5a000000000003, %rdx
	movabsq	$0x5a5a000000000004, %r10
	movabsq	$0x5a5a000000000005, %r8
	movabsq	$0x5a5a000000000006, %r9
	movabsq	$0x5a5a000000000007, %rbx
	movabsq	$0x5a5a000000000008, %r12
	movabsq	$0x5a5a000000000009, %r13
	movabsq	$0x5a5a00000000000a, %r14
	movabsq	$0x5a5a00000000000b, %r15
	movq	-48(%rbp), %rax
	syscall

	movq	-56(%rbp), %rcx		// rcx and r11 are free after syscall
	movq	%rax, 0(%rcx)
	movq	%rdi, 8(%rcx)
	movq	%rsi, 16(%rcx)
	movq	%rdx, 24(%rcx)
	movq	%r10, 32(%rcx)
	movq	%r8, 40(%rcx)
	movq	%r9, 48(%rcx)
	movq	%rbx, 56(%rcx)
	movq	%r12, 64(%rcx)
	movq	%r13, 72(%rcx)
	movq	%r14, 80(%rcx)
	movq	%r15, 88(%rcx)
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	movdqu	%xmm\n, (96 + 16 * \n)(%rcx)
	.endr
	.irp	i, 3,4,5,6,7,8,9,10,11,12,13,14,15,16
	movq	-(8 * \i)(%rsp), %r11
	movq	%r11, (352 + 8 * (\i - 3))(%rcx)
	.endr

	addq	$16, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	ret
	.size	bpftime_test_x86_syscall_with_patterns, .-bpftime_test_x86_syscall_with_patterns

	.section .rodata
	.p2align 4
xmm_patterns:
	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	.quad	0x1111000000000000 + \n
	.quad	0x2222000000000000 + \n
	.endr

	.section .note.GNU-stack,"",@progbits
