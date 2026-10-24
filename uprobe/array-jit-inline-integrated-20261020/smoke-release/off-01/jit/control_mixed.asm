
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/off-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffb004f000 <.data>:
    ffffb004f000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffb004f004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffb004f008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffb004f00c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb004f010:	d10143ff 	sub	sp, sp, #0x50
    ffffb004f014:	f9400016 	ldr	x22, [x0]
    ffffb004f018:	910003e8 	mov	x8, sp
    ffffb004f01c:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb004f020:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb004f024:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffb004f028:	aa1f03f4 	mov	x20, xzr
    ffffb004f02c:	aa1f03f5 	mov	x21, xzr
    ffffb004f030:	aa1f03f3 	mov	x19, xzr
    ffffb004f034:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffb004f038:	5280004f 	mov	w15, #0x2                   	// #2
    ffffb004f03c:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffb004f040:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb004f044:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb004f048:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffb004f04c:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffb004f050:	aa1603ee 	mov	x14, x22
    ffffb004f054:	b93fffef 	str	w15, [sp, #16380]
    ffffb004f058:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffb004f05c:	d1000530 	sub	x16, x9, #0x1
    ffffb004f060:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffb004f064:	f11001ff 	cmp	x15, #0x400
    ffffb004f068:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffb004f06c:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffb004f070:	f10ffdff 	cmp	x15, #0x3ff
    ffffb004f074:	9100162f 	add	x15, x17, #0x5
    ffffb004f078:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffb004f07c:	ea4e419f 	tst	x12, x14, lsr #16
    ffffb004f080:	2a0903f1 	mov	w17, w9
    ffffb004f084:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffb004f088:	8b130133 	add	x19, x9, x19
    ffffb004f08c:	aa1003e9 	mov	x9, x16
    ffffb004f090:	35fffe51 	cbnz	w17, 0xffffb004f058
    ffffb004f094:	d1001101 	sub	x1, x8, #0x4
    ffffb004f098:	52800080 	mov	w0, #0x4                   	// #4
    ffffb004f09c:	aa1303e2 	mov	x2, x19
    ffffb004f0a0:	aa1f03e3 	mov	x3, xzr
    ffffb004f0a4:	9400000b 	bl	0xffffb004f0d0
    ffffb004f0a8:	b4000060 	cbz	x0, 0xffffb004f0b4
    ffffb004f0ac:	a9015414 	stp	x20, x21, [x0, #16]
    ffffb004f0b0:	a9004c16 	stp	x22, x19, [x0]
    ffffb004f0b4:	aa1303e0 	mov	x0, x19
    ffffb004f0b8:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb004f0bc:	910143ff 	add	sp, sp, #0x50
    ffffb004f0c0:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffb004f0c4:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffb004f0c8:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffb004f0cc:	d65f03c0 	ret
    ffffb004f0d0:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb004f0d4:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb004f0d8:	f2b5a5d0 	movk	x16, #0xad2e, lsl #16
    ffffb004f0dc:	f2865610 	movk	x16, #0x32b0
    ffffb004f0e0:	d61f0200 	br	x16
	...
