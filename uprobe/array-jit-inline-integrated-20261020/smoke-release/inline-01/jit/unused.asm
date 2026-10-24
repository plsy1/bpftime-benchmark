
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/inline-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffff8f4ae000 <.data>:
    ffff8f4ae000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffff8f4ae004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffff8f4ae008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff8f4ae00c:	d10143ff 	sub	sp, sp, #0x50
    ffff8f4ae010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffff8f4ae014:	910003e9 	mov	x9, sp
    ffff8f4ae018:	8b080133 	add	x19, x9, x8
    ffff8f4ae01c:	52800034 	mov	w20, #0x1                   	// #1
    ffff8f4ae020:	14000005 	b	0xffff8f4ae034
    ffff8f4ae024:	91000688 	add	x8, x20, #0x1
    ffff8f4ae028:	710fa29f 	cmp	w20, #0x3e8
    ffff8f4ae02c:	aa0803f4 	mov	x20, x8
    ffff8f4ae030:	54000120 	b.eq	0xffff8f4ae054  // b.none
    ffff8f4ae034:	51000688 	sub	w8, w20, #0x1
    ffff8f4ae038:	7110011f 	cmp	w8, #0x400
    ffff8f4ae03c:	b93fffe8 	str	w8, [sp, #16380]
    ffff8f4ae040:	54ffff23 	b.cc	0xffff8f4ae024  // b.lo, b.ul, b.last
    ffff8f4ae044:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffff8f4ae048:	aa1303e1 	mov	x1, x19
    ffff8f4ae04c:	94000008 	bl	0xffff8f4ae06c
    ffff8f4ae050:	17fffff5 	b	0xffff8f4ae024
    ffff8f4ae054:	aa1f03e0 	mov	x0, xzr
    ffff8f4ae058:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff8f4ae05c:	910143ff 	add	sp, sp, #0x50
    ffff8f4ae060:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffff8f4ae064:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffff8f4ae068:	d65f03c0 	ret
    ffff8f4ae06c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff8f4ae070:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff8f4ae074:	f2b18eb0 	movk	x16, #0x8c75, lsl #16
    ffff8f4ae078:	f291f690 	movk	x16, #0x8fb4
    ffff8f4ae07c:	d61f0200 	br	x16
	...
