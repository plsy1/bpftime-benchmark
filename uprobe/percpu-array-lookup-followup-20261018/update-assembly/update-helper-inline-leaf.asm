
/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb/inline-leaf/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .text:

00000000004e8f40 <bpftime_percpu_array_map_update_elem_helper>:
  4e8f40:	stp	x29, x30, [sp, #-16]!
  4e8f44:	adrp	x4, 2e38000 <trace_init_lock+0x28>
  4e8f48:	mov	x29, sp
  4e8f4c:	ldr	x4, [x4, #2184]
  4e8f50:	cmp	x4, #0x0
  4e8f54:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e8f58:	b.lt	4e8fb0 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.tstop
  4e8f5c:	ldr	x5, [x4, #16]
  4e8f60:	cmp	x5, w0, sxtw
  4e8f64:	b.ls	4e8fb0 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.plast
  4e8f68:	ldr	x5, [x4, #8]
  4e8f6c:	mov	w6, #0xa8                  	// #168
  4e8f70:	add	x4, x4, #0x8
  4e8f74:	cmp	x5, #0x1
  4e8f78:	add	x4, x4, x5
  4e8f7c:	smull	x0, w0, w6
  4e8f80:	csetm	x5, ne	// ne = any
  4e8f84:	and	x4, x5, x4
  4e8f88:	adds	x0, x4, x0
  4e8f8c:	b.eq	4e8fb0 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.none
  4e8f90:	ldrb	w4, [x0, #160]
  4e8f94:	cmp	w4, #0x1
  4e8f98:	b.ne	4e8fb0 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.any
  4e8f9c:	ldr	w4, [x0, #72]
  4e8fa0:	cmp	w4, #0x6
  4e8fa4:	b.eq	4e8fc8 <bpftime_percpu_array_map_update_elem_helper+0x88>  // b.none
  4e8fa8:	ldp	x29, x30, [sp], #16
  4e8fac:	b	527aa0 <bpftime::bpf_map_handler::map_update_elem(void const*, void const*, unsigned long, bool) const [clone .constprop.0]>
  4e8fb0:	bl	48ab80 <__errno_location@plt>
  4e8fb4:	mov	w1, #0x2                   	// #2
  4e8fb8:	str	w1, [x0]
  4e8fbc:	mov	x0, #0xffffffffffffffff    	// #-1
  4e8fc0:	ldp	x29, x30, [sp], #16
  4e8fc4:	ret
  4e8fc8:	ldp	x29, x30, [sp], #16
  4e8fcc:	add	x0, x0, #0x78
  4e8fd0:	ldr	x4, [x0]
  4e8fd4:	cmp	x4, #0x1
  4e8fd8:	add	x0, x0, x4
  4e8fdc:	csetm	x4, ne	// ne = any
  4e8fe0:	and	x0, x4, x0
  4e8fe4:	b	4f2230 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)>
