
/home/jetson/src/results/uprobe/array-jit-default-20261021/artifacts/runtime/agent/libbpftime-agent.so:     file format elf64-littleaarch64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .text:

00000000004e9fe0 <bpftime_map_delete_elem_helper>:
  4e9fe0:	stp	x29, x30, [sp, #-48]!
  4e9fe4:	cmp	w0, #0x0
  4e9fe8:	mov	x29, sp
  4e9fec:	stp	x19, x20, [sp, #16]
  4e9ff0:	adrp	x20, 2e58000 <_frida__dwarf_failsafe_error+0x8>
  4e9ff4:	add	x20, x20, #0x870
  4e9ff8:	mov	x19, x0
  4e9ffc:	ldr	x0, [x20, #72]
  4ea000:	ccmp	x0, #0x0, #0x4, ge	// ge = tcont
  4ea004:	b.ne	4ea024 <bpftime_map_delete_elem_helper+0x44>  // b.any
  4ea008:	bl	48c090 <__errno_location@plt>
  4ea00c:	ldp	x19, x20, [sp, #16]
  4ea010:	mov	w1, #0x2                   	// #2
  4ea014:	str	w1, [x0]
  4ea018:	mov	x0, #0xffffffffffffffff    	// #-1
  4ea01c:	ldp	x29, x30, [sp], #48
  4ea020:	ret
  4ea024:	str	x21, [sp, #32]
  4ea028:	mov	x21, x1
  4ea02c:	ldr	x1, [x0, #16]
  4ea030:	cmp	x1, w19, sxtw
  4ea034:	b.ls	4ea078 <bpftime_map_delete_elem_helper+0x98>  // b.plast
  4ea038:	mov	w1, w19
  4ea03c:	bl	4dca20 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4ea040:	ldrb	w0, [x0, #160]
  4ea044:	cmp	w0, #0x1
  4ea048:	b.ne	4ea078 <bpftime_map_delete_elem_helper+0x98>  // b.any
  4ea04c:	ldr	x0, [x20, #72]
  4ea050:	mov	w1, w19
  4ea054:	bl	4dca20 <_ZNK7bpftime15handler_manager11get_handlerEi>
  4ea058:	ldrb	w1, [x0, #160]
  4ea05c:	cmp	w1, #0x1
  4ea060:	b.ne	4ea098 <bpftime_map_delete_elem_helper+0xb8>  // b.any
  4ea064:	ldp	x19, x20, [sp, #16]
  4ea068:	mov	x1, x21
  4ea06c:	ldr	x21, [sp, #32]
  4ea070:	ldp	x29, x30, [sp], #48
  4ea074:	b	52f1a0 <_ZNK7bpftime15bpf_map_handler15map_delete_elemEPKvb.constprop.0>
  4ea078:	bl	48c090 <__errno_location@plt>
  4ea07c:	ldp	x19, x20, [sp, #16]
  4ea080:	mov	w1, #0x2                   	// #2
  4ea084:	ldr	x21, [sp, #32]
  4ea088:	str	w1, [x0]
  4ea08c:	mov	x0, #0xffffffffffffffff    	// #-1
  4ea090:	ldp	x29, x30, [sp], #48
  4ea094:	ret
  4ea098:	cmp	w1, #0xff
  4ea09c:	cset	w0, eq	// eq = none
  4ea0a0:	bl	48da54 <_ZSt26__throw_bad_variant_accessb>

Disassembly of section .fini:
