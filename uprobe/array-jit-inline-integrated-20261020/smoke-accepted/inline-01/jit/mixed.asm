
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/inline-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb544f000 <.data>:
    ffffb544f000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb544f004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb544f008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb544f00c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb544f010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb544f014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb544f018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb544f01c:	d10143ff 	sub	sp, sp, #0x50
    ffffb544f020:	910003f5 	mov	x21, sp
    ffffb544f024:	52800068 	mov	w8, #0x3                   	// #3
    ffffb544f028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffb544f02c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb544f030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb544f034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb544f038:	f2b5d3fb 	movk	x27, #0xae9f, lsl #16
    ffffb544f03c:	aa1f03f3 	mov	x19, xzr
    ffffb544f040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb544f044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb544f048:	f9400017 	ldr	x23, [x0]
    ffffb544f04c:	8b0902b4 	add	x20, x21, x9
    ffffb544f050:	6f00e400 	movi	v0.2d, #0x0
    ffffb544f054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb544f058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb544f05c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb544f060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb544f064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb544f068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb544f06c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb544f070:	1400000b 	b	0xffffb544f09c
    ffffb544f074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb544f078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb544f07c:	91000508 	add	x8, x8, #0x1
    ffffb544f080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb544f084:	f9400008 	ldr	x8, [x0]
    ffffb544f088:	d1000709 	sub	x9, x24, #0x1
    ffffb544f08c:	2a1803ea 	mov	w10, w24
    ffffb544f090:	8b130113 	add	x19, x8, x19
    ffffb544f094:	aa0903f8 	mov	x24, x9
    ffffb544f098:	3400020a 	cbz	w10, 0xffffb544f0d8
    ffffb544f09c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb544f0a0:	53106ae8 	ubfx	w8, w23, #16, #11
    ffffb544f0a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb544f0a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb544f0ac:	54fffe49 	b.ls	0xffffb544f074  // b.plast
    ffffb544f0b0:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb544f0b4:	aa1403e1 	mov	x1, x20
    ffffb544f0b8:	94000024 	bl	0xffffb544f148
    ffffb544f0bc:	b5fffde0 	cbnz	x0, 0xffffb544f078
    ffffb544f0c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb544f0c4:	91000509 	add	x9, x8, #0x1
    ffffb544f0c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb544f0cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb544f0d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb544f0d4:	17ffffed 	b	0xffffb544f088
    ffffb544f0d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb544f0dc:	71000d1f 	cmp	w8, #0x3
    ffffb544f0e0:	540000c8 	b.hi	0xffffb544f0f8  // b.pmore
    ffffb544f0e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb544f0e8:	f2b5d3e9 	movk	x9, #0xae9f, lsl #16
    ffffb544f0ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb544f0f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb544f0f4:	14000005 	b	0xffffb544f108
    ffffb544f0f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb544f0fc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb544f100:	94000012 	bl	0xffffb544f148
    ffffb544f104:	b40000e0 	cbz	x0, 0xffffb544f120
    ffffb544f108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb544f10c:	f9000c08 	str	x8, [x0, #24]
    ffffb544f110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb544f114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb544f118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb544f11c:	f9000008 	str	x8, [x0]
    ffffb544f120:	aa1303e0 	mov	x0, x19
    ffffb544f124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb544f128:	910143ff 	add	sp, sp, #0x50
    ffffb544f12c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb544f130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb544f134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb544f138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb544f13c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb544f140:	f84607fd 	ldr	x29, [sp], #96
    ffffb544f144:	d65f03c0 	ret
    ffffb544f148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb544f14c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb544f150:	f2b64dd0 	movk	x16, #0xb26e, lsl #16
    ffffb544f154:	f293fc90 	movk	x16, #0x9fe4
    ffffb544f158:	d61f0200 	br	x16
	...
