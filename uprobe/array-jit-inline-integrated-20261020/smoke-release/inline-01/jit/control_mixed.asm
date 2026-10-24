
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/inline-01/jit/control_mixed.bin:     file format binary


Disassembly of section .data:

0000ffff8f4d9000 <.data>:
    ffff8f4d9000:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
    ffff8f4d9004:	a90157f6 	stp	x22, x21, [sp, #16]
    ffff8f4d9008:	a9024ff4 	stp	x20, x19, [sp, #32]
    ffff8f4d900c:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff8f4d9010:	d10143ff 	sub	sp, sp, #0x50
    ffff8f4d9014:	f9400016 	ldr	x22, [x0]
    ffff8f4d9018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffff8f4d901c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffff8f4d9020:	528f372d 	mov	w13, #0x79b9                	// #31161
    ffff8f4d9024:	aa1f03f4 	mov	x20, xzr
    ffff8f4d9028:	aa1f03f5 	mov	x21, xzr
    ffff8f4d902c:	aa1f03f3 	mov	x19, xzr
    ffff8f4d9030:	910003e8 	mov	x8, sp
    ffff8f4d9034:	5280004f 	mov	w15, #0x2                   	// #2
    ffff8f4d9038:	52807ce9 	mov	w9, #0x3e7                 	// #999
    ffff8f4d903c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffff8f4d9040:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffff8f4d9044:	5280800c 	mov	w12, #0x400                 	// #1024
    ffff8f4d9048:	72b3c6ed 	movk	w13, #0x9e37, lsl #16
    ffff8f4d904c:	aa1603ee 	mov	x14, x22
    ffff8f4d9050:	b93ffd0f 	str	w15, [x8, #16380]
    ffff8f4d9054:	9b0a2dce 	madd	x14, x14, x10, x11
    ffff8f4d9058:	d1000530 	sub	x16, x9, #0x1
    ffff8f4d905c:	d35069cf 	ubfx	x15, x14, #16, #11
    ffff8f4d9060:	f11001ff 	cmp	x15, #0x400
    ffff8f4d9064:	9a942694 	cinc	x20, x20, cc	// cc = lo, ul, last
    ffff8f4d9068:	8b0f11f1 	add	x17, x15, x15, lsl #4
    ffff8f4d906c:	f10ffdff 	cmp	x15, #0x3ff
    ffff8f4d9070:	9100162f 	add	x15, x17, #0x5
    ffff8f4d9074:	9a9596b5 	cinc	x21, x21, hi	// hi = pmore
    ffff8f4d9078:	ea4e419f 	tst	x12, x14, lsr #16
    ffff8f4d907c:	2a0903f1 	mov	w17, w9
    ffff8f4d9080:	9a8d01e9 	csel	x9, x15, x13, eq	// eq = none
    ffff8f4d9084:	8b130133 	add	x19, x9, x19
    ffff8f4d9088:	aa1003e9 	mov	x9, x16
    ffff8f4d908c:	35fffe51 	cbnz	w17, 0xffff8f4d9054
    ffff8f4d9090:	b97ffd08 	ldr	w8, [x8, #16380]
    ffff8f4d9094:	71000d1f 	cmp	w8, #0x3
    ffff8f4d9098:	540000c8 	b.hi	0xffff8f4d90b0  // b.pmore
    ffff8f4d909c:	d29d4609 	mov	x9, #0xea30                	// #59952
    ffff8f4d90a0:	f2b114c9 	movk	x9, #0x88a6, lsl #16
    ffff8f4d90a4:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffff8f4d90a8:	8b081520 	add	x0, x9, x8, lsl #5
    ffff8f4d90ac:	14000009 	b	0xffff8f4d90d0
    ffff8f4d90b0:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffff8f4d90b4:	910003e9 	mov	x9, sp
    ffff8f4d90b8:	8b080121 	add	x1, x9, x8
    ffff8f4d90bc:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffff8f4d90c0:	aa1303e2 	mov	x2, x19
    ffff8f4d90c4:	aa1f03e3 	mov	x3, xzr
    ffff8f4d90c8:	9400000b 	bl	0xffff8f4d90f4
    ffff8f4d90cc:	b4000060 	cbz	x0, 0xffff8f4d90d8
    ffff8f4d90d0:	a9015414 	stp	x20, x21, [x0, #16]
    ffff8f4d90d4:	a9004c16 	stp	x22, x19, [x0]
    ffff8f4d90d8:	aa1303e0 	mov	x0, x19
    ffff8f4d90dc:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff8f4d90e0:	910143ff 	add	sp, sp, #0x50
    ffff8f4d90e4:	a9424ff4 	ldp	x20, x19, [sp, #32]
    ffff8f4d90e8:	a94157f6 	ldp	x22, x21, [sp, #16]
    ffff8f4d90ec:	a8c37bfd 	ldp	x29, x30, [sp], #48
    ffff8f4d90f0:	d65f03c0 	ret
    ffff8f4d90f4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff8f4d90f8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff8f4d90fc:	f2b18eb0 	movk	x16, #0x8c75, lsl #16
    ffff8f4d9100:	f291f690 	movk	x16, #0x8fb4
    ffff8f4d9104:	d61f0200 	br	x16
	...
