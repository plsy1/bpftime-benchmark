
/tmp/bpftime-percpu-array-cold-20261019.YHrmOV/cold-errors/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e77b4 <bpftime_percpu_array_map_lookup_elem_helper>:
  4e77b4:	adrp	x2, 2e48000 <trace_init_lock+0x28>
  4e77b8:	ldr	x2, [x2, #2184]
  4e77bc:	cmp	x2, #0x0
  4e77c0:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e77c4:	b.lt	4e78a0 <bpftime_percpu_array_map_lookup_elem_helper+0xec>  // b.tstop
  4e77c8:	ldr	x3, [x2, #16]
  4e77cc:	cmp	x3, w0, sxtw
  4e77d0:	b.ls	4e78a0 <bpftime_percpu_array_map_lookup_elem_helper+0xec>  // b.plast
  4e77d4:	ldr	x3, [x2, #8]
  4e77d8:	mov	w5, #0xa8                  	// #168
  4e77dc:	add	x2, x2, #0x8
  4e77e0:	cmp	x3, #0x1
  4e77e4:	add	x4, x2, x3
  4e77e8:	smull	x3, w0, w5
  4e77ec:	csetm	x2, ne	// ne = any
  4e77f0:	and	x0, x2, x4
  4e77f4:	adds	x0, x0, x3
  4e77f8:	b.eq	4e78a0 <bpftime_percpu_array_map_lookup_elem_helper+0xec>  // b.none
  4e77fc:	ldrb	w2, [x0, #160]
  4e7800:	cmp	w2, #0x1
  4e7804:	b.ne	4e78a0 <bpftime_percpu_array_map_lookup_elem_helper+0xec>  // b.any
  4e7808:	ldr	w2, [x0, #72]
  4e780c:	cmp	w2, #0x6
  4e7810:	b.eq	4e781c <bpftime_percpu_array_map_lookup_elem_helper+0x68>  // b.none
  4e7814:	mov	w2, #0x0                   	// #0
  4e7818:	b	4e20f0 <bpftime::bpf_map_handler::map_lookup_elem(void const*, bool) const>
  4e781c:	stp	x29, x30, [sp, #-32]!
  4e7820:	add	x0, x0, #0x78
  4e7824:	mov	x29, sp
  4e7828:	stp	x19, x20, [sp, #16]
  4e782c:	mov	x19, x1
  4e7830:	ldr	x1, [x0]
  4e7834:	cmp	x1, #0x1
  4e7838:	add	x0, x0, x1
  4e783c:	csetm	x20, ne	// ne = any
  4e7840:	and	x20, x20, x0
  4e7844:	bl	48a680 <sched_getcpu@plt>
  4e7848:	cbz	x19, 4e7898 <bpftime_percpu_array_map_lookup_elem_helper+0xe4>
  4e784c:	ldr	w1, [x19]
  4e7850:	ldr	w2, [x20, #40]
  4e7854:	cmp	w1, w2
  4e7858:	b.cs	4e7898 <bpftime_percpu_array_map_lookup_elem_helper+0xe4>  // b.hs, b.nlast
  4e785c:	ldr	x3, [x20, #8]
  4e7860:	mov	w2, w1
  4e7864:	ldrsw	x4, [x20, #32]
  4e7868:	sxtw	x1, w0
  4e786c:	add	x5, x20, #0x8
  4e7870:	cmp	x3, #0x1
  4e7874:	ldr	w6, [x20, #36]
  4e7878:	add	x5, x5, x3
  4e787c:	csetm	x3, ne	// ne = any
  4e7880:	madd	x0, x4, x2, x1
  4e7884:	and	x3, x3, x5
  4e7888:	ldp	x19, x20, [sp, #16]
  4e788c:	ldp	x29, x30, [sp], #32
  4e7890:	madd	x0, x0, x6, x3
  4e7894:	ret
  4e7898:	ldp	x19, x20, [sp, #16]
  4e789c:	ldp	x29, x30, [sp], #32
  4e78a0:	b	48c9ac <bpftime::detail::array_lookup_enoent()>

Disassembly of section .fini:
