
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/slot-helper/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e6900 <bpftime_percpu_array_map_lookup_elem_helper>:
  4e6900:	stp	x29, x30, [sp, #-16]!
  4e6904:	adrp	x2, 2e38000 <trace_init_lock+0x28>
  4e6908:	mov	x29, sp
  4e690c:	ldr	x2, [x2, #2184]
  4e6910:	cmp	x2, #0x0
  4e6914:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e6918:	b.ge	4e6934 <bpftime_percpu_array_map_lookup_elem_helper+0x34>  // b.tcont
  4e691c:	bl	48ab80 <__errno_location@plt>
  4e6920:	mov	w1, #0x2                   	// #2
  4e6924:	str	w1, [x0]
  4e6928:	mov	x0, #0x0                   	// #0
  4e692c:	ldp	x29, x30, [sp], #16
  4e6930:	ret
  4e6934:	ldr	x4, [x2, #16]
  4e6938:	cmp	x4, w0, sxtw
  4e693c:	b.ls	4e691c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.plast
  4e6940:	ldr	x4, [x2, #8]
  4e6944:	mov	w5, #0xa8                  	// #168
  4e6948:	add	x2, x2, #0x8
  4e694c:	cmp	x4, #0x1
  4e6950:	add	x2, x2, x4
  4e6954:	smull	x0, w0, w5
  4e6958:	csetm	x4, ne	// ne = any
  4e695c:	and	x2, x4, x2
  4e6960:	adds	x0, x2, x0
  4e6964:	b.eq	4e691c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.none
  4e6968:	ldrb	w2, [x0, #160]
  4e696c:	cmp	w2, #0x1
  4e6970:	b.ne	4e691c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.any
  4e6974:	ldr	w2, [x0, #72]
  4e6978:	cmp	w2, #0x6
  4e697c:	b.eq	4e698c <bpftime_percpu_array_map_lookup_elem_helper+0x8c>  // b.none
  4e6980:	ldp	x29, x30, [sp], #16
  4e6984:	mov	w2, #0x0                   	// #0
  4e6988:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e698c:	mov	x3, x1
  4e6990:	add	x0, x0, #0x78
  4e6994:	ldr	x1, [x0]
  4e6998:	cmp	x1, #0x1
  4e699c:	add	x0, x0, x1
  4e69a0:	csetm	x2, ne	// ne = any
  4e69a4:	and	x2, x2, x0
  4e69a8:	cbz	x3, 4e69f0 <bpftime_percpu_array_map_lookup_elem_helper+0xf0>
  4e69ac:	ldr	w1, [x2, #40]
  4e69b0:	ldr	w0, [x3]
  4e69b4:	cmp	w0, w1
  4e69b8:	b.cs	4e69f0 <bpftime_percpu_array_map_lookup_elem_helper+0xf0>  // b.hs, b.nlast
  4e69bc:	ldr	x5, [x2, #8]
  4e69c0:	add	x4, x2, #0x8
  4e69c4:	ldrsw	x3, [x2, #32]
  4e69c8:	mov	w0, w0
  4e69cc:	ldr	w1, [x2, #36]
  4e69d0:	cmp	x5, #0x1
  4e69d4:	add	x4, x4, x5
  4e69d8:	csetm	x2, ne	// ne = any
  4e69dc:	and	x2, x2, x4
  4e69e0:	ldp	x29, x30, [sp], #16
  4e69e4:	mul	x3, x3, x1
  4e69e8:	madd	x0, x3, x0, x2
  4e69ec:	b	4f02b0 <bpftime::per_cpu_array_map_impl::lookup_cpu_slot(unsigned char*, unsigned long)>
  4e69f0:	bl	48a680 <sched_getcpu@plt>
  4e69f4:	b	4e691c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>

Disassembly of section .fini:
