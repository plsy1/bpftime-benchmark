
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/inline-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffbbe96000 <.data>:
    ffffbbe96000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffbbe96004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffbbe96008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbbe9600c:	d10143ff 	sub	sp, sp, #0x50
    ffffbbe96010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffbbe96014:	910003e9 	mov	x9, sp
    ffffbbe96018:	8b080133 	add	x19, x9, x8
    ffffbbe9601c:	52800034 	mov	w20, #0x1                   	// #1
    ffffbbe96020:	14000005 	b	0xffffbbe96034
    ffffbbe96024:	91000688 	add	x8, x20, #0x1
    ffffbbe96028:	710fa29f 	cmp	w20, #0x3e8
    ffffbbe9602c:	aa0803f4 	mov	x20, x8
    ffffbbe96030:	54000120 	b.eq	0xffffbbe96054  // b.none
    ffffbbe96034:	51000688 	sub	w8, w20, #0x1
    ffffbbe96038:	7110011f 	cmp	w8, #0x400
    ffffbbe9603c:	b93fffe8 	str	w8, [sp, #16380]
    ffffbbe96040:	54ffff23 	b.cc	0xffffbbe96024  // b.lo, b.ul, b.last
    ffffbbe96044:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffbbe96048:	aa1303e1 	mov	x1, x19
    ffffbbe9604c:	94000008 	bl	0xffffbbe9606c
    ffffbbe96050:	17fffff5 	b	0xffffbbe96024
    ffffbbe96054:	aa1f03e0 	mov	x0, xzr
    ffffbbe96058:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbbe9605c:	910143ff 	add	sp, sp, #0x50
    ffffbbe96060:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffbbe96064:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffbbe96068:	d65f03c0 	ret
    ffffbbe9606c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbbe96070:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbbe96074:	f2b722b0 	movk	x16, #0xb915, lsl #16
    ffffbbe96078:	f291f890 	movk	x16, #0x8fc4
    ffffbbe9607c:	d61f0200 	br	x16
	...
