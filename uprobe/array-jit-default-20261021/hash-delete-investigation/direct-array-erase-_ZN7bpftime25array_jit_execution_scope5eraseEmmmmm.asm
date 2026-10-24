
/home/jetson/src/results/uprobe/array-jit-default-20261021/hash-delete-investigation/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e9080 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm>:
  4e9080:	stp	x29, x30, [sp, #-64]!
  4e9084:	mrs	x5, tpidr_el0
  4e9088:	mov	x29, sp
  4e908c:	stp	x19, x20, [sp, #16]
  4e9090:	mov	x19, x0
  4e9094:	adrp	x0, 2e51000 <strcasecmp@GLIBC_2.17>
  4e9098:	ldr	x6, [x0, #624]
  4e909c:	add	x0, x0, #0x270
  4e90a0:	blr	x6
  4e90a4:	stp	x23, x24, [sp, #48]
  4e90a8:	ldr	x23, [x5, x0]
  4e90ac:	cbz	x23, 4e917c <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xfc>
  4e90b0:	mov	x20, x1
  4e90b4:	mov	x24, x4
  4e90b8:	mov	x1, x19
  4e90bc:	mov	x0, x23
  4e90c0:	stp	x21, x22, [sp, #32]
  4e90c4:	mov	x21, x2
  4e90c8:	mov	x22, x3
  4e90cc:	bl	4e8d24 <_ZN7bpftime25array_jit_execution_scope10find_arrayEm>
  4e90d0:	cbz	x0, 4e90f8 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x78>
  4e90d4:	bl	48c090 <__errno_location@plt>
  4e90d8:	ldp	x21, x22, [sp, #32]
  4e90dc:	mov	w1, #0x16                  	// #22
  4e90e0:	str	w1, [x0]
  4e90e4:	mov	x0, #0xffffffffffffffff    	// #-1
  4e90e8:	ldp	x19, x20, [sp, #16]
  4e90ec:	ldp	x23, x24, [sp, #48]
  4e90f0:	ldp	x29, x30, [sp], #64
  4e90f4:	ret
  4e90f8:	ldr	x0, [x23]
  4e90fc:	add	x4, x0, #0x88
  4e9100:	ldr	x5, [x0, #144]
  4e9104:	cbz	x5, 4e919c <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x11c>
  4e9108:	mov	x3, x4
  4e910c:	nop
  4e9110:	ldr	w0, [x5, #32]
  4e9114:	ldp	x1, x2, [x5, #16]
  4e9118:	cmp	w0, #0x2
  4e911c:	b.hi	4e916c <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xec>  // b.pmore
  4e9120:	mov	x5, x2
  4e9124:	cbnz	x5, 4e9110 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x90>
  4e9128:	cmp	x3, x4
  4e912c:	b.eq	4e919c <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x11c>  // b.none
  4e9130:	ldr	w0, [x3, #32]
  4e9134:	cmp	w0, #0x3
  4e9138:	b.hi	4e919c <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x11c>  // b.pmore
  4e913c:	ldr	x5, [x3, #40]
  4e9140:	mov	x4, x24
  4e9144:	ldp	x23, x24, [sp, #48]
  4e9148:	mov	x2, x21
  4e914c:	mov	x1, x20
  4e9150:	mov	x0, x19
  4e9154:	ldp	x19, x20, [sp, #16]
  4e9158:	mov	x3, x22
  4e915c:	ldp	x21, x22, [sp, #32]
  4e9160:	mov	x16, x5
  4e9164:	ldp	x29, x30, [sp], #64
  4e9168:	br	x16
  4e916c:	mov	x3, x5
  4e9170:	mov	x5, x1
  4e9174:	cbnz	x5, 4e9110 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x90>
  4e9178:	b	4e9128 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xa8>
  4e917c:	bl	48c090 <__errno_location@plt>
  4e9180:	mov	w1, #0x2                   	// #2
  4e9184:	str	w1, [x0]
  4e9188:	ldp	x19, x20, [sp, #16]
  4e918c:	mov	x0, #0xffffffffffffffff    	// #-1
  4e9190:	ldp	x23, x24, [sp, #48]
  4e9194:	ldp	x29, x30, [sp], #64
  4e9198:	ret
  4e919c:	adrp	x0, 1d3e000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf778>
  4e91a0:	add	x0, x0, #0x168
  4e91a4:	bl	48c490 <_ZSt20__throw_out_of_rangePKc@plt>

Disassembly of section .fini:
