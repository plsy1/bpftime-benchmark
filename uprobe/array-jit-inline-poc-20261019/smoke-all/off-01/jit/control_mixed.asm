
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/off-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffbc5f5000 <.data>:
    ffffbc5f5000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffbc5f5004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffbc5f5008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffbc5f500c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbc5f5010:	d10143ff 	sub	sp, sp, #0x50
    ffffbc5f5014:	f9400016 	ldr	x22, [x0]
    ffffbc5f5018:	910003e8 	mov	x8, sp
    ffffbc5f501c:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffbc5f5020:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffbc5f5024:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffbc5f5028:	aa1f03f4 	mov	x20, xzr
    ffffbc5f502c:	aa1f03f5 	mov	x21, xzr
    ffffbc5f5030:	aa1f03f3 	mov	x19, xzr
    ffffbc5f5034:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffbc5f5038:	5280004f 	mov	w15, #0x2                   	// #2
    ffffbc5f503c:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffbc5f5040:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffbc5f5044:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffbc5f5048:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffbc5f504c:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffbc5f5050:	aa1603ee 	mov	x14, x22
    ffffbc5f5054:	b93fffef 	str	w15, [sp, #16380]
    ffffbc5f5058:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffbc5f505c:	d1000530 	sub	x16, x9, #0x1
    ffffbc5f5060:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffbc5f5064:	f11001ff 	cmp	x15, #0x400
    ffffbc5f5068:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffbc5f506c:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffbc5f5070:	f10ffdff 	cmp	x15, #0x3ff
    ffffbc5f5074:	9100162f 	add	x15, x17, #0x5
    ffffbc5f5078:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffbc5f507c:	ea4e419f 	tst	x12, x14, lsr #16
    ffffbc5f5080:	2a0903f1 	mov	w17, w9
    ffffbc5f5084:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffbc5f5088:	8b130133 	add	x19, x9, x19
    ffffbc5f508c:	aa1003e9 	mov	x9, x16
    ffffbc5f5090:	35fffe51 	cbnz	w17, 0xffffbc5f5058
    ffffbc5f5094:	d1001101 	sub	x1, x8, #0x4
    ffffbc5f5098:	52800080 	mov	w0, #0x4                   	// #4
    ffffbc5f509c:	aa1303e2 	mov	x2, x19
    ffffbc5f50a0:	aa1f03e3 	mov	x3, xzr
    ffffbc5f50a4:	9400000b 	bl	0xffffbc5f50d0
    ffffbc5f50a8:	b4000060 	cbz	x0, 0xffffbc5f50b4
    ffffbc5f50ac:	a9015414 	stp	x20, x21, [x0, #16]
    ffffbc5f50b0:	a9004c16 	stp	x22, x19, [x0]
    ffffbc5f50b4:	aa1303e0 	mov	x0, x19
    ffffbc5f50b8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbc5f50bc:	910143ff 	add	sp, sp, #0x50
    ffffbc5f50c0:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffbc5f50c4:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffbc5f50c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffbc5f50cc:	d65f03c0 	ret
    ffffbc5f50d0:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbc5f50d4:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbc5f50d8:	f2b73110 	movk	x16, #0xb988, lsl #16
    ffffbc5f50dc:	f2910610 	movk	x16, #0x8830
    ffffbc5f50e0:	d61f0200 	br	x16
	...
