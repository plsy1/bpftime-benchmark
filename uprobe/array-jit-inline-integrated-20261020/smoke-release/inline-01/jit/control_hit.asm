
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/inline-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffff8f4dd000 <.data>:
    ffff8f4dd000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffff8f4dd004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffff8f4dd008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff8f4dd00c:	d10143ff 	sub	sp, sp, #0x50
    ffff8f4dd010:	f9400014 	ldr	x20, [x0]
    ffff8f4dd014:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffff8f4dd018:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffff8f4dd01c:	aa1f03f3 	mov	x19, xzr
    ffff8f4dd020:	910003e8 	mov	x8, sp
    ffff8f4dd024:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffff8f4dd028:	72a0032a 	movk	w10, #0x19, lsl #16
    ffff8f4dd02c:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffff8f4dd030:	aa1403ec 	mov	x12, x20
    ffff8f4dd034:	b93ffd1f 	str	wzr, [x8, #16380]
    ffff8f4dd038:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffff8f4dd03c:	b1000529 	adds	x9, x9, #0x1
    ffff8f4dd040:	d350658d 	ubfx	x13, x12, #16, #10
    ffff8f4dd044:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffff8f4dd048:	8b0d026d 	add	x13, x19, x13
    ffff8f4dd04c:	910015b3 	add	x19, x13, #0x5
    ffff8f4dd050:	54ffff43 	b.cc	0xffff8f4dd038  // b.lo, b.ul, b.last
    ffff8f4dd054:	b97ffd08 	ldr	w8, [x8, #16380]
    ffff8f4dd058:	71000d1f 	cmp	w8, #0x3
    ffff8f4dd05c:	540000c8 	b.hi	0xffff8f4dd074  // b.pmore
    ffff8f4dd060:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffff8f4dd064:	f2b114c9 	movk	x9, #0x88a6, lsl #16
    ffff8f4dd068:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffff8f4dd06c:	8b081520 	add	x0, x9, x8, lsl #5
    ffff8f4dd070:	14000008 	b	0xffff8f4dd090
    ffff8f4dd074:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffff8f4dd078:	910003e9 	mov	x9, sp
    ffff8f4dd07c:	8b080121 	add	x1, x9, x8
    ffff8f4dd080:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffff8f4dd084:	aa1303e2 	mov	x2, x19
    ffff8f4dd088:	9400000b 	bl	0xffff8f4dd0b4
    ffff8f4dd08c:	b4000080 	cbz	x0, 0xffff8f4dd09c
    ffff8f4dd090:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffff8f4dd094:	a9004c14 	stp	x20, x19, [x0]
    ffff8f4dd098:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffff8f4dd09c:	aa1303e0 	mov	x0, x19
    ffff8f4dd0a0:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff8f4dd0a4:	910143ff 	add	sp, sp, #0x50
    ffff8f4dd0a8:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffff8f4dd0ac:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffff8f4dd0b0:	d65f03c0 	ret
    ffff8f4dd0b4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff8f4dd0b8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff8f4dd0bc:	f2b18eb0 	movk	x16, #0x8c75, lsl #16
    ffff8f4dd0c0:	f291f690 	movk	x16, #0x8fb4
    ffff8f4dd0c4:	d61f0200 	br	x16
	...
