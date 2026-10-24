
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/inline-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffb7f32000 <.data>:
    ffffb7f32000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb7f32004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb7f32008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb7f3200c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb7f32010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb7f32014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb7f32018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb7f3201c:	d10143ff 	sub	sp, sp, #0x50
    ffffb7f32020:	910003f5 	mov	x21, sp
    ffffb7f32024:	52800028 	mov	w8, #0x1                   	// #1
    ffffb7f32028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffb7f3202c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb7f32030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb7f32034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb7f32038:	f2b629bb 	movk	x27, #0xb14d, lsl #16
    ffffb7f3203c:	aa1f03f3 	mov	x19, xzr
    ffffb7f32040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb7f32044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb7f32048:	f9400017 	ldr	x23, [x0]
    ffffb7f3204c:	8b0902b4 	add	x20, x21, x9
    ffffb7f32050:	6f00e400 	movi	v0.2d, #0x0
    ffffb7f32054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb7f32058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb7f3205c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb7f32060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb7f32064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb7f32068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb7f3206c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb7f32070:	1400000b 	b	0xffffb7f3209c
    ffffb7f32074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb7f32078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb7f3207c:	91000508 	add	x8, x8, #0x1
    ffffb7f32080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb7f32084:	f9400008 	ldr	x8, [x0]
    ffffb7f32088:	d1000709 	sub	x9, x24, #0x1
    ffffb7f3208c:	2a1803ea 	mov	w10, w24
    ffffb7f32090:	8b130113 	add	x19, x8, x19
    ffffb7f32094:	aa0903f8 	mov	x24, x9
    ffffb7f32098:	3400020a 	cbz	w10, 0xffffb7f320d8
    ffffb7f3209c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb7f320a0:	531066e8 	ubfx	w8, w23, #16, #10
    ffffb7f320a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb7f320a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb7f320ac:	54fffe49 	b.ls	0xffffb7f32074  // b.plast
    ffffb7f320b0:	52800060 	mov	w0, #0x3                   	// #3
    ffffb7f320b4:	aa1403e1 	mov	x1, x20
    ffffb7f320b8:	94000024 	bl	0xffffb7f32148
    ffffb7f320bc:	b5fffde0 	cbnz	x0, 0xffffb7f32078
    ffffb7f320c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb7f320c4:	91000509 	add	x9, x8, #0x1
    ffffb7f320c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb7f320cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb7f320d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb7f320d4:	17ffffed 	b	0xffffb7f32088
    ffffb7f320d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb7f320dc:	71000d1f 	cmp	w8, #0x3
    ffffb7f320e0:	540000c8 	b.hi	0xffffb7f320f8  // b.pmore
    ffffb7f320e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb7f320e8:	f2b629a9 	movk	x9, #0xb14d, lsl #16
    ffffb7f320ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb7f320f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb7f320f4:	14000005 	b	0xffffb7f32108
    ffffb7f320f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb7f320fc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb7f32100:	94000012 	bl	0xffffb7f32148
    ffffb7f32104:	b40000e0 	cbz	x0, 0xffffb7f32120
    ffffb7f32108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb7f3210c:	f9000c08 	str	x8, [x0, #24]
    ffffb7f32110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb7f32114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb7f32118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb7f3211c:	f9000008 	str	x8, [x0]
    ffffb7f32120:	aa1303e0 	mov	x0, x19
    ffffb7f32124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb7f32128:	910143ff 	add	sp, sp, #0x50
    ffffb7f3212c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb7f32130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb7f32134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb7f32138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb7f3213c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb7f32140:	f84607fd 	ldr	x29, [sp], #96
    ffffb7f32144:	d65f03c0 	ret
    ffffb7f32148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb7f3214c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb7f32150:	f2b6a390 	movk	x16, #0xb51c, lsl #16
    ffffb7f32154:	f291f690 	movk	x16, #0x8fb4
    ffffb7f32158:	d61f0200 	br	x16
	...
