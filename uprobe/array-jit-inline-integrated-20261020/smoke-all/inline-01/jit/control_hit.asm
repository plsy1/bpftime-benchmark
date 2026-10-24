
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/inline-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffb7f34000 <.data>:
    ffffb7f34000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb7f34004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb7f34008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb7f3400c:	d10143ff 	sub	sp, sp, #0x50
    ffffb7f34010:	f9400014 	ldr	x20, [x0]
    ffffb7f34014:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb7f34018:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb7f3401c:	aa1f03f3 	mov	x19, xzr
    ffffb7f34020:	910003e8 	mov	x8, sp
    ffffb7f34024:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffb7f34028:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb7f3402c:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb7f34030:	aa1403ec 	mov	x12, x20
    ffffb7f34034:	b93ffd1f 	str	wzr, [x8, #16380]
    ffffb7f34038:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffb7f3403c:	b1000529 	adds	x9, x9, #0x1
    ffffb7f34040:	d350658d 	ubfx	x13, x12, #16, #10
    ffffb7f34044:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffb7f34048:	8b0d026d 	add	x13, x19, x13
    ffffb7f3404c:	910015b3 	add	x19, x13, #0x5
    ffffb7f34050:	54ffff43 	b.cc	0xffffb7f34038  // b.lo, b.ul, b.last
    ffffb7f34054:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb7f34058:	71000d1f 	cmp	w8, #0x3
    ffffb7f3405c:	540000c8 	b.hi	0xffffb7f34074  // b.pmore
    ffffb7f34060:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb7f34064:	f2b629a9 	movk	x9, #0xb14d, lsl #16
    ffffb7f34068:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb7f3406c:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb7f34070:	14000008 	b	0xffffb7f34090
    ffffb7f34074:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb7f34078:	910003e9 	mov	x9, sp
    ffffb7f3407c:	8b080121 	add	x1, x9, x8
    ffffb7f34080:	52800080 	mov	w0, #0x4                   	// #4
    ffffb7f34084:	aa1303e2 	mov	x2, x19
    ffffb7f34088:	9400000b 	bl	0xffffb7f340b4
    ffffb7f3408c:	b4000080 	cbz	x0, 0xffffb7f3409c
    ffffb7f34090:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffb7f34094:	a9004c14 	stp	x20, x19, [x0]
    ffffb7f34098:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffb7f3409c:	aa1303e0 	mov	x0, x19
    ffffb7f340a0:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb7f340a4:	910143ff 	add	sp, sp, #0x50
    ffffb7f340a8:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb7f340ac:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb7f340b0:	d65f03c0 	ret
    ffffb7f340b4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb7f340b8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb7f340bc:	f2b6a390 	movk	x16, #0xb51c, lsl #16
    ffffb7f340c0:	f291f690 	movk	x16, #0x8fb4
    ffffb7f340c4:	d61f0200 	br	x16
	...
