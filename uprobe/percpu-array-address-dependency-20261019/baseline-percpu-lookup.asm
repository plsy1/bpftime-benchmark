
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/baseline/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e5730 <bpftime_percpu_array_map_lookup_elem_helper>:
  4e5730:	stp	x29, x30, [sp, #-32]!
  4e5734:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e5738:	mov	x29, sp
  4e573c:	ldr	x2, [x2, #2184]
  4e5740:	cmp	x2, #0x0
  4e5744:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e5748:	b.ge	4e5764 <bpftime_percpu_array_map_lookup_elem_helper+0x34>  // b.tcont
  4e574c:	bl	48ab80 <__errno_location@plt>
  4e5750:	mov	w1, #0x2                   	// #2
  4e5754:	str	w1, [x0]
  4e5758:	mov	x0, #0x0                   	// #0
  4e575c:	ldp	x29, x30, [sp], #32
  4e5760:	ret
  4e5764:	ldr	x3, [x2, #16]
  4e5768:	cmp	x3, w0, sxtw
  4e576c:	b.ls	4e574c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.plast
  4e5770:	ldr	x3, [x2, #8]
  4e5774:	mov	w4, #0xa8                  	// #168
  4e5778:	add	x2, x2, #0x8
  4e577c:	cmp	x3, #0x1
  4e5780:	add	x2, x2, x3
  4e5784:	smull	x0, w0, w4
  4e5788:	csetm	x3, ne	// ne = any
  4e578c:	and	x2, x3, x2
  4e5790:	adds	x0, x2, x0
  4e5794:	b.eq	4e574c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.none
  4e5798:	ldrb	w2, [x0, #160]
  4e579c:	cmp	w2, #0x1
  4e57a0:	b.ne	4e574c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.any
  4e57a4:	ldr	w2, [x0, #72]
  4e57a8:	cmp	w2, #0x6
  4e57ac:	b.eq	4e57bc <bpftime_percpu_array_map_lookup_elem_helper+0x8c>  // b.none
  4e57b0:	ldp	x29, x30, [sp], #32
  4e57b4:	mov	w2, #0x0                   	// #0
  4e57b8:	b	4e20e0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e57bc:	stp	x19, x20, [sp, #16]
  4e57c0:	mov	x19, x1
  4e57c4:	add	x0, x0, #0x78
  4e57c8:	ldr	x1, [x0]
  4e57cc:	cmp	x1, #0x1
  4e57d0:	add	x0, x0, x1
  4e57d4:	csetm	x20, ne	// ne = any
  4e57d8:	and	x20, x20, x0
  4e57dc:	bl	48a680 <sched_getcpu@plt>
  4e57e0:	cbz	x19, 4e582c <bpftime_percpu_array_map_lookup_elem_helper+0xfc>
  4e57e4:	ldr	w1, [x19]
  4e57e8:	ldr	w2, [x20, #40]
  4e57ec:	cmp	w1, w2
  4e57f0:	b.cs	4e582c <bpftime_percpu_array_map_lookup_elem_helper+0xfc>  // b.hs, b.nlast
  4e57f4:	ldr	x3, [x20, #8]
  4e57f8:	mov	w2, w1
  4e57fc:	ldrsw	x1, [x20, #32]
  4e5800:	sxtw	x0, w0
  4e5804:	add	x4, x20, #0x8
  4e5808:	cmp	x3, #0x1
  4e580c:	ldr	w5, [x20, #36]
  4e5810:	add	x4, x4, x3
  4e5814:	csetm	x3, ne	// ne = any
  4e5818:	madd	x0, x1, x2, x0
  4e581c:	and	x1, x3, x4
  4e5820:	ldp	x19, x20, [sp, #16]
  4e5824:	madd	x0, x0, x5, x1
  4e5828:	b	4e575c <bpftime_percpu_array_map_lookup_elem_helper+0x2c>
  4e582c:	bl	48ab80 <__errno_location@plt>
  4e5830:	ldp	x19, x20, [sp, #16]
  4e5834:	mov	w1, #0x2                   	// #2
  4e5838:	str	w1, [x0]
  4e583c:	mov	x0, #0x0                   	// #0
  4e5840:	b	4e575c <bpftime_percpu_array_map_lookup_elem_helper+0x2c>

Disassembly of section .fini:
