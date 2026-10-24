
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/inline-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb7f07000 <.data>:
    ffffb7f07000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb7f07004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb7f07008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb7f0700c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb7f07010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb7f07014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb7f07018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb7f0701c:	d10143ff 	sub	sp, sp, #0x50
    ffffb7f07020:	910003f5 	mov	x21, sp
    ffffb7f07024:	52800068 	mov	w8, #0x3                   	// #3
    ffffb7f07028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffb7f0702c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb7f07030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb7f07034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb7f07038:	f2b629bb 	movk	x27, #0xb14d, lsl #16
    ffffb7f0703c:	aa1f03f3 	mov	x19, xzr
    ffffb7f07040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb7f07044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb7f07048:	f9400017 	ldr	x23, [x0]
    ffffb7f0704c:	8b0902b4 	add	x20, x21, x9
    ffffb7f07050:	6f00e400 	movi	v0.2d, #0x0
    ffffb7f07054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb7f07058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb7f0705c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb7f07060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb7f07064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb7f07068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb7f0706c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb7f07070:	1400000b 	b	0xffffb7f0709c
    ffffb7f07074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb7f07078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb7f0707c:	91000508 	add	x8, x8, #0x1
    ffffb7f07080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb7f07084:	f9400008 	ldr	x8, [x0]
    ffffb7f07088:	d1000709 	sub	x9, x24, #0x1
    ffffb7f0708c:	2a1803ea 	mov	w10, w24
    ffffb7f07090:	8b130113 	add	x19, x8, x19
    ffffb7f07094:	aa0903f8 	mov	x24, x9
    ffffb7f07098:	3400020a 	cbz	w10, 0xffffb7f070d8
    ffffb7f0709c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb7f070a0:	53106ae8 	ubfx	w8, w23, #16, #11
    ffffb7f070a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb7f070a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb7f070ac:	54fffe49 	b.ls	0xffffb7f07074  // b.plast
    ffffb7f070b0:	52800060 	mov	w0, #0x3                   	// #3
    ffffb7f070b4:	aa1403e1 	mov	x1, x20
    ffffb7f070b8:	94000024 	bl	0xffffb7f07148
    ffffb7f070bc:	b5fffde0 	cbnz	x0, 0xffffb7f07078
    ffffb7f070c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb7f070c4:	91000509 	add	x9, x8, #0x1
    ffffb7f070c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb7f070cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb7f070d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb7f070d4:	17ffffed 	b	0xffffb7f07088
    ffffb7f070d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb7f070dc:	71000d1f 	cmp	w8, #0x3
    ffffb7f070e0:	540000c8 	b.hi	0xffffb7f070f8  // b.pmore
    ffffb7f070e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb7f070e8:	f2b629a9 	movk	x9, #0xb14d, lsl #16
    ffffb7f070ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb7f070f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb7f070f4:	14000005 	b	0xffffb7f07108
    ffffb7f070f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb7f070fc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb7f07100:	94000012 	bl	0xffffb7f07148
    ffffb7f07104:	b40000e0 	cbz	x0, 0xffffb7f07120
    ffffb7f07108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb7f0710c:	f9000c08 	str	x8, [x0, #24]
    ffffb7f07110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb7f07114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb7f07118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb7f0711c:	f9000008 	str	x8, [x0]
    ffffb7f07120:	aa1303e0 	mov	x0, x19
    ffffb7f07124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb7f07128:	910143ff 	add	sp, sp, #0x50
    ffffb7f0712c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb7f07130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb7f07134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb7f07138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb7f0713c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb7f07140:	f84607fd 	ldr	x29, [sp], #96
    ffffb7f07144:	d65f03c0 	ret
    ffffb7f07148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb7f0714c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb7f07150:	f2b6a390 	movk	x16, #0xb51c, lsl #16
    ffffb7f07154:	f291f690 	movk	x16, #0x8fb4
    ffffb7f07158:	d61f0200 	br	x16
	...
