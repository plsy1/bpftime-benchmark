
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/inline-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffb36a2000 <.data>:
    ffffb36a2000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffb36a2004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffb36a2008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffb36a200c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffb36a2010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffb36a2014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffb36a2018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb36a201c:	d10143ff 	sub	sp, sp, #0x50
    ffffb36a2020:	910003f5 	mov	x21, sp
    ffffb36a2024:	52800028 	mov	w8, #0x1                   	// #1
    ffffb36a2028:	d299261b 	mov	x27, #0xc930                	// #51504
    ffffb36a202c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb36a2030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb36a2034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb36a2038:	f2b5989b 	movk	x27, #0xacc4, lsl #16
    ffffb36a203c:	aa1f03f3 	mov	x19, xzr
    ffffb36a2040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb36a2044:	b93ffea8 	str	w8, [x21, #16380]
    ffffb36a2048:	f9400017 	ldr	x23, [x0]
    ffffb36a204c:	8b0902b4 	add	x20, x21, x9
    ffffb36a2050:	6f00e400 	movi	v0.2d, #0x0
    ffffb36a2054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb36a2058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb36a205c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb36a2060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb36a2064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffb36a2068:	3ca86aa0 	str	q0, [x21, x8]
    ffffb36a206c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb36a2070:	1400000b 	b	0xffffb36a209c
    ffffb36a2074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffb36a2078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb36a207c:	91000508 	add	x8, x8, #0x1
    ffffb36a2080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb36a2084:	f9400008 	ldr	x8, [x0]
    ffffb36a2088:	d1000709 	sub	x9, x24, #0x1
    ffffb36a208c:	2a1803ea 	mov	w10, w24
    ffffb36a2090:	8b130113 	add	x19, x8, x19
    ffffb36a2094:	aa0903f8 	mov	x24, x9
    ffffb36a2098:	3400020a 	cbz	w10, 0xffffb36a20d8
    ffffb36a209c:	9b196af7 	madd	x23, x23, x25, x26
    ffffb36a20a0:	531066e8 	ubfx	w8, w23, #16, #10
    ffffb36a20a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffb36a20a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb36a20ac:	54fffe49 	b.ls	0xffffb36a2074  // b.plast
    ffffb36a20b0:	52800060 	mov	w0, #0x3                   	// #3
    ffffb36a20b4:	aa1403e1 	mov	x1, x20
    ffffb36a20b8:	94000024 	bl	0xffffb36a2148
    ffffb36a20bc:	b5fffde0 	cbnz	x0, 0xffffb36a2078
    ffffb36a20c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb36a20c4:	91000509 	add	x9, x8, #0x1
    ffffb36a20c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb36a20cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb36a20d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb36a20d4:	17ffffed 	b	0xffffb36a2088
    ffffb36a20d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffb36a20dc:	71000d1f 	cmp	w8, #0x3
    ffffb36a20e0:	540000c8 	b.hi	0xffffb36a20f8  // b.pmore
    ffffb36a20e4:	d29d3609 	mov	x9, #0xe9b0                	// #59824
    ffffb36a20e8:	f2b59889 	movk	x9, #0xacc4, lsl #16
    ffffb36a20ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb36a20f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb36a20f4:	14000005 	b	0xffffb36a2108
    ffffb36a20f8:	d10012c1 	sub	x1, x22, #0x4
    ffffb36a20fc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb36a2100:	94000012 	bl	0xffffb36a2148
    ffffb36a2104:	b40000e0 	cbz	x0, 0xffffb36a2120
    ffffb36a2108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb36a210c:	f9000c08 	str	x8, [x0, #24]
    ffffb36a2110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb36a2114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb36a2118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb36a211c:	f9000008 	str	x8, [x0]
    ffffb36a2120:	aa1303e0 	mov	x0, x19
    ffffb36a2124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb36a2128:	910143ff 	add	sp, sp, #0x50
    ffffb36a212c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffb36a2130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffb36a2134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffb36a2138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffb36a213c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffb36a2140:	f84607fd 	ldr	x29, [sp], #96
    ffffb36a2144:	d65f03c0 	ret
    ffffb36a2148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb36a214c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb36a2150:	f2b61270 	movk	x16, #0xb093, lsl #16
    ffffb36a2154:	f2910610 	movk	x16, #0x8830
    ffffb36a2158:	d61f0200 	br	x16
	...
