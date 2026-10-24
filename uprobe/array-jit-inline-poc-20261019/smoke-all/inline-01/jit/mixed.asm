
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/inline-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb3677000 <.data>:
    ffffb3677000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb3677004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb3677008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb367700c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb3677010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb3677014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb3677018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb367701c:	d10143ff 	sub	sp, sp, #0x50
    ffffb3677020:	910003f5 	mov	x21, sp
    ffffb3677024:	52800068 	mov	w8, #0x3                   	// #3
    ffffb3677028:	d299261b 	mov	x27, #0xc930                	// #51504
    ffffb367702c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb3677030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb3677034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb3677038:	f2b5989b 	movk	x27, #0xacc4, lsl #16
    ffffb367703c:	aa1f03f3 	mov	x19, xzr
    ffffb3677040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb3677044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb3677048:	f9400017 	ldr	x23, [x0]
    ffffb367704c:	8b0902b4 	add	x20, x21, x9
    ffffb3677050:	6f00e400 	movi	v0.2d, #0x0
    ffffb3677054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb3677058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb367705c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb3677060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb3677064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb3677068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb367706c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb3677070:	1400000b 	b	0xffffb367709c
    ffffb3677074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb3677078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb367707c:	91000508 	add	x8, x8, #0x1
    ffffb3677080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb3677084:	f9400008 	ldr	x8, [x0]
    ffffb3677088:	d1000709 	sub	x9, x24, #0x1
    ffffb367708c:	2a1803ea 	mov	w10, w24
    ffffb3677090:	8b130113 	add	x19, x8, x19
    ffffb3677094:	aa0903f8 	mov	x24, x9
    ffffb3677098:	3400020a 	cbz	w10, 0xffffb36770d8
    ffffb367709c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb36770a0:	53106ae8 	ubfx	w8, w23, #16, #11
    ffffb36770a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb36770a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb36770ac:	54fffe49 	b.ls	0xffffb3677074  // b.plast
    ffffb36770b0:	52800060 	mov	w0, #0x3                   	// #3
    ffffb36770b4:	aa1403e1 	mov	x1, x20
    ffffb36770b8:	94000024 	bl	0xffffb3677148
    ffffb36770bc:	b5fffde0 	cbnz	x0, 0xffffb3677078
    ffffb36770c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb36770c4:	91000509 	add	x9, x8, #0x1
    ffffb36770c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb36770cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb36770d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb36770d4:	17ffffed 	b	0xffffb3677088
    ffffb36770d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb36770dc:	71000d1f 	cmp	w8, #0x3
    ffffb36770e0:	540000c8 	b.hi	0xffffb36770f8  // b.pmore
    ffffb36770e4:	d29d3609 	mov	x9, #0xe9b0                	// #59824
    ffffb36770e8:	f2b59889 	movk	x9, #0xacc4, lsl #16
    ffffb36770ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb36770f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb36770f4:	14000005 	b	0xffffb3677108
    ffffb36770f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb36770fc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb3677100:	94000012 	bl	0xffffb3677148
    ffffb3677104:	b40000e0 	cbz	x0, 0xffffb3677120
    ffffb3677108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb367710c:	f9000c08 	str	x8, [x0, #24]
    ffffb3677110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb3677114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb3677118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb367711c:	f9000008 	str	x8, [x0]
    ffffb3677120:	aa1303e0 	mov	x0, x19
    ffffb3677124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb3677128:	910143ff 	add	sp, sp, #0x50
    ffffb367712c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb3677130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb3677134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb3677138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb367713c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb3677140:	f84607fd 	ldr	x29, [sp], #96
    ffffb3677144:	d65f03c0 	ret
    ffffb3677148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb367714c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb3677150:	f2b61270 	movk	x16, #0xb093, lsl #16
    ffffb3677154:	f2910610 	movk	x16, #0x8830
    ffffb3677158:	d61f0200 	br	x16
	...
