
/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate/build-uprobe-handler-fast-access-20261016/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e5844 <bpftime_array_map_lookup_elem_helper>:
  4e5844:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e5848:	ldr	x2, [x2, #2184]
  4e584c:	cmp	x2, #0x0
  4e5850:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e5854:	b.lt	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.tstop
  4e5858:	ldr	x3, [x2, #16]
  4e585c:	cmp	x3, w0, sxtw
  4e5860:	b.ls	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.plast
  4e5864:	ldr	x3, [x2, #8]
  4e5868:	mov	w6, #0xa8                  	// #168
  4e586c:	add	x2, x2, #0x8
  4e5870:	cmp	x3, #0x1
  4e5874:	add	x4, x2, x3
  4e5878:	smull	x3, w0, w6
  4e587c:	csetm	x2, ne	// ne = any
  4e5880:	and	x0, x2, x4
  4e5884:	adds	x0, x0, x3
  4e5888:	b.eq	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e588c:	ldrb	w2, [x0, #160]
  4e5890:	cmp	w2, #0x1
  4e5894:	b.ne	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.any
  4e5898:	ldr	w2, [x0, #72]
  4e589c:	cmp	w2, #0x2
  4e58a0:	b.eq	4e58ac <bpftime_array_map_lookup_elem_helper+0x68>  // b.none
  4e58a4:	mov	w2, #0x0                   	// #0
  4e58a8:	b	4e20e0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e58ac:	ldr	w2, [x1]
  4e58b0:	add	x0, x0, #0x78
  4e58b4:	ldr	x1, [x0]
  4e58b8:	cmp	x1, #0x1
  4e58bc:	add	x0, x0, x1
  4e58c0:	csetm	x1, ne	// ne = any
  4e58c4:	and	x1, x1, x0
  4e58c8:	add	x3, x1, #0x8
  4e58cc:	ldr	x0, [x1, #8]
  4e58d0:	ldr	w4, [x1, #36]
  4e58d4:	cmp	x0, #0x1
  4e58d8:	add	x3, x3, x0
  4e58dc:	csetm	x0, ne	// ne = any
  4e58e0:	ldr	w1, [x1, #32]
  4e58e4:	and	x0, x0, x3
  4e58e8:	cmp	w2, w4
  4e58ec:	b.cs	4e58fc <bpftime_array_map_lookup_elem_helper+0xb8>  // b.hs, b.nlast
  4e58f0:	mul	w2, w2, w1
  4e58f4:	add	x0, x0, x2
  4e58f8:	ret
  4e58fc:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
