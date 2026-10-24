
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/off-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffbc5f9000 <.data>:
    ffffbc5f9000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffbc5f9004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffbc5f9008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffbc5f900c:	d10143ff 	sub	sp, sp, #0x50
    ffffbc5f9010:	f9400014 	ldr	x20, [x0]
    ffffbc5f9014:	910003e8 	mov	x8, sp
    ffffbc5f9018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffbc5f901c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffbc5f9020:	aa1f03f3 	mov	x19, xzr
    ffffbc5f9024:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffbc5f9028:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffbc5f902c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffbc5f9030:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffbc5f9034:	aa1403ec 	mov	x12, x20
    ffffbc5f9038:	b93fffff 	str	wzr, [sp, #16380]
    ffffbc5f903c:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffbc5f9040:	b1000529 	adds	x9, x9, #0x1
    ffffbc5f9044:	d350658d 	ubfx	x13, x12, #16, #10
    ffffbc5f9048:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffbc5f904c:	8b0d026d 	add	x13, x19, x13
    ffffbc5f9050:	910015b3 	add	x19, x13, #0x5
    ffffbc5f9054:	54ffff43 	b.cc	0xffffbc5f903c  // b.lo, b.ul, b.last
    ffffbc5f9058:	d1001101 	sub	x1, x8, #0x4
    ffffbc5f905c:	52800080 	mov	w0, #0x4                   	// #4
    ffffbc5f9060:	aa1303e2 	mov	x2, x19
    ffffbc5f9064:	9400000b 	bl	0xffffbc5f9090
    ffffbc5f9068:	b4000080 	cbz	x0, 0xffffbc5f9078
    ffffbc5f906c:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffbc5f9070:	a9004c14 	stp	x20, x19, [x0]
    ffffbc5f9074:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffbc5f9078:	aa1303e0 	mov	x0, x19
    ffffbc5f907c:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffbc5f9080:	910143ff 	add	sp, sp, #0x50
    ffffbc5f9084:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffbc5f9088:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffbc5f908c:	d65f03c0 	ret
    ffffbc5f9090:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffbc5f9094:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffbc5f9098:	f2b73110 	movk	x16, #0xb988, lsl #16
    ffffbc5f909c:	f2910610 	movk	x16, #0x8830
    ffffbc5f90a0:	d61f0200 	br	x16
	...
