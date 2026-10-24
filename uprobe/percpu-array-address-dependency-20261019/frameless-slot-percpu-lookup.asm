
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/frameless-slot/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e5940 <bpftime_percpu_array_map_lookup_elem_helper>:
  4e5940:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e5944:	mov	x5, x1
  4e5948:	ldr	x2, [x2, #2184]
  4e594c:	cmp	x2, #0x0
  4e5950:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e5954:	b.lt	4e5a0c <bpftime_percpu_array_map_lookup_elem_helper+0xcc>  // b.tstop
  4e5958:	ldr	x3, [x2, #16]
  4e595c:	cmp	x3, w0, sxtw
  4e5960:	b.ls	4e5a0c <bpftime_percpu_array_map_lookup_elem_helper+0xcc>  // b.plast
  4e5964:	ldr	x3, [x2, #8]
  4e5968:	mov	w6, #0xa8                  	// #168
  4e596c:	add	x2, x2, #0x8
  4e5970:	cmp	x3, #0x1
  4e5974:	add	x4, x2, x3
  4e5978:	smull	x3, w0, w6
  4e597c:	csetm	x2, ne	// ne = any
  4e5980:	and	x0, x2, x4
  4e5984:	adds	x0, x0, x3
  4e5988:	b.eq	4e5a0c <bpftime_percpu_array_map_lookup_elem_helper+0xcc>  // b.none
  4e598c:	ldrb	w2, [x0, #160]
  4e5990:	cmp	w2, #0x1
  4e5994:	b.ne	4e5a0c <bpftime_percpu_array_map_lookup_elem_helper+0xcc>  // b.any
  4e5998:	ldr	w2, [x0, #72]
  4e599c:	cmp	w2, #0x6
  4e59a0:	b.eq	4e59ac <bpftime_percpu_array_map_lookup_elem_helper+0x6c>  // b.none
  4e59a4:	mov	w2, #0x0                   	// #0
  4e59a8:	b	4e2120 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e59ac:	ldr	x1, [x0, #120]
  4e59b0:	add	x0, x0, #0x78
  4e59b4:	cmp	x1, #0x1
  4e59b8:	add	x0, x0, x1
  4e59bc:	csetm	x2, ne	// ne = any
  4e59c0:	and	x2, x2, x0
  4e59c4:	cbz	x5, 4e5a08 <bpftime_percpu_array_map_lookup_elem_helper+0xc8>
  4e59c8:	ldr	w1, [x2, #40]
  4e59cc:	ldr	w0, [x5]
  4e59d0:	cmp	w0, w1
  4e59d4:	b.cs	4e5a08 <bpftime_percpu_array_map_lookup_elem_helper+0xc8>  // b.hs, b.nlast
  4e59d8:	ldr	x5, [x2, #8]
  4e59dc:	mov	w3, w0
  4e59e0:	ldr	w1, [x2, #36]
  4e59e4:	add	x4, x2, #0x8
  4e59e8:	ldrsw	x0, [x2, #32]
  4e59ec:	cmp	x5, #0x1
  4e59f0:	add	x4, x4, x5
  4e59f4:	csetm	x2, ne	// ne = any
  4e59f8:	and	x2, x2, x4
  4e59fc:	mul	x0, x0, x1
  4e5a00:	madd	x0, x0, x3, x2
  4e5a04:	b	4ef2c0 <bpftime::per_cpu_array_map_impl::lookup_cpu_slot(unsigned char*, unsigned long)>
  4e5a08:	b	48c9d0 <bpftime::per_cpu_array_map_impl::lookup_invalid_key()>
  4e5a0c:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
