
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/off-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffff828a0000 <.data>:
    ffff828a0000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffff828a0004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffff828a0008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffff828a000c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff828a0010:	d10143ff 	sub	sp, sp, #0x50
    ffff828a0014:	f9400016 	ldr	x22, [x0]
    ffff828a0018:	910003e8 	mov	x8, sp
    ffff828a001c:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffff828a0020:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffff828a0024:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffff828a0028:	aa1f03f4 	mov	x20, xzr
    ffff828a002c:	aa1f03f5 	mov	x21, xzr
    ffff828a0030:	aa1f03f3 	mov	x19, xzr
    ffff828a0034:	91401108 	add	x8, x8, #0x4, lsl #12
    ffff828a0038:	5280004f 	mov	w15, #0x2                   	// #2
    ffff828a003c:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffff828a0040:	72a0032a 	movk	w10, #0x19, lsl #16
    ffff828a0044:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffff828a0048:	5280800c 	mov	w12, #0x400                 	// #1024
    ffff828a004c:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffff828a0050:	aa1603ee 	mov	x14, x22
    ffff828a0054:	b93fffef 	str	w15, [sp, #16380]
    ffff828a0058:	9b0a2dce 	madd	x14, x14, x10, x11
    ffff828a005c:	d1000530 	sub	x16, x9, #0x1
    ffff828a0060:	d35069cf 	ubfx	x15, x14, #16, #11
    ffff828a0064:	f11001ff 	cmp	x15, #0x400
    ffff828a0068:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffff828a006c:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffff828a0070:	f10ffdff 	cmp	x15, #0x3ff
    ffff828a0074:	9100162f 	add	x15, x17, #0x5
    ffff828a0078:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffff828a007c:	ea4e419f 	tst	x12, x14, lsr #16
    ffff828a0080:	2a0903f1 	mov	w17, w9
    ffff828a0084:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffff828a0088:	8b130133 	add	x19, x9, x19
    ffff828a008c:	aa1003e9 	mov	x9, x16
    ffff828a0090:	35fffe51 	cbnz	w17, 0xffff828a0058
    ffff828a0094:	d1001101 	sub	x1, x8, #0x4
    ffff828a0098:	52800080 	mov	w0, #0x4                   	// #4
    ffff828a009c:	aa1303e2 	mov	x2, x19
    ffff828a00a0:	aa1f03e3 	mov	x3, xzr
    ffff828a00a4:	9400000b 	bl	0xffff828a00d0
    ffff828a00a8:	b4000060 	cbz	x0, 0xffff828a00b4
    ffff828a00ac:	a9015414 	stp	x20, x21, [x0, #16]
    ffff828a00b0:	a9004c16 	stp	x22, x19, [x0]
    ffff828a00b4:	aa1303e0 	mov	x0, x19
    ffff828a00b8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff828a00bc:	910143ff 	add	sp, sp, #0x50
    ffff828a00c0:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffff828a00c4:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffff828a00c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffff828a00cc:	d65f03c0 	ret
    ffff828a00d0:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff828a00d4:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff828a00d8:	f2aff690 	movk	x16, #0x7fb4, lsl #16
    ffff828a00dc:	f2864c10 	movk	x16, #0x3260
    ffff828a00e0:	d61f0200 	br	x16
	...
