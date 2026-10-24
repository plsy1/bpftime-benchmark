
/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate/build-uprobe-handler-fast-access-20261016/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e5844 <bpftime_array_map_lookup_elem_helper>:
  4e5844:	adrp	x2, 2e48000 <trace_init_lock+0x28>
  4e5848:	mov	x5, x1
  4e584c:	ldr	x2, [x2, #2184]
  4e5850:	cmp	x2, #0x0
  4e5854:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e5858:	b.lt	4e5908 <bpftime_array_map_lookup_elem_helper+0xc4>  // b.tstop
  4e585c:	ldr	x3, [x2, #16]
  4e5860:	cmp	x3, w0, sxtw
  4e5864:	b.ls	4e5908 <bpftime_array_map_lookup_elem_helper+0xc4>  // b.plast
  4e5868:	ldr	x3, [x2, #8]
  4e586c:	mov	w6, #0xa8                  	// #168
  4e5870:	add	x2, x2, #0x8
  4e5874:	cmp	x3, #0x1
  4e5878:	add	x4, x2, x3
  4e587c:	smull	x3, w0, w6
  4e5880:	csetm	x2, ne	// ne = any
  4e5884:	and	x0, x2, x4
  4e5888:	adds	x0, x0, x3
  4e588c:	b.eq	4e5908 <bpftime_array_map_lookup_elem_helper+0xc4>  // b.none
  4e5890:	ldrb	w2, [x0, #160]
  4e5894:	cmp	w2, #0x1
  4e5898:	b.ne	4e5908 <bpftime_array_map_lookup_elem_helper+0xc4>  // b.any
  4e589c:	ldr	w2, [x0, #72]
  4e58a0:	cmp	w2, #0x2
  4e58a4:	b.eq	4e58b0 <bpftime_array_map_lookup_elem_helper+0x6c>  // b.none
  4e58a8:	mov	w2, #0x0                   	// #0
  4e58ac:	b	4e20e0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e58b0:	ldr	x1, [x0, #120]
  4e58b4:	add	x0, x0, #0x78
  4e58b8:	cmp	x1, #0x1
  4e58bc:	b.eq	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e58c0:	add	x0, x0, x1
  4e58c4:	ldr	w1, [x5]
  4e58c8:	ldr	w2, [x0, #36]
  4e58cc:	cmp	w1, w2
  4e58d0:	b.cs	4e5908 <bpftime_array_map_lookup_elem_helper+0xc4>  // b.hs, b.nlast
  4e58d4:	ldr	x3, [x0, #8]
  4e58d8:	add	x2, x0, #0x8
  4e58dc:	ldr	w4, [x0, #32]
  4e58e0:	cmp	x3, #0x1
  4e58e4:	add	x2, x2, x3
  4e58e8:	csetm	x0, ne	// ne = any
  4e58ec:	and	x0, x0, x2
  4e58f0:	mul	w1, w1, w4
  4e58f4:	add	x0, x0, x1
  4e58f8:	ret
  4e58fc:	mov	x0, #0x0                   	// #0
  4e5900:	ldr	w0, [x0, #36]
  4e5904:	brk	#0x3e8
  4e5908:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
