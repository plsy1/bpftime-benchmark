
/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb/baseline/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .text:

00000000004e8ec0 <bpftime_percpu_array_map_update_elem_helper>:
  4e8ec0:	stp	x29, x30, [sp, #-16]!
  4e8ec4:	adrp	x4, 2e38000 <trace_init_lock+0x28>
  4e8ec8:	mov	x29, sp
  4e8ecc:	ldr	x4, [x4, #2184]
  4e8ed0:	cmp	x4, #0x0
  4e8ed4:	ccmp	w0, #0x0, #0x1, ne	// ne = any
  4e8ed8:	b.lt	4e8f30 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.tstop
  4e8edc:	ldr	x5, [x4, #16]
  4e8ee0:	cmp	x5, w0, sxtw
  4e8ee4:	b.ls	4e8f30 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.plast
  4e8ee8:	ldr	x5, [x4, #8]
  4e8eec:	mov	w6, #0xa8                  	// #168
  4e8ef0:	add	x4, x4, #0x8
  4e8ef4:	cmp	x5, #0x1
  4e8ef8:	add	x4, x4, x5
  4e8efc:	smull	x0, w0, w6
  4e8f00:	csetm	x5, ne	// ne = any
  4e8f04:	and	x4, x5, x4
  4e8f08:	adds	x0, x4, x0
  4e8f0c:	b.eq	4e8f30 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.none
  4e8f10:	ldrb	w4, [x0, #160]
  4e8f14:	cmp	w4, #0x1
  4e8f18:	b.ne	4e8f30 <bpftime_percpu_array_map_update_elem_helper+0x70>  // b.any
  4e8f1c:	ldr	w4, [x0, #72]
  4e8f20:	cmp	w4, #0x6
  4e8f24:	b.eq	4e8f48 <bpftime_percpu_array_map_update_elem_helper+0x88>  // b.none
  4e8f28:	ldp	x29, x30, [sp], #16
  4e8f2c:	b	526ac0 <bpftime::bpf_map_handler::map_update_elem(void const*, void const*, unsigned long, bool) const [clone .constprop.0]>
  4e8f30:	bl	48ab80 <__errno_location@plt>
  4e8f34:	mov	w1, #0x2                   	// #2
  4e8f38:	str	w1, [x0]
  4e8f3c:	mov	x0, #0xffffffffffffffff    	// #-1
  4e8f40:	ldp	x29, x30, [sp], #16
  4e8f44:	ret
  4e8f48:	ldp	x29, x30, [sp], #16
  4e8f4c:	add	x0, x0, #0x78
  4e8f50:	ldr	x4, [x0]
  4e8f54:	cmp	x4, #0x1
  4e8f58:	add	x0, x0, x4
  4e8f5c:	csetm	x4, ne	// ne = any
  4e8f60:	and	x0, x4, x0
  4e8f64:	b	4f2234 <bpftime::per_cpu_array_map_impl::elem_update(void const*, void const*, unsigned long)>
