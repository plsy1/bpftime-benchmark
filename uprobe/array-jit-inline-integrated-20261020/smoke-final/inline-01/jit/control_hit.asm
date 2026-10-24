
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/inline-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffbbec5000 <.data>:
    ffffbbec5000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffbbec5004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffbbec5008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbbec500c:	d10143ff 	sub	sp, sp, #0x50
    ffffbbec5010:	f9400014 	ldr	x20, [x0]
    ffffbbec5014:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffbbec5018:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffbbec501c:	aa1f03f3 	mov	x19, xzr
    ffffbbec5020:	910003e8 	mov	x8, sp
    ffffbbec5024:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffbbec5028:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffbbec502c:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffbbec5030:	aa1403ec 	mov	x12, x20
    ffffbbec5034:	b93ffd1f 	str	wzr, [x8, #16380]
    ffffbbec5038:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffbbec503c:	b1000529 	adds	x9, x9, #0x1
    ffffbbec5040:	d350658d 	ubfx	x13, x12, #16, #10
    ffffbbec5044:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffbbec5048:	8b0d026d 	add	x13, x19, x13
    ffffbbec504c:	910015b3 	add	x19, x13, #0x5
    ffffbbec5050:	54ffff43 	b.cc	0xffffbbec5038  // b.lo, b.ul, b.last
    ffffbbec5054:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffbbec5058:	71000d1f 	cmp	w8, #0x3
    ffffbbec505c:	540000c8 	b.hi	0xffffbbec5074  // b.pmore
    ffffbbec5060:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffbbec5064:	f2b6a8c9 	movk	x9, #0xb546, lsl #16
    ffffbbec5068:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffbbec506c:	8b081520 	add	x0, x9, x8, lsl #5
    ffffbbec5070:	14000008 	b	0xffffbbec5090
    ffffbbec5074:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffbbec5078:	910003e9 	mov	x9, sp
    ffffbbec507c:	8b080121 	add	x1, x9, x8
    ffffbbec5080:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbec5084:	aa1303e2 	mov	x2, x19
    ffffbbec5088:	9400000b 	bl	0xffffbbec50b4
    ffffbbec508c:	b4000080 	cbz	x0, 0xffffbbec509c
    ffffbbec5090:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffbbec5094:	a9004c14 	stp	x20, x19, [x0]
    ffffbbec5098:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffbbec509c:	aa1303e0 	mov	x0, x19
    ffffbbec50a0:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbbec50a4:	910143ff 	add	sp, sp, #0x50
    ffffbbec50a8:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffbbec50ac:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffbbec50b0:	d65f03c0 	ret
    ffffbbec50b4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbbec50b8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbbec50bc:	f2b722b0 	movk	x16, #0xb915, lsl #16
    ffffbbec50c0:	f291f890 	movk	x16, #0x8fc4
    ffffbbec50c4:	d61f0200 	br	x16
	...
