
/home/jetson/src/results/uprobe/array-jit-default-20261021/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e9434 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm>:
  4e9434:	stp	x29, x30, [sp, #-64]!
  4e9438:	mrs	x5, tpidr_el0
  4e943c:	mov	x29, sp
  4e9440:	stp	x19, x20, [sp, #16]
  4e9444:	mov	x19, x0
  4e9448:	adrp	x0, 2e51000 <strcasecmp@GLIBC_2.17>
  4e944c:	ldr	x6, [x0, #624]
  4e9450:	add	x0, x0, #0x270
  4e9454:	blr	x6
  4e9458:	stp	x23, x24, [sp, #48]
  4e945c:	ldr	x24, [x5, x0]
  4e9460:	cbz	x24, 4e9524 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xf0>
  4e9464:	mov	x20, x1
  4e9468:	mov	x23, x4
  4e946c:	mov	x1, x19
  4e9470:	mov	x0, x24
  4e9474:	stp	x21, x22, [sp, #32]
  4e9478:	mov	x21, x2
  4e947c:	mov	x22, x3
  4e9480:	bl	4e8d24 <_ZN7bpftime25array_jit_execution_scope10find_arrayEm>
  4e9484:	cbz	x0, 4e94a4 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x70>
  4e9488:	ldp	x21, x22, [sp, #32]
  4e948c:	mov	x1, x20
  4e9490:	ldp	x19, x20, [sp, #16]
  4e9494:	ldp	x23, x24, [sp, #48]
  4e9498:	ldp	x29, x30, [sp], #64
  4e949c:	ldr	x0, [x0, #16]
  4e94a0:	b	52f1a0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0>
  4e94a4:	ldr	x0, [x24]
  4e94a8:	add	x4, x0, #0x88
  4e94ac:	ldr	x5, [x0, #144]
  4e94b0:	cbz	x5, 4e9544 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x110>
  4e94b4:	mov	x3, x4
  4e94b8:	ldr	w0, [x5, #32]
  4e94bc:	ldp	x2, x1, [x5, #16]
  4e94c0:	cmp	w0, #0x2
  4e94c4:	b.ls	4e9518 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xe4>  // b.plast
  4e94c8:	mov	x3, x5
  4e94cc:	mov	x5, x2
  4e94d0:	cbnz	x5, 4e94b8 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x84>
  4e94d4:	cmp	x3, x4
  4e94d8:	b.eq	4e9544 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x110>  // b.none
  4e94dc:	ldr	w0, [x3, #32]
  4e94e0:	cmp	w0, #0x3
  4e94e4:	b.hi	4e9544 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x110>  // b.pmore
  4e94e8:	ldr	x5, [x3, #40]
  4e94ec:	mov	x4, x23
  4e94f0:	ldp	x23, x24, [sp, #48]
  4e94f4:	mov	x2, x21
  4e94f8:	mov	x1, x20
  4e94fc:	mov	x0, x19
  4e9500:	ldp	x19, x20, [sp, #16]
  4e9504:	mov	x3, x22
  4e9508:	ldp	x21, x22, [sp, #32]
  4e950c:	mov	x16, x5
  4e9510:	ldp	x29, x30, [sp], #64
  4e9514:	br	x16
  4e9518:	mov	x5, x1
  4e951c:	cbnz	x5, 4e94b8 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0x84>
  4e9520:	b	4e94d4 <_ZN7bpftime25array_jit_execution_scope5eraseEmmmmm+0xa0>
  4e9524:	bl	48c090 <__errno_location@plt>
  4e9528:	ldp	x19, x20, [sp, #16]
  4e952c:	mov	w1, #0x2                   	// #2
  4e9530:	ldp	x23, x24, [sp, #48]
  4e9534:	str	w1, [x0]
  4e9538:	mov	x0, #0xffffffffffffffff    	// #-1
  4e953c:	ldp	x29, x30, [sp], #64
  4e9540:	ret
  4e9544:	adrp	x0, 1d34000 <_ZTSSt23_Sp_counted_ptr_inplaceIN6spdlog5sinks9null_sinkINS0_7details10null_mutexEEESaIvELN9__gnu_cxx12_Lock_policyE2EE+0xf788>
  4e9548:	add	x0, x0, #0x158
  4e954c:	bl	48c490 <_ZSt20__throw_out_of_rangePKc@plt>

Disassembly of section .fini:
