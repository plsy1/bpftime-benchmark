
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/off-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffff82875000 <.data>:
    ffff82875000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffff82875004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffff82875008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff8287500c:	d10143ff 	sub	sp, sp, #0x50
    ffff82875010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffff82875014:	910003e9 	mov	x9, sp
    ffff82875018:	8b080133 	add	x19, x9, x8
    ffff8287501c:	52800034 	mov	w20, #0x1                   	// #1
    ffff82875020:	51000688 	sub	w8, w20, #0x1
    ffff82875024:	52800060 	mov	w0, #0x3                   	// #3
    ffff82875028:	aa1303e1 	mov	x1, x19
    ffff8287502c:	b93fffe8 	str	w8, [sp, #16380]
    ffff82875030:	9400000b 	bl	0xffff8287505c
    ffff82875034:	91000688 	add	x8, x20, #0x1
    ffff82875038:	710fa29f 	cmp	w20, #0x3e8
    ffff8287503c:	aa0803f4 	mov	x20, x8
    ffff82875040:	54ffff01 	b.ne	0xffff82875020  // b.any
    ffff82875044:	aa1f03e0 	mov	x0, xzr
    ffff82875048:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff8287504c:	910143ff 	add	sp, sp, #0x50
    ffff82875050:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffff82875054:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffff82875058:	d65f03c0 	ret
    ffff8287505c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff82875060:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff82875064:	f2aff690 	movk	x16, #0x7fb4, lsl #16
    ffff82875068:	f2864c10 	movk	x16, #0x3260
    ffff8287506c:	d61f0200 	br	x16
	...
