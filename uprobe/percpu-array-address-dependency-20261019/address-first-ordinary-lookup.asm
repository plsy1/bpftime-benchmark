
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/address-first/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e6880 <bpftime_array_map_lookup_elem_helper>:
  4e6880:	adrp	x2, 2e48000 <trace_init_lock+0x28>
  4e6884:	ldr	x2, [x2, #2184]
  4e6888:	cmp	x2, #0x0
  4e688c:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e6890:	b.lt	4e6938 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.tstop
  4e6894:	ldr	x3, [x2, #16]
  4e6898:	cmp	x3, w0, sxtw
  4e689c:	b.ls	4e6938 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.plast
  4e68a0:	ldr	x3, [x2, #8]
  4e68a4:	mov	w6, #0xa8                  	// #168
  4e68a8:	add	x2, x2, #0x8
  4e68ac:	cmp	x3, #0x1
  4e68b0:	add	x4, x2, x3
  4e68b4:	smull	x3, w0, w6
  4e68b8:	csetm	x2, ne	// ne = any
  4e68bc:	and	x0, x2, x4
  4e68c0:	adds	x0, x0, x3
  4e68c4:	b.eq	4e6938 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.none
  4e68c8:	ldrb	w2, [x0, #160]
  4e68cc:	cmp	w2, #0x1
  4e68d0:	b.ne	4e6938 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.any
  4e68d4:	ldr	w2, [x0, #72]
  4e68d8:	cmp	w2, #0x2
  4e68dc:	b.eq	4e68e8 <bpftime_array_map_lookup_elem_helper+0x68>  // b.none
  4e68e0:	mov	w2, #0x0                   	// #0
  4e68e4:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e68e8:	ldr	w2, [x1]
  4e68ec:	add	x0, x0, #0x78
  4e68f0:	ldr	x1, [x0]
  4e68f4:	cmp	x1, #0x1
  4e68f8:	add	x0, x0, x1
  4e68fc:	csetm	x1, ne	// ne = any
  4e6900:	and	x1, x1, x0
  4e6904:	ldr	w0, [x1, #36]
  4e6908:	cmp	w2, w0
  4e690c:	b.cs	4e6938 <bpftime_array_map_lookup_elem_helper+0xb8>  // b.hs, b.nlast
  4e6910:	ldr	x3, [x1, #8]
  4e6914:	add	x0, x1, #0x8
  4e6918:	ldr	w4, [x1, #32]
  4e691c:	cmp	x3, #0x1
  4e6920:	add	x1, x0, x3
  4e6924:	csetm	x0, ne	// ne = any
  4e6928:	and	x0, x0, x1
  4e692c:	mul	w2, w2, w4
  4e6930:	add	x0, x0, x2
  4e6934:	ret
  4e6938:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
