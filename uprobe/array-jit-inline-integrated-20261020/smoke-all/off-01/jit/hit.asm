
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/off-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffff828a2000 <.data>:
    ffff828a2000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffff828a2004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffff828a2008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffff828a200c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffff828a2010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffff828a2014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff828a2018:	d10143ff 	sub	sp, sp, #0x50
    ffff828a201c:	910003f5 	mov	x21, sp
    ffff828a2020:	52800028 	mov	w8, #0x1                   	// #1
    ffff828a2024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffff828a2028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffff828a202c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffff828a2030:	aa1f03f3 	mov	x19, xzr
    ffff828a2034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffff828a2038:	b93ffea8 	str	w8, [x21, #16380]
    ffff828a203c:	f9400017 	ldr	x23, [x0]
    ffff828a2040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffff828a2044:	6f00e400 	movi	v0.2d, #0x0
    ffff828a2048:	8b0902b4 	add	x20, x21, x9
    ffff828a204c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffff828a2050:	72a00339 	movk	w25, #0x19, lsl #16
    ffff828a2054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffff828a2058:	f91ff3f7 	str	x23, [sp, #16352]
    ffff828a205c:	3ca86aa0 	str	q0, [x21, x8]
    ffff828a2060:	1400000a 	b	0xffff828a2088
    ffff828a2064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff828a2068:	91000508 	add	x8, x8, #0x1
    ffff828a206c:	f91ffbe8 	str	x8, [sp, #16368]
    ffff828a2070:	f9400008 	ldr	x8, [x0]
    ffff828a2074:	d1000709 	sub	x9, x24, #0x1
    ffff828a2078:	2a1803ea 	mov	w10, w24
    ffff828a207c:	8b130113 	add	x19, x8, x19
    ffff828a2080:	aa0903f8 	mov	x24, x9
    ffff828a2084:	340001ca 	cbz	w10, 0xffff828a20bc
    ffff828a2088:	9b196af7 	madd	x23, x23, x25, x26
    ffff828a208c:	52800060 	mov	w0, #0x3                   	// #3
    ffff828a2090:	aa1403e1 	mov	x1, x20
    ffff828a2094:	531066e8 	ubfx	w8, w23, #16, #10
    ffff828a2098:	b93ffaa8 	str	w8, [x21, #16376]
    ffff828a209c:	9400001b 	bl	0xffff828a2108
    ffff828a20a0:	b5fffe20 	cbnz	x0, 0xffff828a2064
    ffff828a20a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff828a20a8:	91000509 	add	x9, x8, #0x1
    ffff828a20ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffff828a20b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffff828a20b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffff828a20b8:	17ffffef 	b	0xffff828a2074
    ffff828a20bc:	d10012c1 	sub	x1, x22, #0x4
    ffff828a20c0:	52800080 	mov	w0, #0x4                   	// #4
    ffff828a20c4:	94000011 	bl	0xffff828a2108
    ffff828a20c8:	b40000e0 	cbz	x0, 0xffff828a20e4
    ffff828a20cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff828a20d0:	f9000c08 	str	x8, [x0, #24]
    ffff828a20d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff828a20d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffff828a20dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffff828a20e0:	f9000008 	str	x8, [x0]
    ffff828a20e4:	aa1303e0 	mov	x0, x19
    ffff828a20e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff828a20ec:	910143ff 	add	sp, sp, #0x50
    ffff828a20f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffff828a20f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffff828a20f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffff828a20fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffff828a2100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffff828a2104:	d65f03c0 	ret
    ffff828a2108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff828a210c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff828a2110:	f2aff690 	movk	x16, #0x7fb4, lsl #16
    ffff828a2114:	f2864c10 	movk	x16, #0x3260
    ffff828a2118:	d61f0200 	br	x16
	...
