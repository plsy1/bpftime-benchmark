
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/off-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb9190000 <.data>:
    ffffb9190000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffb9190004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffb9190008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffb919000c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb9190010:	d10143ff 	sub	sp, sp, #0x50
    ffffb9190014:	f9400016 	ldr	x22, [x0]
    ffffb9190018:	910003e8 	mov	x8, sp
    ffffb919001c:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb9190020:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb9190024:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffb9190028:	aa1f03f4 	mov	x20, xzr
    ffffb919002c:	aa1f03f5 	mov	x21, xzr
    ffffb9190030:	aa1f03f3 	mov	x19, xzr
    ffffb9190034:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffb9190038:	5280004f 	mov	w15, #0x2                   	// #2
    ffffb919003c:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffb9190040:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb9190044:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb9190048:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffb919004c:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffb9190050:	aa1603ee 	mov	x14, x22
    ffffb9190054:	b93fffef 	str	w15, [sp, #16380]
    ffffb9190058:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffb919005c:	d1000530 	sub	x16, x9, #0x1
    ffffb9190060:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffb9190064:	f11001ff 	cmp	x15, #0x400
    ffffb9190068:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffb919006c:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffb9190070:	f10ffdff 	cmp	x15, #0x3ff
    ffffb9190074:	9100162f 	add	x15, x17, #0x5
    ffffb9190078:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffb919007c:	ea4e419f 	tst	x12, x14, lsr #16
    ffffb9190080:	2a0903f1 	mov	w17, w9
    ffffb9190084:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffb9190088:	8b130133 	add	x19, x9, x19
    ffffb919008c:	aa1003e9 	mov	x9, x16
    ffffb9190090:	35fffe51 	cbnz	w17, 0xffffb9190058
    ffffb9190094:	d1001101 	sub	x1, x8, #0x4
    ffffb9190098:	52800080 	mov	w0, #0x4                   	// #4
    ffffb919009c:	aa1303e2 	mov	x2, x19
    ffffb91900a0:	aa1f03e3 	mov	x3, xzr
    ffffb91900a4:	9400000b 	bl	0xffffb91900d0
    ffffb91900a8:	b4000060 	cbz	x0, 0xffffb91900b4
    ffffb91900ac:	a9015414 	stp	x20, x21, [x0, #16]
    ffffb91900b0:	a9004c16 	stp	x22, x19, [x0]
    ffffb91900b4:	aa1303e0 	mov	x0, x19
    ffffb91900b8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb91900bc:	910143ff 	add	sp, sp, #0x50
    ffffb91900c0:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffb91900c4:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffb91900c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffb91900cc:	d65f03c0 	ret
    ffffb91900d0:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb91900d4:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb91900d8:	f2b6c870 	movk	x16, #0xb643, lsl #16
    ffffb91900dc:	f2865a10 	movk	x16, #0x32d0
    ffffb91900e0:	d61f0200 	br	x16
	...
