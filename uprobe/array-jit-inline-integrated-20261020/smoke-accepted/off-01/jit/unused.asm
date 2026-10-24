
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/off-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffae8c3000 <.data>:
    ffffae8c3000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffae8c3004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffae8c3008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffae8c300c:	d10143ff 	sub	sp, sp, #0x50
    ffffae8c3010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffae8c3014:	910003e9 	mov	x9, sp
    ffffae8c3018:	8b080133 	add	x19, x9, x8
    ffffae8c301c:	52800034 	mov	w20, #0x1                   	// #1
    ffffae8c3020:	51000688 	sub	w8, w20, #0x1
    ffffae8c3024:	52800060 	mov	w0, #0x3                   	// #3
    ffffae8c3028:	aa1303e1 	mov	x1, x19
    ffffae8c302c:	b93fffe8 	str	w8, [sp, #16380]
    ffffae8c3030:	9400000b 	bl	0xffffae8c305c
    ffffae8c3034:	91000688 	add	x8, x20, #0x1
    ffffae8c3038:	710fa29f 	cmp	w20, #0x3e8
    ffffae8c303c:	aa0803f4 	mov	x20, x8
    ffffae8c3040:	54ffff01 	b.ne	0xffffae8c3020  // b.any
    ffffae8c3044:	aa1f03e0 	mov	x0, xzr
    ffffae8c3048:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffae8c304c:	910143ff 	add	sp, sp, #0x50
    ffffae8c3050:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffae8c3054:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffae8c3058:	d65f03c0 	ret
    ffffae8c305c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffae8c3060:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffae8c3064:	f2b576f0 	movk	x16, #0xabb7, lsl #16
    ffffae8c3068:	f2887610 	movk	x16, #0x43b0
    ffffae8c306c:	d61f0200 	br	x16
	...
