
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/off-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb9167000 <.data>:
    ffffb9167000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffffb9167004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffffb9167008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffffb916700c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffffb9167010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffffb9167014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb9167018:	d10143ff 	sub	sp, sp, #0x50
    ffffb916701c:	910003f5 	mov	x21, sp
    ffffb9167020:	52800068 	mov	w8, #0x3                   	// #3
    ffffb9167024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffb9167028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffb916702c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffb9167030:	aa1f03f3 	mov	x19, xzr
    ffffb9167034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffb9167038:	b93ffea8 	str	w8, [x21, #16380]
    ffffb916703c:	f9400017 	ldr	x23, [x0]
    ffffb9167040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffb9167044:	6f00e400 	movi	v0.2d, #0x0
    ffffb9167048:	8b0902b4 	add	x20, x21, x9
    ffffb916704c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffb9167050:	72a00339 	movk	w25, #0x19, lsl #16
    ffffb9167054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffb9167058:	f91ff3f7 	str	x23, [sp, #16352]
    ffffb916705c:	3ca86aa0 	str	q0, [x21, x8]
    ffffb9167060:	1400000a 	b	0xffffb9167088
    ffffb9167064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb9167068:	91000508 	add	x8, x8, #0x1
    ffffb916706c:	f91ffbe8 	str	x8, [sp, #16368]
    ffffb9167070:	f9400008 	ldr	x8, [x0]
    ffffb9167074:	d1000709 	sub	x9, x24, #0x1
    ffffb9167078:	2a1803ea 	mov	w10, w24
    ffffb916707c:	8b130113 	add	x19, x8, x19
    ffffb9167080:	aa0903f8 	mov	x24, x9
    ffffb9167084:	340001ca 	cbz	w10, 0xffffb91670bc
    ffffb9167088:	9b196af7 	madd	x23, x23, x25, x26
    ffffb916708c:	52800060 	mov	w0, #0x3                   	// #3
    ffffb9167090:	aa1403e1 	mov	x1, x20
    ffffb9167094:	53106ae8 	ubfx	w8, w23, #16, #11
    ffffb9167098:	b93ffaa8 	str	w8, [x21, #16376]
    ffffb916709c:	9400001b 	bl	0xffffb9167108
    ffffb91670a0:	b5fffe20 	cbnz	x0, 0xffffb9167064
    ffffb91670a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb91670a8:	91000509 	add	x9, x8, #0x1
    ffffb91670ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffb91670b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffb91670b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffffb91670b8:	17ffffef 	b	0xffffb9167074
    ffffb91670bc:	d10012c1 	sub	x1, x22, #0x4
    ffffb91670c0:	52800080 	mov	w0, #0x4                   	// #4
    ffffb91670c4:	94000011 	bl	0xffffb9167108
    ffffb91670c8:	b40000e0 	cbz	x0, 0xffffb91670e4
    ffffb91670cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffb91670d0:	f9000c08 	str	x8, [x0, #24]
    ffffb91670d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffb91670d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffffb91670dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffb91670e0:	f9000008 	str	x8, [x0]
    ffffb91670e4:	aa1303e0 	mov	x0, x19
    ffffb91670e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb91670ec:	910143ff 	add	sp, sp, #0x50
    ffffb91670f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffffb91670f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffffb91670f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffffb91670fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffffb9167100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffffb9167104:	d65f03c0 	ret
    ffffb9167108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb916710c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb9167110:	f2b6c870 	movk	x16, #0xb643, lsl #16
    ffffb9167114:	f2865a10 	movk	x16, #0x32d0
    ffffb9167118:	d61f0200 	br	x16
	...
