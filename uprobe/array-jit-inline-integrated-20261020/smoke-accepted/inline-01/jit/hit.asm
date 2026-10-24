
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/inline-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffb547a000 <.data>:
    ffffb547a000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb547a004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb547a008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb547a00c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb547a010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb547a014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb547a018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb547a01c:	d10143ff 	sub	sp, sp, #0x50
    ffffb547a020:	910003f5 	mov	x21, sp
    ffffb547a024:	52800028 	mov	w8, #0x1                   	// #1
    ffffb547a028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffb547a02c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb547a030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb547a034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb547a038:	f2b5d3fb 	movk	x27, #0xae9f, lsl #16
    ffffb547a03c:	aa1f03f3 	mov	x19, xzr
    ffffb547a040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb547a044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb547a048:	f9400017 	ldr	x23, [x0]
    ffffb547a04c:	8b0902b4 	add	x20, x21, x9
    ffffb547a050:	6f00e400 	movi	v0.2d, #0x0
    ffffb547a054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb547a058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb547a05c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb547a060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb547a064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb547a068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb547a06c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb547a070:	1400000b 	b	0xffffb547a09c
    ffffb547a074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb547a078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb547a07c:	91000508 	add	x8, x8, #0x1
    ffffb547a080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb547a084:	f9400008 	ldr	x8, [x0]
    ffffb547a088:	d1000709 	sub	x9, x24, #0x1
    ffffb547a08c:	2a1803ea 	mov	w10, w24
    ffffb547a090:	8b130113 	add	x19, x8, x19
    ffffb547a094:	aa0903f8 	mov	x24, x9
    ffffb547a098:	3400020a 	cbz	w10, 0xffffb547a0d8
    ffffb547a09c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb547a0a0:	531066e8 	ubfx	w8, w23, #16, #10
    ffffb547a0a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb547a0a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb547a0ac:	54fffe49 	b.ls	0xffffb547a074  // b.plast
    ffffb547a0b0:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb547a0b4:	aa1403e1 	mov	x1, x20
    ffffb547a0b8:	94000024 	bl	0xffffb547a148
    ffffb547a0bc:	b5fffde0 	cbnz	x0, 0xffffb547a078
    ffffb547a0c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb547a0c4:	91000509 	add	x9, x8, #0x1
    ffffb547a0c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb547a0cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb547a0d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb547a0d4:	17ffffed 	b	0xffffb547a088
    ffffb547a0d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb547a0dc:	71000d1f 	cmp	w8, #0x3
    ffffb547a0e0:	540000c8 	b.hi	0xffffb547a0f8  // b.pmore
    ffffb547a0e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb547a0e8:	f2b5d3e9 	movk	x9, #0xae9f, lsl #16
    ffffb547a0ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb547a0f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb547a0f4:	14000005 	b	0xffffb547a108
    ffffb547a0f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb547a0fc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb547a100:	94000012 	bl	0xffffb547a148
    ffffb547a104:	b40000e0 	cbz	x0, 0xffffb547a120
    ffffb547a108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb547a10c:	f9000c08 	str	x8, [x0, #24]
    ffffb547a110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb547a114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb547a118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb547a11c:	f9000008 	str	x8, [x0]
    ffffb547a120:	aa1303e0 	mov	x0, x19
    ffffb547a124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb547a128:	910143ff 	add	sp, sp, #0x50
    ffffb547a12c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb547a130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb547a134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb547a138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb547a13c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb547a140:	f84607fd 	ldr	x29, [sp], #96
    ffffb547a144:	d65f03c0 	ret
    ffffb547a148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb547a14c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb547a150:	f2b64dd0 	movk	x16, #0xb26e, lsl #16
    ffffb547a154:	f293fc90 	movk	x16, #0x9fe4
    ffffb547a158:	d61f0200 	br	x16
	...
