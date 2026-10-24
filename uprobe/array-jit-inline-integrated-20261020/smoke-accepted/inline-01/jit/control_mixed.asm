
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/inline-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb5478000 <.data>:
    ffffb5478000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffb5478004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffb5478008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffb547800c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb5478010:	d10143ff 	sub	sp, sp, #0x50
    ffffb5478014:	f9400016 	ldr	x22, [x0]
    ffffb5478018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb547801c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb5478020:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffb5478024:	aa1f03f4 	mov	x20, xzr
    ffffb5478028:	aa1f03f5 	mov	x21, xzr
    ffffb547802c:	aa1f03f3 	mov	x19, xzr
    ffffb5478030:	910003e8 	mov	x8, sp
    ffffb5478034:	5280004f 	mov	w15, #0x2                   	// #2
    ffffb5478038:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffb547803c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb5478040:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb5478044:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffb5478048:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffb547804c:	aa1603ee 	mov	x14, x22
    ffffb5478050:	b93ffd0f 	str	w15, [x8, #16380]
    ffffb5478054:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffb5478058:	d1000530 	sub	x16, x9, #0x1
    ffffb547805c:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffb5478060:	f11001ff 	cmp	x15, #0x400
    ffffb5478064:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffb5478068:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffb547806c:	f10ffdff 	cmp	x15, #0x3ff
    ffffb5478070:	9100162f 	add	x15, x17, #0x5
    ffffb5478074:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffb5478078:	ea4e419f 	tst	x12, x14, lsr #16
    ffffb547807c:	2a0903f1 	mov	w17, w9
    ffffb5478080:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffb5478084:	8b130133 	add	x19, x9, x19
    ffffb5478088:	aa1003e9 	mov	x9, x16
    ffffb547808c:	35fffe51 	cbnz	w17, 0xffffb5478054
    ffffb5478090:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb5478094:	71000d1f 	cmp	w8, #0x3
    ffffb5478098:	540000c8 	b.hi	0xffffb54780b0  // b.pmore
    ffffb547809c:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb54780a0:	f2b5d3e9 	movk	x9, #0xae9f, lsl #16
    ffffb54780a4:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb54780a8:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb54780ac:	14000009 	b	0xffffb54780d0
    ffffb54780b0:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb54780b4:	910003e9 	mov	x9, sp
    ffffb54780b8:	8b080121 	add	x1, x9, x8
    ffffb54780bc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb54780c0:	aa1303e2 	mov	x2, x19
    ffffb54780c4:	aa1f03e3 	mov	x3, xzr
    ffffb54780c8:	9400000b 	bl	0xffffb54780f4
    ffffb54780cc:	b4000060 	cbz	x0, 0xffffb54780d8
    ffffb54780d0:	a9015414 	stp	x20, x21, [x0, #16]
    ffffb54780d4:	a9004c16 	stp	x22, x19, [x0]
    ffffb54780d8:	aa1303e0 	mov	x0, x19
    ffffb54780dc:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb54780e0:	910143ff 	add	sp, sp, #0x50
    ffffb54780e4:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffb54780e8:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffb54780ec:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffb54780f0:	d65f03c0 	ret
    ffffb54780f4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb54780f8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb54780fc:	f2b64dd0 	movk	x16, #0xb26e, lsl #16
    ffffb5478100:	f293fc90 	movk	x16, #0x9fe4
    ffffb5478104:	d61f0200 	br	x16
	...
