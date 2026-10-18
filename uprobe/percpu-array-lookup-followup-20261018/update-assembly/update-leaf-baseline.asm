
/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb/baseline/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .text:

00000000004f2234 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)>:
  4f2234:	stp	x29, x30, [sp, #-48]!
  4f2238:	mov	x29, sp
  4f223c:	cmp	w3, #0x2
  4f2240:	b.hi	4f22d4 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xa0>  // b.pmore
  4f2244:	stp	x19, x20, [sp, #16]
  4f2248:	mov	x20, x1
  4f224c:	mov	x19, x0
  4f2250:	stp	x21, x22, [sp, #32]
  4f2254:	mov	x22, x3
  4f2258:	mov	x21, x2
  4f225c:	bl	48a680 <sched_getcpu@plt>
  4f2260:	cbz	x20, 4f231c <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xe8>
  4f2264:	ldr	w2, [x19, #40]
  4f2268:	ldr	w1, [x20]
  4f226c:	cmp	w1, w2
  4f2270:	b.cs	4f2300 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xcc>  // b.hs, b.nlast
  4f2274:	cmp	x22, #0x1
  4f2278:	b.eq	4f232c <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xf8>  // b.none
  4f227c:	ldrsw	x3, [x19, #32]
  4f2280:	mov	w1, w1
  4f2284:	sxtw	x0, w0
  4f2288:	ldr	w2, [x19, #36]
  4f228c:	ldr	x4, [x19, #8]
  4f2290:	add	x19, x19, #0x8
  4f2294:	madd	x3, x3, x1, x0
  4f2298:	cmp	x4, #0x1
  4f229c:	add	x19, x19, x4
  4f22a0:	csetm	x1, ne	// ne = any
  4f22a4:	and	x1, x1, x19
  4f22a8:	mul	x3, x3, x2
  4f22ac:	add	x0, x1, x3
  4f22b0:	cmp	x2, #0x1
  4f22b4:	b.le	4f22f0 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xbc>
  4f22b8:	mov	x1, x21
  4f22bc:	bl	48a080 <memmove@plt>
  4f22c0:	ldp	x19, x20, [sp, #16]
  4f22c4:	mov	x0, #0x0                   	// #0
  4f22c8:	ldp	x21, x22, [sp, #32]
  4f22cc:	ldp	x29, x30, [sp], #48
  4f22d0:	ret
  4f22d4:	bl	48ab80 <__errno_location@plt>
  4f22d8:	mov	x1, x0
  4f22dc:	mov	w2, #0x16                  	// #22
  4f22e0:	mov	x0, #0xffffffffffffffff    	// #-1
  4f22e4:	str	w2, [x1]
  4f22e8:	ldp	x29, x30, [sp], #48
  4f22ec:	ret
  4f22f0:	b.ne	4f22c0 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0x8c>  // b.any
  4f22f4:	ldrb	w0, [x21]
  4f22f8:	strb	w0, [x1, x3]
  4f22fc:	b	4f22c0 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0x8c>
  4f2300:	bl	48ab80 <__errno_location@plt>
  4f2304:	mov	w1, #0x7                   	// #7
  4f2308:	str	w1, [x0]
  4f230c:	ldp	x19, x20, [sp, #16]
  4f2310:	mov	x0, #0xffffffffffffffff    	// #-1
  4f2314:	ldp	x21, x22, [sp, #32]
  4f2318:	b	4f22cc <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0x98>
  4f231c:	bl	48ab80 <__errno_location@plt>
  4f2320:	mov	w1, #0x2                   	// #2
  4f2324:	str	w1, [x0]
  4f2328:	b	4f230c <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xd8>
  4f232c:	bl	48ab80 <__errno_location@plt>
  4f2330:	mov	w1, #0x11                  	// #17
  4f2334:	str	w1, [x0]
  4f2338:	b	4f230c <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)+0xd8>
