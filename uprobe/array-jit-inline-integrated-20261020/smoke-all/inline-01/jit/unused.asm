
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/inline-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffb7f05000 <.data>:
    ffffb7f05000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb7f05004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb7f05008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb7f0500c:	d10143ff 	sub	sp, sp, #0x50
    ffffb7f05010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb7f05014:	910003e9 	mov	x9, sp
    ffffb7f05018:	8b080133 	add	x19, x9, x8
    ffffb7f0501c:	52800034 	mov	w20, #0x1                   	// #1
    ffffb7f05020:	14000005 	b	0xffffb7f05034
    ffffb7f05024:	91000688 	add	x8, x20, #0x1
    ffffb7f05028:	710fa29f 	cmp	w20, #0x3e8
    ffffb7f0502c:	aa0803f4 	mov	x20, x8
    ffffb7f05030:	54000120 	b.eq	0xffffb7f05054  // b.none
    ffffb7f05034:	51000688 	sub	w8, w20, #0x1
    ffffb7f05038:	7110011f 	cmp	w8, #0x400
    ffffb7f0503c:	b93fffe8 	str	w8, [sp, #16380]
    ffffb7f05040:	54ffff23 	b.cc	0xffffb7f05024  // b.lo, b.ul, b.last
    ffffb7f05044:	52800060 	mov	w0, #0x3                   	// #3
    ffffb7f05048:	aa1303e1 	mov	x1, x19
    ffffb7f0504c:	94000008 	bl	0xffffb7f0506c
    ffffb7f05050:	17fffff5 	b	0xffffb7f05024
    ffffb7f05054:	aa1f03e0 	mov	x0, xzr
    ffffb7f05058:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb7f0505c:	910143ff 	add	sp, sp, #0x50
    ffffb7f05060:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb7f05064:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb7f05068:	d65f03c0 	ret
    ffffb7f0506c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb7f05070:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb7f05074:	f2b6a390 	movk	x16, #0xb51c, lsl #16
    ffffb7f05078:	f291f690 	movk	x16, #0x8fb4
    ffffb7f0507c:	d61f0200 	br	x16
	...
