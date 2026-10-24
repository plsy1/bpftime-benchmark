
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/inline-01/jit/unused.bin:     file format binary


Disassembly of section .data:

0000ffffb544d000 <.data>:
    ffffb544d000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb544d004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb544d008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb544d00c:	d10143ff 	sub	sp, sp, #0x50
    ffffb544d010:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb544d014:	910003e9 	mov	x9, sp
    ffffb544d018:	8b080133 	add	x19, x9, x8
    ffffb544d01c:	52800034 	mov	w20, #0x1                   	// #1
    ffffb544d020:	14000005 	b	0xffffb544d034
    ffffb544d024:	91000688 	add	x8, x20, #0x1
    ffffb544d028:	710fa29f 	cmp	w20, #0x3e8
    ffffb544d02c:	aa0803f4 	mov	x20, x8
    ffffb544d030:	54000120 	b.eq	0xffffb544d054  // b.none
    ffffb544d034:	51000688 	sub	w8, w20, #0x1
    ffffb544d038:	7110011f 	cmp	w8, #0x400
    ffffb544d03c:	b93fffe8 	str	w8, [sp, #16380]
    ffffb544d040:	54ffff23 	b.cc	0xffffb544d024  // b.lo, b.ul, b.last
    ffffb544d044:	92800000 	mov	x0, #0xffffffffffffffff    	// #-1
    ffffb544d048:	aa1303e1 	mov	x1, x19
    ffffb544d04c:	94000008 	bl	0xffffb544d06c
    ffffb544d050:	17fffff5 	b	0xffffb544d024
    ffffb544d054:	aa1f03e0 	mov	x0, xzr
    ffffb544d058:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb544d05c:	910143ff 	add	sp, sp, #0x50
    ffffb544d060:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb544d064:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb544d068:	d65f03c0 	ret
    ffffb544d06c:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb544d070:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb544d074:	f2b64dd0 	movk	x16, #0xb26e, lsl #16
    ffffb544d078:	f293fc90 	movk	x16, #0x9fe4
    ffffb544d07c:	d61f0200 	br	x16
	...
