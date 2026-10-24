
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/off-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffb9165000 <.data>:
    ffffb9165000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb9165004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb9165008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb916500c:	d10143ff 	sub	sp, sp, #0x50
    ffffb9165010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb9165014:	910003e9 	mov	x9, sp
    ffffb9165018:	8b080133 	add	x19, x9, x8
    ffffb916501c:	52800034 	mov	w20, #0x1                   	// #1
    ffffb9165020:	51000688 	sub	w8, w20, #0x1
    ffffb9165024:	52800060 	mov	w0, #0x3                   	// #3
    ffffb9165028:	aa1303e1 	mov	x1, x19
    ffffb916502c:	b93fffe8 	str	w8, [sp, #16380]
    ffffb9165030:	9400000b 	bl	0xffffb916505c
    ffffb9165034:	91000688 	add	x8, x20, #0x1
    ffffb9165038:	710fa29f 	cmp	w20, #0x3e8
    ffffb916503c:	aa0803f4 	mov	x20, x8
    ffffb9165040:	54ffff01 	b.ne	0xffffb9165020  // b.any
    ffffb9165044:	aa1f03e0 	mov	x0, xzr
    ffffb9165048:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb916504c:	910143ff 	add	sp, sp, #0x50
    ffffb9165050:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb9165054:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb9165058:	d65f03c0 	ret
    ffffb916505c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb9165060:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb9165064:	f2b6c870 	movk	x16, #0xb643, lsl #16
    ffffb9165068:	f2865a10 	movk	x16, #0x32d0
    ffffb916506c:	d61f0200 	br	x16
	...
