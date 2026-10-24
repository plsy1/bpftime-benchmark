
/tmp/bpftime-percpu-array-cold-20261019.YHrmOV/cold-errors/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e78a4 <bpftime_array_map_lookup_elem_helper>:
  4e78a4:	adrp	x2, 2e48000 <trace_init_lock+0x28>
  4e78a8:	ldr	x2, [x2, #2184]
  4e78ac:	cmp	x2, #0x0
  4e78b0:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e78b4:	b.lt	4e795c <bpftime_array_map_lookup_elem_helper+0xb8>  // b.tstop
  4e78b8:	ldr	x3, [x2, #16]
  4e78bc:	cmp	x3, w0, sxtw
  4e78c0:	b.ls	4e795c <bpftime_array_map_lookup_elem_helper+0xb8>  // b.plast
  4e78c4:	ldr	x3, [x2, #8]
  4e78c8:	mov	w6, #0xa8                  	// #168
  4e78cc:	add	x2, x2, #0x8
  4e78d0:	cmp	x3, #0x1
  4e78d4:	add	x4, x2, x3
  4e78d8:	smull	x3, w0, w6
  4e78dc:	csetm	x2, ne	// ne = any
  4e78e0:	and	x0, x2, x4
  4e78e4:	adds	x0, x0, x3
  4e78e8:	b.eq	4e795c <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e78ec:	ldrb	w2, [x0, #160]
  4e78f0:	cmp	w2, #0x1
  4e78f4:	b.ne	4e795c <bpftime_array_map_lookup_elem_helper+0xb8>  // b.any
  4e78f8:	ldr	w2, [x0, #72]
  4e78fc:	cmp	w2, #0x2
  4e7900:	b.eq	4e790c <bpftime_array_map_lookup_elem_helper+0x68>  // b.none
  4e7904:	mov	w2, #0x0                   	// #0
  4e7908:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e790c:	ldr	w2, [x1]
  4e7910:	add	x0, x0, #0x78
  4e7914:	ldr	x1, [x0]
  4e7918:	cmp	x1, #0x1
  4e791c:	add	x0, x0, x1
  4e7920:	csetm	x1, ne	// ne = any
  4e7924:	and	x1, x1, x0
  4e7928:	ldr	w0, [x1, #36]
  4e792c:	cmp	w2, w0
  4e7930:	b.cs	4e795c <bpftime_array_map_lookup_elem_helper+0xb8>  // b.hs, b.nlast
  4e7934:	ldr	x3, [x1, #8]
  4e7938:	add	x0, x1, #0x8
  4e793c:	ldr	w4, [x1, #32]
  4e7940:	cmp	x3, #0x1
  4e7944:	add	x1, x0, x3
  4e7948:	csetm	x0, ne	// ne = any
  4e794c:	and	x0, x0, x1
  4e7950:	mul	w2, w2, w4
  4e7954:	add	x0, x0, x2
  4e7958:	ret
  4e795c:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
