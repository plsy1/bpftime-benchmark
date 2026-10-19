
/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/baseline/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e8420 <bpftime_array_map_lookup_elem_helper>:
  4e8420:	stp	x29, x30, [sp, #-32]!
  4e8424:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e8428:	mov	x29, sp
  4e842c:	ldr	x2, [x2, #2184]
  4e8430:	cmp	x2, #0x0
  4e8434:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e8438:	b.ge	4e8454 <bpftime_array_map_lookup_elem_helper+0x34>  // b.tcont
  4e843c:	bl	48ab80 <__errno_location@plt>
  4e8440:	mov	w1, #0x2                   	// #2
  4e8444:	str	w1, [x0]
  4e8448:	mov	x0, #0x0                   	// #0
  4e844c:	ldp	x29, x30, [sp], #32
  4e8450:	ret
  4e8454:	ldr	x4, [x2, #16]
  4e8458:	cmp	x4, w0, sxtw
  4e845c:	b.ls	4e843c <bpftime_array_map_lookup_elem_helper+0x1c>  // b.plast
  4e8460:	ldr	x4, [x2, #8]
  4e8464:	mov	w5, #0xa8                  	// #168
  4e8468:	add	x2, x2, #0x8
  4e846c:	cmp	x4, #0x1
  4e8470:	add	x2, x2, x4
  4e8474:	smull	x0, w0, w5
  4e8478:	csetm	x4, ne	// ne = any
  4e847c:	and	x2, x4, x2
  4e8480:	adds	x0, x2, x0
  4e8484:	b.eq	4e843c <bpftime_array_map_lookup_elem_helper+0x1c>  // b.none
  4e8488:	ldrb	w2, [x0, #160]
  4e848c:	cmp	w2, #0x1
  4e8490:	b.ne	4e843c <bpftime_array_map_lookup_elem_helper+0x1c>  // b.any
  4e8494:	str	x19, [sp, #16]
  4e8498:	ldr	w19, [x0, #72]
  4e849c:	cmp	w19, #0x2
  4e84a0:	b.eq	4e84b4 <bpftime_array_map_lookup_elem_helper+0x94>  // b.none
  4e84a4:	ldr	x19, [sp, #16]
  4e84a8:	mov	w2, #0x0                   	// #0
  4e84ac:	ldp	x29, x30, [sp], #32
  4e84b0:	b	4e50d0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e84b4:	ldr	w2, [x1]
  4e84b8:	add	x0, x0, #0x78
  4e84bc:	ldr	x1, [x0]
  4e84c0:	cmp	x1, #0x1
  4e84c4:	add	x0, x0, x1
  4e84c8:	csetm	x1, ne	// ne = any
  4e84cc:	and	x0, x1, x0
  4e84d0:	ldr	w1, [x0, #36]
  4e84d4:	cmp	w2, w1
  4e84d8:	b.cs	4e8508 <bpftime_array_map_lookup_elem_helper+0xe8>  // b.hs, b.nlast
  4e84dc:	ldr	x3, [x0, #8]
  4e84e0:	add	x1, x0, #0x8
  4e84e4:	ldr	w4, [x0, #32]
  4e84e8:	cmp	x3, #0x1
  4e84ec:	add	x1, x1, x3
  4e84f0:	csetm	x0, ne	// ne = any
  4e84f4:	and	x0, x0, x1
  4e84f8:	mul	w2, w2, w4
  4e84fc:	ldr	x19, [sp, #16]
  4e8500:	add	x0, x0, x2
  4e8504:	b	4e844c <bpftime_array_map_lookup_elem_helper+0x2c>
  4e8508:	bl	48ab80 <__errno_location@plt>
  4e850c:	mov	x1, x0
  4e8510:	mov	x0, #0x0                   	// #0
  4e8514:	str	w19, [x1]
  4e8518:	ldr	x19, [sp, #16]
  4e851c:	b	4e844c <bpftime_array_map_lookup_elem_helper+0x2c>

Disassembly of section .fini:
