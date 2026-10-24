
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/off-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffbc5ca000 <.data>:
    ffffbc5ca000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffbc5ca004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffbc5ca008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbc5ca00c:	d10143ff 	sub	sp, sp, #0x50
    ffffbc5ca010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffbc5ca014:	910003e9 	mov	x9, sp
    ffffbc5ca018:	8b080133 	add	x19, x9, x8
    ffffbc5ca01c:	52800034 	mov	w20, #0x1                   	// #1
    ffffbc5ca020:	51000688 	sub	w8, w20, #0x1
    ffffbc5ca024:	52800060 	mov	w0, #0x3                   	// #3
    ffffbc5ca028:	aa1303e1 	mov	x1, x19
    ffffbc5ca02c:	b93fffe8 	str	w8, [sp, #16380]
    ffffbc5ca030:	9400000b 	bl	0xffffbc5ca05c
    ffffbc5ca034:	91000688 	add	x8, x20, #0x1
    ffffbc5ca038:	710fa29f 	cmp	w20, #0x3e8
    ffffbc5ca03c:	aa0803f4 	mov	x20, x8
    ffffbc5ca040:	54ffff01 	b.ne	0xffffbc5ca020  // b.any
    ffffbc5ca044:	aa1f03e0 	mov	x0, xzr
    ffffbc5ca048:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbc5ca04c:	910143ff 	add	sp, sp, #0x50
    ffffbc5ca050:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffbc5ca054:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffbc5ca058:	d65f03c0 	ret
    ffffbc5ca05c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbc5ca060:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbc5ca064:	f2b73110 	movk	x16, #0xb988, lsl #16
    ffffbc5ca068:	f2910610 	movk	x16, #0x8830
    ffffbc5ca06c:	d61f0200 	br	x16
	...
