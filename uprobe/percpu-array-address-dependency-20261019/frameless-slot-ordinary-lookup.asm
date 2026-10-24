
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/frameless-slot/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e57d0 <bpftime_array_map_lookup_elem_helper>:
  4e57d0:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e57d4:	ldr	x2, [x2, #2184]
  4e57d8:	cmp	x2, #0x0
  4e57dc:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e57e0:	b.lt	4e5888 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.tstop
  4e57e4:	ldr	x3, [x2, #16]
  4e57e8:	cmp	x3, w0, sxtw
  4e57ec:	b.ls	4e5888 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.plast
  4e57f0:	ldr	x3, [x2, #8]
  4e57f4:	mov	w6, #0xa8                  	// #168
  4e57f8:	add	x2, x2, #0x8
  4e57fc:	cmp	x3, #0x1
  4e5800:	add	x4, x2, x3
  4e5804:	smull	x3, w0, w6
  4e5808:	csetm	x2, ne	// ne = any
  4e580c:	and	x0, x2, x4
  4e5810:	adds	x0, x0, x3
  4e5814:	b.eq	4e5888 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e5818:	ldrb	w2, [x0, #160]
  4e581c:	cmp	w2, #0x1
  4e5820:	b.ne	4e5888 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.any
  4e5824:	ldr	w2, [x0, #72]
  4e5828:	cmp	w2, #0x2
  4e582c:	b.eq	4e5838 <bpftime_array_map_lookup_elem_helper+0x68>  // b.none
  4e5830:	mov	w2, #0x0                   	// #0
  4e5834:	b	4e2120 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e5838:	ldr	w2, [x1]
  4e583c:	add	x0, x0, #0x78
  4e5840:	ldr	x1, [x0]
  4e5844:	cmp	x1, #0x1
  4e5848:	add	x0, x0, x1
  4e584c:	csetm	x1, ne	// ne = any
  4e5850:	and	x1, x1, x0
  4e5854:	ldr	w0, [x1, #36]
  4e5858:	cmp	w2, w0
  4e585c:	b.cs	4e5888 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.hs, b.nlast
  4e5860:	ldr	x3, [x1, #8]
  4e5864:	add	x0, x1, #0x8
  4e5868:	ldr	w4, [x1, #32]
  4e586c:	cmp	x3, #0x1
  4e5870:	add	x1, x0, x3
  4e5874:	csetm	x0, ne	// ne = any
  4e5878:	and	x0, x0, x1
  4e587c:	mul	w2, w2, w4
  4e5880:	add	x0, x0, x2
  4e5884:	ret
  4e5888:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
