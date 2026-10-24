
/tmp/bpftime-percpu-array-address-20261019.S1mHZP/address-first/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e6770 <bpftime_percpu_array_map_lookup_elem_helper>:
  4e6770:	stp	x29, x30, [sp, #-48]!
  4e6774:	adrp	x2, 2e48000 <trace_init_lock+0x28>
  4e6778:	mov	x29, sp
  4e677c:	ldr	x2, [x2, #2184]
  4e6780:	cmp	x2, #0x0
  4e6784:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e6788:	b.ge	4e67a4 <bpftime_percpu_array_map_lookup_elem_helper+0x34>  // b.tcont
  4e678c:	bl	48ab80 <__errno_location@plt>
  4e6790:	mov	w1, #0x2                   	// #2
  4e6794:	str	w1, [x0]
  4e6798:	mov	x0, #0x0                   	// #0
  4e679c:	ldp	x29, x30, [sp], #48
  4e67a0:	ret
  4e67a4:	ldr	x4, [x2, #16]
  4e67a8:	cmp	x4, w0, sxtw
  4e67ac:	b.ls	4e678c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.plast
  4e67b0:	ldr	x4, [x2, #8]
  4e67b4:	mov	w5, #0xa8                  	// #168
  4e67b8:	add	x2, x2, #0x8
  4e67bc:	cmp	x4, #0x1
  4e67c0:	add	x2, x2, x4
  4e67c4:	smull	x0, w0, w5
  4e67c8:	csetm	x4, ne	// ne = any
  4e67cc:	and	x2, x4, x2
  4e67d0:	adds	x0, x2, x0
  4e67d4:	b.eq	4e678c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.none
  4e67d8:	ldrb	w2, [x0, #160]
  4e67dc:	cmp	w2, #0x1
  4e67e0:	b.ne	4e678c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>  // b.any
  4e67e4:	ldr	w2, [x0, #72]
  4e67e8:	cmp	w2, #0x6
  4e67ec:	b.eq	4e67fc <bpftime_percpu_array_map_lookup_elem_helper+0x8c>  // b.none
  4e67f0:	ldp	x29, x30, [sp], #48
  4e67f4:	mov	w2, #0x0                   	// #0
  4e67f8:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e67fc:	mov	x3, x1
  4e6800:	add	x0, x0, #0x78
  4e6804:	ldr	x1, [x0]
  4e6808:	cmp	x1, #0x1
  4e680c:	add	x0, x0, x1
  4e6810:	csetm	x1, ne	// ne = any
  4e6814:	and	x0, x1, x0
  4e6818:	cbz	x3, 4e6878 <bpftime_percpu_array_map_lookup_elem_helper+0x108>
  4e681c:	ldr	w2, [x0, #40]
  4e6820:	ldr	w1, [x3]
  4e6824:	cmp	w1, w2
  4e6828:	b.cs	4e6878 <bpftime_percpu_array_map_lookup_elem_helper+0x108>  // b.hs, b.nlast
  4e682c:	stp	x19, x20, [sp, #16]
  4e6830:	mov	w1, w1
  4e6834:	ldrsw	x20, [x0, #32]
  4e6838:	stp	x21, x22, [sp, #32]
  4e683c:	add	x21, x0, #0x8
  4e6840:	ldr	w22, [x0, #36]
  4e6844:	ldr	x2, [x0, #8]
  4e6848:	mul	x20, x20, x1
  4e684c:	cmp	x2, #0x1
  4e6850:	add	x21, x21, x2
  4e6854:	cset	x19, eq	// eq = none
  4e6858:	bl	48a680 <sched_getcpu@plt>
  4e685c:	add	x0, x20, w0, sxtw
  4e6860:	sub	x1, x19, #0x1
  4e6864:	and	x1, x1, x21
  4e6868:	ldp	x19, x20, [sp, #16]
  4e686c:	madd	x0, x0, x22, x1
  4e6870:	ldp	x21, x22, [sp, #32]
  4e6874:	b	4e679c <bpftime_percpu_array_map_lookup_elem_helper+0x2c>
  4e6878:	bl	48a680 <sched_getcpu@plt>
  4e687c:	b	4e678c <bpftime_percpu_array_map_lookup_elem_helper+0x1c>

Disassembly of section .fini:
