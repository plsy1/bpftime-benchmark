
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/slot-helper/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e6790 <bpftime_array_map_lookup_elem_helper>:
  4e6790:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e6794:	ldr	x2, [x2, #2184]
  4e6798:	cmp	x2, #0x0
  4e679c:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e67a0:	b.lt	4e6848 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.tstop
  4e67a4:	ldr	x3, [x2, #16]
  4e67a8:	cmp	x3, w0, sxtw
  4e67ac:	b.ls	4e6848 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.plast
  4e67b0:	ldr	x3, [x2, #8]
  4e67b4:	mov	w6, #0xa8                  	// #168
  4e67b8:	add	x2, x2, #0x8
  4e67bc:	cmp	x3, #0x1
  4e67c0:	add	x4, x2, x3
  4e67c4:	smull	x3, w0, w6
  4e67c8:	csetm	x2, ne	// ne = any
  4e67cc:	and	x0, x2, x4
  4e67d0:	adds	x0, x0, x3
  4e67d4:	b.eq	4e6848 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e67d8:	ldrb	w2, [x0, #160]
  4e67dc:	cmp	w2, #0x1
  4e67e0:	b.ne	4e6848 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.any
  4e67e4:	ldr	w2, [x0, #72]
  4e67e8:	cmp	w2, #0x2
  4e67ec:	b.eq	4e67f8 <bpftime_array_map_lookup_elem_helper+0x68>  // b.none
  4e67f0:	mov	w2, #0x0                   	// #0
  4e67f4:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e67f8:	ldr	w2, [x1]
  4e67fc:	add	x0, x0, #0x78
  4e6800:	ldr	x1, [x0]
  4e6804:	cmp	x1, #0x1
  4e6808:	add	x0, x0, x1
  4e680c:	csetm	x1, ne	// ne = any
  4e6810:	and	x1, x1, x0
  4e6814:	ldr	w0, [x1, #36]
  4e6818:	cmp	w2, w0
  4e681c:	b.cs	4e6848 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.hs, b.nlast
  4e6820:	ldr	x3, [x1, #8]
  4e6824:	add	x0, x1, #0x8
  4e6828:	ldr	w4, [x1, #32]
  4e682c:	cmp	x3, #0x1
  4e6830:	add	x1, x0, x3
  4e6834:	csetm	x0, ne	// ne = any
  4e6838:	and	x0, x0, x1
  4e683c:	mul	w2, w2, w4
  4e6840:	add	x0, x0, x2
  4e6844:	ret
  4e6848:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
