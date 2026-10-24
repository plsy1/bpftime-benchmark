
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/off-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffbc5f7000 <.data>:
    ffffbc5f7000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffffbc5f7004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffffbc5f7008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffffbc5f700c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffffbc5f7010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffffbc5f7014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbc5f7018:	d10143ff 	sub	sp, sp, #0x50
    ffffbc5f701c:	910003f5 	mov	x21, sp
    ffffbc5f7020:	52800028 	mov	w8, #0x1                   	// #1
    ffffbc5f7024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffbc5f7028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffbc5f702c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffbc5f7030:	aa1f03f3 	mov	x19, xzr
    ffffbc5f7034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffbc5f7038:	b93ffea8 	str	w8, [x21, #16380]
    ffffbc5f703c:	f9400017 	ldr	x23, [x0]
    ffffbc5f7040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffbc5f7044:	6f00e400 	movi	v0.2d, #0x0
    ffffbc5f7048:	8b0902b4 	add	x20, x21, x9
    ffffbc5f704c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffbc5f7050:	72a00339 	movk	w25, #0x19, lsl #16
    ffffbc5f7054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffbc5f7058:	f91ff3f7 	str	x23, [sp, #16352]
    ffffbc5f705c:	3ca86aa0 	str	q0, [x21, x8]
    ffffbc5f7060:	1400000a 	b	0xffffbc5f7088
    ffffbc5f7064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbc5f7068:	91000508 	add	x8, x8, #0x1
    ffffbc5f706c:	f91ffbe8 	str	x8, [sp, #16368]
    ffffbc5f7070:	f9400008 	ldr	x8, [x0]
    ffffbc5f7074:	d1000709 	sub	x9, x24, #0x1
    ffffbc5f7078:	2a1803ea 	mov	w10, w24
    ffffbc5f707c:	8b130113 	add	x19, x8, x19
    ffffbc5f7080:	aa0903f8 	mov	x24, x9
    ffffbc5f7084:	340001ca 	cbz	w10, 0xffffbc5f70bc
    ffffbc5f7088:	9b196af7 	madd	x23, x23, x25, x26
    ffffbc5f708c:	52800060 	mov	w0, #0x3                   	// #3
    ffffbc5f7090:	aa1403e1 	mov	x1, x20
    ffffbc5f7094:	531066e8 	ubfx	w8, w23, #16, #10
    ffffbc5f7098:	b93ffaa8 	str	w8, [x21, #16376]
    ffffbc5f709c:	9400001b 	bl	0xffffbc5f7108
    ffffbc5f70a0:	b5fffe20 	cbnz	x0, 0xffffbc5f7064
    ffffbc5f70a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbc5f70a8:	91000509 	add	x9, x8, #0x1
    ffffbc5f70ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffbc5f70b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffbc5f70b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffffbc5f70b8:	17ffffef 	b	0xffffbc5f7074
    ffffbc5f70bc:	d10012c1 	sub	x1, x22, #0x4
    ffffbc5f70c0:	52800080 	mov	w0, #0x4                   	// #4
    ffffbc5f70c4:	94000011 	bl	0xffffbc5f7108
    ffffbc5f70c8:	b40000e0 	cbz	x0, 0xffffbc5f70e4
    ffffbc5f70cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbc5f70d0:	f9000c08 	str	x8, [x0, #24]
    ffffbc5f70d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbc5f70d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffffbc5f70dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffbc5f70e0:	f9000008 	str	x8, [x0]
    ffffbc5f70e4:	aa1303e0 	mov	x0, x19
    ffffbc5f70e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbc5f70ec:	910143ff 	add	sp, sp, #0x50
    ffffbc5f70f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffffbc5f70f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffffbc5f70f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffffbc5f70fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffffbc5f7100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffffbc5f7104:	d65f03c0 	ret
    ffffbc5f7108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbc5f710c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbc5f7110:	f2b73110 	movk	x16, #0xb988, lsl #16
    ffffbc5f7114:	f2910610 	movk	x16, #0x8830
    ffffbc5f7118:	d61f0200 	br	x16
	...
