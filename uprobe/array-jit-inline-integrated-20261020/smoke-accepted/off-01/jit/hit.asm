
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/off-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffae8f0000 <.data>:
    ffffae8f0000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffffae8f0004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffffae8f0008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffffae8f000c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffffae8f0010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffffae8f0014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffae8f0018:	d10143ff 	sub	sp, sp, #0x50
    ffffae8f001c:	910003f5 	mov	x21, sp
    ffffae8f0020:	52800028 	mov	w8, #0x1                   	// #1
    ffffae8f0024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffae8f0028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffae8f002c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffae8f0030:	aa1f03f3 	mov	x19, xzr
    ffffae8f0034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffae8f0038:	b93ffea8 	str	w8, [x21, #16380]
    ffffae8f003c:	f9400017 	ldr	x23, [x0]
    ffffae8f0040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffae8f0044:	6f00e400 	movi	v0.2d, #0x0
    ffffae8f0048:	8b0902b4 	add	x20, x21, x9
    ffffae8f004c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffae8f0050:	72a00339 	movk	w25, #0x19, lsl #16
    ffffae8f0054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffae8f0058:	f91ff3f7 	str	x23, [sp, #16352]
    ffffae8f005c:	3ca86aa0 	str	q0, [x21, x8]
    ffffae8f0060:	1400000a 	b	0xffffae8f0088
    ffffae8f0064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffae8f0068:	91000508 	add	x8, x8, #0x1
    ffffae8f006c:	f91ffbe8 	str	x8, [sp, #16368]
    ffffae8f0070:	f9400008 	ldr	x8, [x0]
    ffffae8f0074:	d1000709 	sub	x9, x24, #0x1
    ffffae8f0078:	2a1803ea 	mov	w10, w24
    ffffae8f007c:	8b130113 	add	x19, x8, x19
    ffffae8f0080:	aa0903f8 	mov	x24, x9
    ffffae8f0084:	340001ca 	cbz	w10, 0xffffae8f00bc
    ffffae8f0088:	9b196af7 	madd	x23, x23, x25, x26
    ffffae8f008c:	52800060 	mov	w0, #0x3                   	// #3
    ffffae8f0090:	aa1403e1 	mov	x1, x20
    ffffae8f0094:	531066e8 	ubfx	w8, w23, #16, #10
    ffffae8f0098:	b93ffaa8 	str	w8, [x21, #16376]
    ffffae8f009c:	9400001b 	bl	0xffffae8f0108
    ffffae8f00a0:	b5fffe20 	cbnz	x0, 0xffffae8f0064
    ffffae8f00a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffae8f00a8:	91000509 	add	x9, x8, #0x1
    ffffae8f00ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffae8f00b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffae8f00b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffffae8f00b8:	17ffffef 	b	0xffffae8f0074
    ffffae8f00bc:	d10012c1 	sub	x1, x22, #0x4
    ffffae8f00c0:	52800080 	mov	w0, #0x4                   	// #4
    ffffae8f00c4:	94000011 	bl	0xffffae8f0108
    ffffae8f00c8:	b40000e0 	cbz	x0, 0xffffae8f00e4
    ffffae8f00cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffae8f00d0:	f9000c08 	str	x8, [x0, #24]
    ffffae8f00d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffae8f00d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffffae8f00dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffae8f00e0:	f9000008 	str	x8, [x0]
    ffffae8f00e4:	aa1303e0 	mov	x0, x19
    ffffae8f00e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffae8f00ec:	910143ff 	add	sp, sp, #0x50
    ffffae8f00f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffffae8f00f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffffae8f00f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffffae8f00fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffffae8f0100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffffae8f0104:	d65f03c0 	ret
    ffffae8f0108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffae8f010c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffae8f0110:	f2b576f0 	movk	x16, #0xabb7, lsl #16
    ffffae8f0114:	f2887610 	movk	x16, #0x43b0
    ffffae8f0118:	d61f0200 	br	x16
	...
