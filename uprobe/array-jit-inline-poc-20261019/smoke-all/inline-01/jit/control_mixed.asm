
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/inline-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb36a0000 <.data>:
    ffffb36a0000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffb36a0004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffb36a0008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffb36a000c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb36a0010:	d10143ff 	sub	sp, sp, #0x50
    ffffb36a0014:	f9400016 	ldr	x22, [x0]
    ffffb36a0018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb36a001c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb36a0020:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffb36a0024:	aa1f03f4 	mov	x20, xzr
    ffffb36a0028:	aa1f03f5 	mov	x21, xzr
    ffffb36a002c:	aa1f03f3 	mov	x19, xzr
    ffffb36a0030:	910003e8 	mov	x8, sp
    ffffb36a0034:	5280004f 	mov	w15, #0x2                   	// #2
    ffffb36a0038:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffb36a003c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb36a0040:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb36a0044:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffb36a0048:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffb36a004c:	aa1603ee 	mov	x14, x22
    ffffb36a0050:	b93ffd0f 	str	w15, [x8, #16380]
    ffffb36a0054:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffb36a0058:	d1000530 	sub	x16, x9, #0x1
    ffffb36a005c:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffb36a0060:	f11001ff 	cmp	x15, #0x400
    ffffb36a0064:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffb36a0068:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffb36a006c:	f10ffdff 	cmp	x15, #0x3ff
    ffffb36a0070:	9100162f 	add	x15, x17, #0x5
    ffffb36a0074:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffb36a0078:	ea4e419f 	tst	x12, x14, lsr #16
    ffffb36a007c:	2a0903f1 	mov	w17, w9
    ffffb36a0080:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffb36a0084:	8b130133 	add	x19, x9, x19
    ffffb36a0088:	aa1003e9 	mov	x9, x16
    ffffb36a008c:	35fffe51 	cbnz	w17, 0xffffb36a0054
    ffffb36a0090:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb36a0094:	71000d1f 	cmp	w8, #0x3
    ffffb36a0098:	540000c8 	b.hi	0xffffb36a00b0  // b.pmore
    ffffb36a009c:	d29d3609 	mov	x9, #0xe9b0                	// #59824
    ffffb36a00a0:	f2b59889 	movk	x9, #0xacc4, lsl #16
    ffffb36a00a4:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb36a00a8:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb36a00ac:	14000009 	b	0xffffb36a00d0
    ffffb36a00b0:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb36a00b4:	910003e9 	mov	x9, sp
    ffffb36a00b8:	8b080121 	add	x1, x9, x8
    ffffb36a00bc:	52800080 	mov	w0, #0x4                   	// #4
    ffffb36a00c0:	aa1303e2 	mov	x2, x19
    ffffb36a00c4:	aa1f03e3 	mov	x3, xzr
    ffffb36a00c8:	9400000b 	bl	0xffffb36a00f4
    ffffb36a00cc:	b4000060 	cbz	x0, 0xffffb36a00d8
    ffffb36a00d0:	a9015414 	stp	x20, x21, [x0, #16]
    ffffb36a00d4:	a9004c16 	stp	x22, x19, [x0]
    ffffb36a00d8:	aa1303e0 	mov	x0, x19
    ffffb36a00dc:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb36a00e0:	910143ff 	add	sp, sp, #0x50
    ffffb36a00e4:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffb36a00e8:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffb36a00ec:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffb36a00f0:	d65f03c0 	ret
    ffffb36a00f4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb36a00f8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb36a00fc:	f2b61270 	movk	x16, #0xb093, lsl #16
    ffffb36a0100:	f2910610 	movk	x16, #0x8830
    ffffb36a0104:	d61f0200 	br	x16
	...
