// int64_t bpftime_test_svc_with_patterns(int64_t nr, uint64_t *out)
//
// Sets x1-x7, x9-x30 to 0x5a5a00000000000N (N = register number), the low and
// high halves of v0-v31 to 0x111100000000000N and 0x222200000000000N, and
// NZCV to N and C, then issues `svc #0` with x8 = nr and x0 = 0. Right after
// the svc it stores x0-x30, NZCV and q0-q31 to out[0..30], out[31] and
// out[32..95] (q_n at out[32 + 2n], low half first). The kernel preserves
// every register except x0 across a system call, so after the rewriting each
// value must still be the pattern. Returns x0 (the system call's result).
	.text
	.globl	bpftime_test_svc_with_patterns
	.type	bpftime_test_svc_with_patterns, %function
	.p2align 2
bpftime_test_svc_with_patterns:
	hint	#34			// bti c
	stp	x29, x30, [sp, #-176]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	x25, x26, [sp, #64]
	stp	x27, x28, [sp, #80]
	stp	d8, d9, [sp, #96]
	stp	d10, d11, [sp, #112]
	stp	d12, d13, [sp, #128]
	stp	d14, d15, [sp, #144]
	stp	x0, x1, [sp, #160]	// nr, out

	.irp	n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	movz	x9, #\n
	movk	x9, #0x1111, lsl #48
	movz	x10, #\n
	movk	x10, #0x2222, lsl #48
	fmov	d\n, x9
	mov	v\n\().d[1], x10
	.endr

	ldr	x8, [sp, #160]
	movz	x0, #0xa000, lsl #16	// N and C
	msr	nzcv, x0
	mov	x0, #0
	.irp	n, 1,2,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	movz	x\n, #\n
	movk	x\n, #0x5a5a, lsl #48
	.endr
	svc	#0

	sub	sp, sp, #768
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	mrs	x0, nzcv
	stp	x30, x0, [sp, #240]
	add	x0, sp, #256
	stp	q0, q1, [x0, #0]
	stp	q2, q3, [x0, #32]
	stp	q4, q5, [x0, #64]
	stp	q6, q7, [x0, #96]
	stp	q8, q9, [x0, #128]
	stp	q10, q11, [x0, #160]
	stp	q12, q13, [x0, #192]
	stp	q14, q15, [x0, #224]
	stp	q16, q17, [x0, #256]
	stp	q18, q19, [x0, #288]
	stp	q20, q21, [x0, #320]
	stp	q22, q23, [x0, #352]
	stp	q24, q25, [x0, #384]
	stp	q26, q27, [x0, #416]
	stp	q28, q29, [x0, #448]
	stp	q30, q31, [x0, #480]

	ldr	x1, [sp, #(768 + 168)]	// out
	mov	x2, #0
1:	ldr	x3, [sp, x2]
	str	x3, [x1, x2]
	add	x2, x2, #8
	cmp	x2, #768
	b.ne	1b
	ldr	x0, [sp, #0]
	add	sp, sp, #768

	ldp	x19, x20, [sp, #16]
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x25, x26, [sp, #64]
	ldp	x27, x28, [sp, #80]
	ldp	d8, d9, [sp, #96]
	ldp	d10, d11, [sp, #112]
	ldp	d12, d13, [sp, #128]
	ldp	d14, d15, [sp, #144]
	ldp	x29, x30, [sp], #176
	ret
	.size	bpftime_test_svc_with_patterns, .-bpftime_test_svc_with_patterns

// A signal restorer of its own (SA_RESTORER), as musl and the Go runtime
// install: the rt_sigreturn it issues is in rewritten code.
	.globl	bpftime_test_restorer
	.type	bpftime_test_restorer, %function
	.p2align 2
bpftime_test_restorer:
	mov	x8, #139
	svc	#0
	.size	bpftime_test_restorer, .-bpftime_test_restorer

	.section .note.GNU-stack,"",%progbits
