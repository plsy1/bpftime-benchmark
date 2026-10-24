
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/off-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffb0024000 <.data>:
    ffffb0024000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb0024004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb0024008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb002400c:	d10143ff 	sub	sp, sp, #0x50
    ffffb0024010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb0024014:	910003e9 	mov	x9, sp
    ffffb0024018:	8b080133 	add	x19, x9, x8
    ffffb002401c:	52800034 	mov	w20, #0x1                   	// #1
    ffffb0024020:	51000688 	sub	w8, w20, #0x1
    ffffb0024024:	52800060 	mov	w0, #0x3                   	// #3
    ffffb0024028:	aa1303e1 	mov	x1, x19
    ffffb002402c:	b93fffe8 	str	w8, [sp, #16380]
    ffffb0024030:	9400000b 	bl	0xffffb002405c
    ffffb0024034:	91000688 	add	x8, x20, #0x1
    ffffb0024038:	710fa29f 	cmp	w20, #0x3e8
    ffffb002403c:	aa0803f4 	mov	x20, x8
    ffffb0024040:	54ffff01 	b.ne	0xffffb0024020  // b.any
    ffffb0024044:	aa1f03e0 	mov	x0, xzr
    ffffb0024048:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb002404c:	910143ff 	add	sp, sp, #0x50
    ffffb0024050:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb0024054:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb0024058:	d65f03c0 	ret
    ffffb002405c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb0024060:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb0024064:	f2b5a5d0 	movk	x16, #0xad2e, lsl #16
    ffffb0024068:	f2865610 	movk	x16, #0x32b0
    ffffb002406c:	d61f0200 	br	x16
	...
