
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/inline-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffffbbe98000 <.data>:
    ffffbbe98000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffbbe98004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffbbe98008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffbbe9800c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffbbe98010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffbbe98014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffbbe98018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbbe9801c:	d10143ff 	sub	sp, sp, #0x50
    ffffbbe98020:	910003f5 	mov	x21, sp
    ffffbbe98024:	52800068 	mov	w8, #0x3                   	// #3
    ffffbbe98028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffbbe9802c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffbbe98030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffbbe98034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffbbe98038:	f2b6a8db 	movk	x27, #0xb546, lsl #16
    ffffbbe9803c:	aa1f03f3 	mov	x19, xzr
    ffffbbe98040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffbbe98044:	b93ffea8 	str	w8, [x21, #16380]
    ffffbbe98048:	f9400017 	ldr	x23, [x0]
    ffffbbe9804c:	8b0902b4 	add	x20, x21, x9
    ffffbbe98050:	6f00e400 	movi	v0.2d, #0x0
    ffffbbe98054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffbbe98058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffbbe9805c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffbbe98060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffbbe98064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffbbe98068:	3ca86aa0 	str	q0, [x21, x8]
    ffffbbe9806c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffbbe98070:	1400000b 	b	0xffffbbe9809c
    ffffbbe98074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffbbe98078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbbe9807c:	91000508 	add	x8, x8, #0x1
    ffffbbe98080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffbbe98084:	f9400008 	ldr	x8, [x0]
    ffffbbe98088:	d1000709 	sub	x9, x24, #0x1
    ffffbbe9808c:	2a1803ea 	mov	w10, w24
    ffffbbe98090:	8b130113 	add	x19, x8, x19
    ffffbbe98094:	aa0903f8 	mov	x24, x9
    ffffbbe98098:	3400020a 	cbz	w10, 0xffffbbe980d8
    ffffbbe9809c:	9b196af7 	madd	x23, x23, x25, x26
    ffffbbe980a0:	53106ae8 	ubfx	w8, w23, #16, #11
    ffffbbe980a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffbbe980a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffbbe980ac:	54fffe49 	b.ls	0xffffbbe98074  // b.plast
    ffffbbe980b0:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbe980b4:	aa1403e1 	mov	x1, x20
    ffffbbe980b8:	94000024 	bl	0xffffbbe98148
    ffffbbe980bc:	b5fffde0 	cbnz	x0, 0xffffbbe98078
    ffffbbe980c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbbe980c4:	91000509 	add	x9, x8, #0x1
    ffffbbe980c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffbbe980cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffbbe980d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffbbe980d4:	17ffffed 	b	0xffffbbe98088
    ffffbbe980d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffbbe980dc:	71000d1f 	cmp	w8, #0x3
    ffffbbe980e0:	540000c8 	b.hi	0xffffbbe980f8  // b.pmore
    ffffbbe980e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffbbe980e8:	f2b6a8c9 	movk	x9, #0xb546, lsl #16
    ffffbbe980ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffbbe980f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffbbe980f4:	14000005 	b	0xffffbbe98108
    ffffbbe980f8:	d10012c1 	sub	x1, x22, #0x4
    ffffbbe980fc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbe98100:	94000012 	bl	0xffffbbe98148
    ffffbbe98104:	b40000e0 	cbz	x0, 0xffffbbe98120
    ffffbbe98108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbbe9810c:	f9000c08 	str	x8, [x0, #24]
    ffffbbe98110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbbe98114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffbbe98118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffbbe9811c:	f9000008 	str	x8, [x0]
    ffffbbe98120:	aa1303e0 	mov	x0, x19
    ffffbbe98124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbbe98128:	910143ff 	add	sp, sp, #0x50
    ffffbbe9812c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffbbe98130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffbbe98134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffbbe98138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffbbe9813c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffbbe98140:	f84607fd 	ldr	x29, [sp], #96
    ffffbbe98144:	d65f03c0 	ret
    ffffbbe98148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbbe9814c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbbe98150:	f2b722b0 	movk	x16, #0xb915, lsl #16
    ffffbbe98154:	f291f890 	movk	x16, #0x8fc4
    ffffbbe98158:	d61f0200 	br	x16
	...
