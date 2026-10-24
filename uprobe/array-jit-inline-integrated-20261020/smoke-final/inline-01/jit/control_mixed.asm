
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/inline-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffffbbec1000 <.data>:
    ffffbbec1000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffffbbec1004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffffbbec1008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffffbbec100c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbbec1010:	d10143ff 	sub	sp, sp, #0x50
    ffffbbec1014:	f9400016 	ldr	x22, [x0]
    ffffbbec1018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffbbec101c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffbbec1020:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffffbbec1024:	aa1f03f4 	mov	x20, xzr
    ffffbbec1028:	aa1f03f5 	mov	x21, xzr
    ffffbbec102c:	aa1f03f3 	mov	x19, xzr
    ffffbbec1030:	910003e8 	mov	x8, sp
    ffffbbec1034:	5280004f 	mov	w15, #0x2                   	// #2
    ffffbbec1038:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffffbbec103c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffbbec1040:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffbbec1044:	5280800c 	mov	w12, #0x400                 	// #1024
    ffffbbec1048:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffffbbec104c:	aa1603ee 	mov	x14, x22
    ffffbbec1050:	b93ffd0f 	str	w15, [x8, #16380]
    ffffbbec1054:	9b0a2dce 	madd	x14, x14, x10, x11
    ffffbbec1058:	d1000530 	sub	x16, x9, #0x1
    ffffbbec105c:	d35069cf 	ubfx	x15, x14, #16, #11
    ffffbbec1060:	f11001ff 	cmp	x15, #0x400
    ffffbbec1064:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffffbbec1068:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffffbbec106c:	f10ffdff 	cmp	x15, #0x3ff
    ffffbbec1070:	9100162f 	add	x15, x17, #0x5
    ffffbbec1074:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffffbbec1078:	ea4e419f 	tst	x12, x14, lsr #16
    ffffbbec107c:	2a0903f1 	mov	w17, w9
    ffffbbec1080:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffffbbec1084:	8b130133 	add	x19, x9, x19
    ffffbbec1088:	aa1003e9 	mov	x9, x16
    ffffbbec108c:	35fffe51 	cbnz	w17, 0xffffbbec1054
    ffffbbec1090:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffbbec1094:	71000d1f 	cmp	w8, #0x3
    ffffbbec1098:	540000c8 	b.hi	0xffffbbec10b0  // b.pmore
    ffffbbec109c:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffffbbec10a0:	f2b6a8c9 	movk	x9, #0xb546, lsl #16
    ffffbbec10a4:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffbbec10a8:	8b081520 	add	x0, x9, x8, lsl #5
    ffffbbec10ac:	14000009 	b	0xffffbbec10d0
    ffffbbec10b0:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffbbec10b4:	910003e9 	mov	x9, sp
    ffffbbec10b8:	8b080121 	add	x1, x9, x8
    ffffbbec10bc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbec10c0:	aa1303e2 	mov	x2, x19
    ffffbbec10c4:	aa1f03e3 	mov	x3, xzr
    ffffbbec10c8:	9400000b 	bl	0xffffbbec10f4
    ffffbbec10cc:	b4000060 	cbz	x0, 0xffffbbec10d8
    ffffbbec10d0:	a9015414 	stp	x20, x21, [x0, #16]
    ffffbbec10d4:	a9004c16 	stp	x22, x19, [x0]
    ffffbbec10d8:	aa1303e0 	mov	x0, x19
    ffffbbec10dc:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbbec10e0:	910143ff 	add	sp, sp, #0x50
    ffffbbec10e4:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffffbbec10e8:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffffbbec10ec:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffffbbec10f0:	d65f03c0 	ret
    ffffbbec10f4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbbec10f8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbbec10fc:	f2b722b0 	movk	x16, #0xb915, lsl #16
    ffffbbec1100:	f291f890 	movk	x16, #0x8fc4
    ffffbbec1104:	d61f0200 	br	x16
	...
