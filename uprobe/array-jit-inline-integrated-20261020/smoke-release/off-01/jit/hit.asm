
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/off-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffb0051000 <.data>:
    ffffb0051000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffffb0051004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffffb0051008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffffb005100c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffffb0051010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffffb0051014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb0051018:	d10143ff 	sub	sp, sp, #0x50
    ffffb005101c:	910003f5 	mov	x21, sp
    ffffb0051020:	52800028 	mov	w8, #0x1                   	// #1
    ffffb0051024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb0051028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb005102c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb0051030:	aa1f03f3 	mov	x19, xzr
    ffffb0051034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb0051038:	b93ffea8 	str	w8, [x21, #16380]
    ffffb005103c:	f9400017 	ldr	x23, [x0]
    ffffb0051040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb0051044:	6f00e400 	movi	v0.2d, #0x0
    ffffb0051048:	8b0902b4 	add	x20, x21, x9
    ffffb005104c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb0051050:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb0051054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb0051058:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb005105c:	3ca86aa0 	str	q0, [x21, x8]
    ffffb0051060:	1400000a 	b	0xffffb0051088
    ffffb0051064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb0051068:	91000508 	add	x8, x8, #0x1
    ffffb005106c:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb0051070:	f9400008 	ldr	x8, [x0]
    ffffb0051074:	d1000709 	sub	x9, x24, #0x1
    ffffb0051078:	2a1803ea 	mov	w10, w24
    ffffb005107c:	8b130113 	add	x19, x8, x19
    ffffb0051080:	aa0903f8 	mov	x24, x9
    ffffb0051084:	340001ca 	cbz	w10, 0xffffb00510bc
    ffffb0051088:	9b196af7 	madd	x23, x23, x25, x26
    ffffb005108c:	52800060 	mov	w0, #0x3                   	// #3
    ffffb0051090:	aa1403e1 	mov	x1, x20
    ffffb0051094:	531066e8 	ubfx	w8, w23, #16, #10
    ffffb0051098:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb005109c:	9400001b 	bl	0xffffb0051108
    ffffb00510a0:	b5fffe20 	cbnz	x0, 0xffffb0051064
    ffffb00510a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb00510a8:	91000509 	add	x9, x8, #0x1
    ffffb00510ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb00510b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb00510b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb00510b8:	17ffffef 	b	0xffffb0051074
    ffffb00510bc:	d10012c1 	sub	x1, x22, #0x4
    ffffb00510c0:	52800080 	mov	w0, #0x4                   	// #4
    ffffb00510c4:	94000011 	bl	0xffffb0051108
    ffffb00510c8:	b40000e0 	cbz	x0, 0xffffb00510e4
    ffffb00510cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb00510d0:	f9000c08 	str	x8, [x0, #24]
    ffffb00510d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb00510d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb00510dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb00510e0:	f9000008 	str	x8, [x0]
    ffffb00510e4:	aa1303e0 	mov	x0, x19
    ffffb00510e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb00510ec:	910143ff 	add	sp, sp, #0x50
    ffffb00510f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffffb00510f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffffb00510f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffffb00510fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffffb0051100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffffb0051104:	d65f03c0 	ret
    ffffb0051108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb005110c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb0051110:	f2b5a5d0 	movk	x16, #0xad2e, lsl #16
    ffffb0051114:	f2865610 	movk	x16, #0x32b0
    ffffb0051118:	d61f0200 	br	x16
	...
