
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/off-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffae8ee000 <.data>:
    ffffae8ee000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffae8ee004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffae8ee008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffae8ee00c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffae8ee010:	d10143ff 	sub	sp, sp, #0x50
    ffffae8ee014:	f9400016 	ldr	x22, [x0]
    ffffae8ee018:	910003e8 	mov	x8, sp
    ffffae8ee01c:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffae8ee020:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffae8ee024:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffae8ee028:	aa1f03f4 	mov	x20, xzr
    ffffae8ee02c:	aa1f03f5 	mov	x21, xzr
    ffffae8ee030:	aa1f03f3 	mov	x19, xzr
    ffffae8ee034:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffae8ee038:	5280004f 	mov	w15, #0x2                   	// #2
    ffffae8ee03c:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffae8ee040:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffae8ee044:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffae8ee048:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffae8ee04c:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffae8ee050:	aa1603ee 	mov	x14, x22
    ffffae8ee054:	b93fffef 	str	w15, [sp, #16380]
    ffffae8ee058:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffae8ee05c:	d1000530 	sub	x16, x9, #0x1
    ffffae8ee060:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffae8ee064:	f11001ff 	cmp	x15, #0x400
    ffffae8ee068:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffae8ee06c:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffae8ee070:	f10ffdff 	cmp	x15, #0x3ff
    ffffae8ee074:	9100162f 	add	x15, x17, #0x5
    ffffae8ee078:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffae8ee07c:	ea4e419f 	tst	x12, x14, lsr #16
    ffffae8ee080:	2a0903f1 	mov	w17, w9
    ffffae8ee084:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffae8ee088:	8b130133 	add	x19, x9, x19
    ffffae8ee08c:	aa1003e9 	mov	x9, x16
    ffffae8ee090:	35fffe51 	cbnz	w17, 0xffffae8ee058
    ffffae8ee094:	d1001101 	sub	x1, x8, #0x4
    ffffae8ee098:	52800080 	mov	w0, #0x4                   	// #4
    ffffae8ee09c:	aa1303e2 	mov	x2, x19
    ffffae8ee0a0:	aa1f03e3 	mov	x3, xzr
    ffffae8ee0a4:	9400000b 	bl	0xffffae8ee0d0
    ffffae8ee0a8:	b4000060 	cbz	x0, 0xffffae8ee0b4
    ffffae8ee0ac:	a9015414 	stp	x20, x21, [x0, #16]
    ffffae8ee0b0:	a9004c16 	stp	x22, x19, [x0]
    ffffae8ee0b4:	aa1303e0 	mov	x0, x19
    ffffae8ee0b8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffae8ee0bc:	910143ff 	add	sp, sp, #0x50
    ffffae8ee0c0:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffae8ee0c4:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffae8ee0c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffae8ee0cc:	d65f03c0 	ret
    ffffae8ee0d0:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffae8ee0d4:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffae8ee0d8:	f2b576f0 	movk	x16, #0xabb7, lsl #16
    ffffae8ee0dc:	f2887610 	movk	x16, #0x43b0
    ffffae8ee0e0:	d61f0200 	br	x16
	...
