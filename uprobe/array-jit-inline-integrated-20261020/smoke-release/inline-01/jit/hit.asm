
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/inline-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffff8f4db000 <.data>:
    ffff8f4db000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffff8f4db004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffff8f4db008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffff8f4db00c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffff8f4db010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffff8f4db014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffff8f4db018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff8f4db01c:	d10143ff 	sub	sp, sp, #0x50
    ffff8f4db020:	910003f5 	mov	x21, sp
    ffff8f4db024:	52800028 	mov	w8, #0x1                   	// #1
    ffff8f4db028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffff8f4db02c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffff8f4db030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffff8f4db034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffff8f4db038:	f2b114db 	movk	x27, #0x88a6, lsl #16
    ffff8f4db03c:	aa1f03f3 	mov	x19, xzr
    ffff8f4db040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffff8f4db044:	b93ffea8 	str	w8, [x21, #16380]
    ffff8f4db048:	f9400017 	ldr	x23, [x0]
    ffff8f4db04c:	8b0902b4 	add	x20, x21, x9
    ffff8f4db050:	6f00e400 	movi	v0.2d, #0x0
    ffff8f4db054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffff8f4db058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffff8f4db05c:	72a00339 	movk	w25, #0x19, lsl #16
    ffff8f4db060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffff8f4db064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffff8f4db068:	3ca86aa0 	str	q0, [x21, x8]
    ffff8f4db06c:	f91ff3f7 	str	x23, [sp, #16352]
    ffff8f4db070:	1400000b 	b	0xffff8f4db09c
    ffff8f4db074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffff8f4db078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff8f4db07c:	91000508 	add	x8, x8, #0x1
    ffff8f4db080:	f91ffbe8 	str	x8, [sp, #16368]
    ffff8f4db084:	f9400008 	ldr	x8, [x0]
    ffff8f4db088:	d1000709 	sub	x9, x24, #0x1
    ffff8f4db08c:	2a1803ea 	mov	w10, w24
    ffff8f4db090:	8b130113 	add	x19, x8, x19
    ffff8f4db094:	aa0903f8 	mov	x24, x9
    ffff8f4db098:	3400020a 	cbz	w10, 0xffff8f4db0d8
    ffff8f4db09c:	9b196af7 	madd	x23, x23, x25, x26
    ffff8f4db0a0:	531066e8 	ubfx	w8, w23, #16, #10
    ffff8f4db0a4:	710ffd1f 	cmp	w8, #0x3ff
    ffff8f4db0a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffff8f4db0ac:	54fffe49 	b.ls	0xffff8f4db074  // b.plast
    ffff8f4db0b0:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffff8f4db0b4:	aa1403e1 	mov	x1, x20
    ffff8f4db0b8:	94000024 	bl	0xffff8f4db148
    ffff8f4db0bc:	b5fffde0 	cbnz	x0, 0xffff8f4db078
    ffff8f4db0c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff8f4db0c4:	91000509 	add	x9, x8, #0x1
    ffff8f4db0c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffff8f4db0cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffff8f4db0d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffff8f4db0d4:	17ffffed 	b	0xffff8f4db088
    ffff8f4db0d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffff8f4db0dc:	71000d1f 	cmp	w8, #0x3
    ffff8f4db0e0:	540000c8 	b.hi	0xffff8f4db0f8  // b.pmore
    ffff8f4db0e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffff8f4db0e8:	f2b114c9 	movk	x9, #0x88a6, lsl #16
    ffff8f4db0ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffff8f4db0f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffff8f4db0f4:	14000005 	b	0xffff8f4db108
    ffff8f4db0f8:	d10012c1 	sub	x1, x22, #0x4
    ffff8f4db0fc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffff8f4db100:	94000012 	bl	0xffff8f4db148
    ffff8f4db104:	b40000e0 	cbz	x0, 0xffff8f4db120
    ffff8f4db108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffff8f4db10c:	f9000c08 	str	x8, [x0, #24]
    ffff8f4db110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffff8f4db114:	a900a013 	stp	x19, x8, [x0, #8]
    ffff8f4db118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffff8f4db11c:	f9000008 	str	x8, [x0]
    ffff8f4db120:	aa1303e0 	mov	x0, x19
    ffff8f4db124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff8f4db128:	910143ff 	add	sp, sp, #0x50
    ffff8f4db12c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffff8f4db130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffff8f4db134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffff8f4db138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffff8f4db13c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffff8f4db140:	f84607fd 	ldr	x29, [sp], #96
    ffff8f4db144:	d65f03c0 	ret
    ffff8f4db148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff8f4db14c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff8f4db150:	f2b18eb0 	movk	x16, #0x8c75, lsl #16
    ffff8f4db154:	f291f690 	movk	x16, #0x8fb4
    ffff8f4db158:	d61f0200 	br	x16
	...
