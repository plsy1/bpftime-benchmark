
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/inline-01/jit/hit.bin:     file format binary


Disassembly of section .data:

0000ffffbbec3000 <.data>:
    ffffbbec3000:	f81a0ffd 	str	x29, [sp, #-96]!
    ffffbbec3004:	a9016ffe 	stp	x30, x27, [sp, #16]
    ffffbbec3008:	a90267fa 	stp	x26, x25, [sp, #32]
    ffffbbec300c:	a9035ff8 	stp	x24, x23, [sp, #48]
    ffffbbec3010:	a90457f6 	stp	x22, x21, [sp, #64]
    ffffbbec3014:	a9054ff4 	stp	x20, x19, [sp, #80]
    ffffbbec3018:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbbec301c:	d10143ff 	sub	sp, sp, #0x50
    ffffbbec3020:	910003f5 	mov	x21, sp
    ffffbbec3024:	52800028 	mov	w8, #0x1                   	// #1
    ffffbbec3028:	d299361b 	mov	x27, #0xc9b0                	// #51632
    ffffbbec302c:	5287ff09 	mov	w9, #0x3ff8                	// #16376
    ffffbbec3030:	528cc1b9 	mov	w25, #0x660d                	// #26125
    ffffbbec3034:	529e6bfa 	mov	w26, #0xf35f                	// #62303
    ffffbbec3038:	f2b6a8db 	movk	x27, #0xb546, lsl #16
    ffffbbec303c:	aa1f03f3 	mov	x19, xzr
    ffffbbec3040:	914012b6 	add	x22, x21, #0x4, lsl #12
    ffffbbec3044:	b93ffea8 	str	w8, [x21, #16380]
    ffffbbec3048:	f9400017 	ldr	x23, [x0]
    ffffbbec304c:	8b0902b4 	add	x20, x21, x9
    ffffbbec3050:	6f00e400 	movi	v0.2d, #0x0
    ffffbbec3054:	5287fd08 	mov	w8, #0x3fe8                	// #16360
    ffffbbec3058:	52807cf8 	mov	w24, #0x3e7                 	// #999
    ffffbbec305c:	72a00339 	movk	w25, #0x19, lsl #16
    ffffbbec3060:	72a78dda 	movk	w26, #0x3c6e, lsl #16
    ffffbbec3064:	f2dffffb 	movk	x27, #0xffff, lsl #32
    ffffbbec3068:	3ca86aa0 	str	q0, [x21, x8]
    ffffbbec306c:	f91ff3f7 	str	x23, [sp, #16352]
    ffffbbec3070:	1400000b 	b	0xffffbbec309c
    ffffbbec3074:	8b080f60 	add	x0, x27, x8, lsl #3
    ffffbbec3078:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbbec307c:	91000508 	add	x8, x8, #0x1
    ffffbbec3080:	f91ffbe8 	str	x8, [sp, #16368]
    ffffbbec3084:	f9400008 	ldr	x8, [x0]
    ffffbbec3088:	d1000709 	sub	x9, x24, #0x1
    ffffbbec308c:	2a1803ea 	mov	w10, w24
    ffffbbec3090:	8b130113 	add	x19, x8, x19
    ffffbbec3094:	aa0903f8 	mov	x24, x9
    ffffbbec3098:	3400020a 	cbz	w10, 0xffffbbec30d8
    ffffbbec309c:	9b196af7 	madd	x23, x23, x25, x26
    ffffbbec30a0:	531066e8 	ubfx	w8, w23, #16, #10
    ffffbbec30a4:	710ffd1f 	cmp	w8, #0x3ff
    ffffbbec30a8:	b93ffaa8 	str	w8, [x21, #16376]
    ffffbbec30ac:	54fffe49 	b.ls	0xffffbbec3074  // b.plast
    ffffbbec30b0:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbec30b4:	aa1403e1 	mov	x1, x20
    ffffbbec30b8:	94000024 	bl	0xffffbbec3148
    ffffbbec30bc:	b5fffde0 	cbnz	x0, 0xffffbbec3078
    ffffbbec30c0:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbbec30c4:	91000509 	add	x9, x8, #0x1
    ffffbbec30c8:	528f3728 	mov	w8, #0x79b9                	// #31161
    ffffbbec30cc:	72b3c6e8 	movk	w8, #0x9e37, lsl #16
    ffffbbec30d0:	f91ff7e9 	str	x9, [sp, #16360]
    ffffbbec30d4:	17ffffed 	b	0xffffbbec3088
    ffffbbec30d8:	b97ffea8 	ldr	w8, [x21, #16380]
    ffffbbec30dc:	71000d1f 	cmp	w8, #0x3
    ffffbbec30e0:	540000c8 	b.hi	0xffffbbec30f8  // b.pmore
    ffffbbec30e4:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffbbec30e8:	f2b6a8c9 	movk	x9, #0xb546, lsl #16
    ffffbbec30ec:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffbbec30f0:	8b081520 	add	x0, x9, x8, lsl #5
    ffffbbec30f4:	14000005 	b	0xffffbbec3108
    ffffbbec30f8:	d10012c1 	sub	x1, x22, #0x4
    ffffbbec30fc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbec3100:	94000012 	bl	0xffffbbec3148
    ffffbbec3104:	b40000e0 	cbz	x0, 0xffffbbec3120
    ffffbbec3108:	f95ff7e8 	ldr	x8, [sp, #16360]
    ffffbbec310c:	f9000c08 	str	x8, [x0, #24]
    ffffbbec3110:	f95ffbe8 	ldr	x8, [sp, #16368]
    ffffbbec3114:	a900a013 	stp	x19, x8, [x0, #8]
    ffffbbec3118:	f95ff3e8 	ldr	x8, [sp, #16352]
    ffffbbec311c:	f9000008 	str	x8, [x0]
    ffffbbec3120:	aa1303e0 	mov	x0, x19
    ffffbbec3124:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbbec3128:	910143ff 	add	sp, sp, #0x50
    ffffbbec312c:	a9454ff4 	ldp	x20, x19, [sp, #80]
    ffffbbec3130:	a94457f6 	ldp	x22, x21, [sp, #64]
    ffffbbec3134:	a9435ff8 	ldp	x24, x23, [sp, #48]
    ffffbbec3138:	a94267fa 	ldp	x26, x25, [sp, #32]
    ffffbbec313c:	a9416ffe 	ldp	x30, x27, [sp, #16]
    ffffbbec3140:	f84607fd 	ldr	x29, [sp], #96
    ffffbbec3144:	d65f03c0 	ret
    ffffbbec3148:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbbec314c:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbbec3150:	f2b722b0 	movk	x16, #0xb915, lsl #16
    ffffbbec3154:	f291f890 	movk	x16, #0x8fc4
    ffffbbec3158:	d61f0200 	br	x16
	...
