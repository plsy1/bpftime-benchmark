
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/off-01/jit/mixed.bin:     file format binary


Disassembly of section .data:

0000ffff82877000 <.data>:
    ffff82877000:	a9bb7bfd 	stp	x29, x30, [sp, #-80]!
    ffff82877004:	a90167fa 	stp	x26, x25, [sp, #16]
    ffff82877008:	a9025ff8 	stp	x24, x23, [sp, #32]
    ffff8287700c:	a90357f6 	stp	x22, x21, [sp, #48]
    ffff82877010:	a9044ff4 	stp	x20, x19, [sp, #64]
    ffff82877014:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff82877018:	d10143ff 	sub	sp, sp, #0x50
    ffff8287701c:	910003f5 	mov	x21, sp
    ffff82877020:	52800068 	mov	w8, #0x3                   	// #3
    ffff82877024:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffff82877028:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffff8287702c:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffff82877030:	aa1f03f3 	mov	x19, xzr
    ffff82877034:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffff82877038:	b93ffea8 	str	w8, [x21, #16380]
    ffff8287703c:	f9400017 	ldr	x23, [x0]
    ffff82877040:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffff82877044:	6f00e400 	movi	v0.2d, #0x0
    ffff82877048:	8b0902b4 	add	x20, x21, x9
    ffff8287704c:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffff82877050:	72a00339 	movk	w25, #0x19, lsl #16
    ffff82877054:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffff82877058:	f91ff3f7 	str	x23, [sp, #16352]
    ffff8287705c:	3ca86aa0 	str	q0, [x21, x8]
    ffff82877060:	1400000a 	b	0xffff82877088
    ffff82877064:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff82877068:	91000508 	add	x8, x8, #0x1
    ffff8287706c:	f91ffbe8 	str	x8, [sp, #16368]
    ffff82877070:	f9400008 	ldr	x8, [x0]
    ffff82877074:	d1000709 	sub	x9, x24, #0x1
    ffff82877078:	2a1803ea 	mov	w10, w24
    ffff8287707c:	8b130113 	add	x19, x8, x19
    ffff82877080:	aa0903f8 	mov	x24, x9
    ffff82877084:	340001ca 	cbz	w10, 0xffff828770bc
    ffff82877088:	9b196af7 	madd	x23, x23, x25, x26
    ffff8287708c:	52800060 	mov	w0, #0x3                   	// #3
    ffff82877090:	aa1403e1 	mov	x1, x20
    ffff82877094:	53106ae8 	ubfx	w8, w23, #16, #11
    ffff82877098:	b93ffaa8 	str	w8, [x21, #16376]
    ffff8287709c:	9400001b 	bl	0xffff82877108
    ffff828770a0:	b5fffe20 	cbnz	x0, 0xffff82877064
    ffff828770a4:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff828770a8:	91000509 	add	x9, x8, #0x1
    ffff828770ac:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffff828770b0:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffff828770b4:	f91ff7e9 	str	x9, [sp, #16360]
    ffff828770b8:	17ffffef 	b	0xffff82877074
    ffff828770bc:	d10012c1 	sub	x1, x22, #0x4
    ffff828770c0:	52800080 	mov	w0, #0x4                   	// #4
    ffff828770c4:	94000011 	bl	0xffff82877108
    ffff828770c8:	b40000e0 	cbz	x0, 0xffff828770e4
    ffff828770cc:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff828770d0:	f9000c08 	str	x8, [x0, #24]
    ffff828770d4:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff828770d8:	a900a013 	stp	x19, x8, [x0, #8]
    ffff828770dc:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffff828770e0:	f9000008 	str	x8, [x0]
    ffff828770e4:	aa1303e0 	mov	x0, x19
    ffff828770e8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff828770ec:	910143ff 	add	sp, sp, #0x50
    ffff828770f0:	a9444ff4 	ldp	x20, x19, [sp, #64]
    ffff828770f4:	a94357f6 	ldp	x22, x21, [sp, #48]
    ffff828770f8:	a9425ff8 	ldp	x24, x23, [sp, #32]
    ffff828770fc:	a94167fa 	ldp	x26, x25, [sp, #16]
    ffff82877100:	a8c57bfd 	ldp	x29, x30, [sp], #80
    ffff82877104:	d65f03c0 	ret
    ffff82877108:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff8287710c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff82877110:	f2aff690 	movk	x16, #0x7fb4, lsl #16
    ffff82877114:	f2864c10 	movk	x16, #0x3260
    ffff82877118:	d61f0200 	br	x16
	...
