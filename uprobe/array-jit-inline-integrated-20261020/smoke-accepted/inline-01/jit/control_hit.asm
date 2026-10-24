
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/inline-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffb547c000 <.data>:
    ffffb547c000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb547c004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb547c008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb547c00c:	d10143ff 	sub	sp, sp, #0x50
    ffffb547c010:	f9400014 	ldr	x20, [x0]
    ffffb547c014:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb547c018:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb547c01c:	aa1f03f3 	mov	x19, xzr
    ffffb547c020:	910003e8 	mov	x8, sp
    ffffb547c024:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffb547c028:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb547c02c:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb547c030:	aa1403ec 	mov	x12, x20
    ffffb547c034:	b93ffd1f 	str	wzr, [x8, #16380]
    ffffb547c038:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffb547c03c:	b1000529 	adds	x9, x9, #0x1
    ffffb547c040:	d350658d 	ubfx	x13, x12, #16, #10
    ffffb547c044:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffb547c048:	8b0d026d 	add	x13, x19, x13
    ffffb547c04c:	910015b3 	add	x19, x13, #0x5
    ffffb547c050:	54ffff43 	b.cc	0xffffb547c038  // b.lo, b.ul, b.last
    ffffb547c054:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb547c058:	71000d1f 	cmp	w8, #0x3
    ffffb547c05c:	540000c8 	b.hi	0xffffb547c074  // b.pmore
    ffffb547c060:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb547c064:	f2b5d3e9 	movk	x9, #0xae9f, lsl #16
    ffffb547c068:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb547c06c:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb547c070:	14000008 	b	0xffffb547c090
    ffffb547c074:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb547c078:	910003e9 	mov	x9, sp
    ffffb547c07c:	8b080121 	add	x1, x9, x8
    ffffb547c080:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb547c084:	aa1303e2 	mov	x2, x19
    ffffb547c088:	9400000b 	bl	0xffffb547c0b4
    ffffb547c08c:	b4000080 	cbz	x0, 0xffffb547c09c
    ffffb547c090:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffb547c094:	a9004c14 	stp	x20, x19, [x0]
    ffffb547c098:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffb547c09c:	aa1303e0 	mov	x0, x19
    ffffb547c0a0:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb547c0a4:	910143ff 	add	sp, sp, #0x50
    ffffb547c0a8:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb547c0ac:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb547c0b0:	d65f03c0 	ret
    ffffb547c0b4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb547c0b8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb547c0bc:	f2b64dd0 	movk	x16, #0xb26e, lsl #16
    ffffb547c0c0:	f293fc90 	movk	x16, #0x9fe4
    ffffb547c0c4:	d61f0200 	br	x16
	...
