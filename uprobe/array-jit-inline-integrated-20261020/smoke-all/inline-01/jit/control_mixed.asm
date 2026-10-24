
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/inline-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb7f30000 <.data>:
    ffffb7f30000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffb7f30004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffb7f30008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffb7f3000c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb7f30010:	d10143ff 	sub	sp, sp, #0x50
    ffffb7f30014:	f9400016 	ldr	x22, [x0]
    ffffb7f30018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb7f3001c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb7f30020:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffb7f30024:	aa1f03f4 	mov	x20, xzr
    ffffb7f30028:	aa1f03f5 	mov	x21, xzr
    ffffb7f3002c:	aa1f03f3 	mov	x19, xzr
    ffffb7f30030:	910003e8 	mov	x8, sp
    ffffb7f30034:	5280004f 	mov	w15, #0x2                   	// #2
    ffffb7f30038:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffb7f3003c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb7f30040:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb7f30044:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffb7f30048:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffb7f3004c:	aa1603ee 	mov	x14, x22
    ffffb7f30050:	b93ffd0f 	str	w15, [x8, #16380]
    ffffb7f30054:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffb7f30058:	d1000530 	sub	x16, x9, #0x1
    ffffb7f3005c:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffb7f30060:	f11001ff 	cmp	x15, #0x400
    ffffb7f30064:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffb7f30068:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffb7f3006c:	f10ffdff 	cmp	x15, #0x3ff
    ffffb7f30070:	9100162f 	add	x15, x17, #0x5
    ffffb7f30074:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffb7f30078:	ea4e419f 	tst	x12, x14, lsr #16
    ffffb7f3007c:	2a0903f1 	mov	w17, w9
    ffffb7f30080:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffb7f30084:	8b130133 	add	x19, x9, x19
    ffffb7f30088:	aa1003e9 	mov	x9, x16
    ffffb7f3008c:	35fffe51 	cbnz	w17, 0xffffb7f30054
    ffffb7f30090:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb7f30094:	71000d1f 	cmp	w8, #0x3
    ffffb7f30098:	540000c8 	b.hi	0xffffb7f300b0  // b.pmore
    ffffb7f3009c:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffb7f300a0:	f2b629a9 	movk	x9, #0xb14d, lsl #16
    ffffb7f300a4:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb7f300a8:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb7f300ac:	14000009 	b	0xffffb7f300d0
    ffffb7f300b0:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb7f300b4:	910003e9 	mov	x9, sp
    ffffb7f300b8:	8b080121 	add	x1, x9, x8
    ffffb7f300bc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb7f300c0:	aa1303e2 	mov	x2, x19
    ffffb7f300c4:	aa1f03e3 	mov	x3, xzr
    ffffb7f300c8:	9400000b 	bl	0xffffb7f300f4
    ffffb7f300cc:	b4000060 	cbz	x0, 0xffffb7f300d8
    ffffb7f300d0:	a9015414 	stp	x20, x21, [x0, #16]
    ffffb7f300d4:	a9004c16 	stp	x22, x19, [x0]
    ffffb7f300d8:	aa1303e0 	mov	x0, x19
    ffffb7f300dc:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb7f300e0:	910143ff 	add	sp, sp, #0x50
    ffffb7f300e4:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffb7f300e8:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffb7f300ec:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffb7f300f0:	d65f03c0 	ret
    ffffb7f300f4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb7f300f8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb7f300fc:	f2b6a390 	movk	x16, #0xb51c, lsl #16
    ffffb7f30100:	f291f690 	movk	x16, #0x8fb4
    ffffb7f30104:	d61f0200 	br	x16
	...
