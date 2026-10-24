
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/inline-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffb3675000 <.data>:
    ffffb3675000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb3675004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb3675008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb367500c:	d10143ff 	sub	sp, sp, #0x50
    ffffb3675010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb3675014:	910003e9 	mov	x9, sp
    ffffb3675018:	8b080133 	add	x19, x9, x8
    ffffb367501c:	52800034 	mov	w20, #0x1                   	// #1
    ffffb3675020:	14000005 	b	0xffffb3675034
    ffffb3675024:	91000688 	add	x8, x20, #0x1
    ffffb3675028:	710fa29f 	cmp	w20, #0x3e8
    ffffb367502c:	aa0803f4 	mov	x20, x8
    ffffb3675030:	54000120 	b.eq	0xffffb3675054  // b.none
    ffffb3675034:	51000688 	sub	w8, w20, #0x1
    ffffb3675038:	7110011f 	cmp	w8, #0x400
    ffffb367503c:	b93fffe8 	str	w8, [sp, #16380]
    ffffb3675040:	54ffff23 	b.cc	0xffffb3675024  // b.lo, b.ul, b.last
    ffffb3675044:	52800060 	mov	w0, #0x3                   	// #3
    ffffb3675048:	aa1303e1 	mov	x1, x19
    ffffb367504c:	94000008 	bl	0xffffb367506c
    ffffb3675050:	17fffff5 	b	0xffffb3675024
    ffffb3675054:	aa1f03e0 	mov	x0, xzr
    ffffb3675058:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb367505c:	910143ff 	add	sp, sp, #0x50
    ffffb3675060:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb3675064:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb3675068:	d65f03c0 	ret
    ffffb367506c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb3675070:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb3675074:	f2b61270 	movk	x16, #0xb093, lsl #16
    ffffb3675078:	f2910610 	movk	x16, #0x8830
    ffffb367507c:	d61f0200 	br	x16
	...
