
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-all/off-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffff828a4000 <.data>:
    ffff828a4000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffff828a4004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffff828a4008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffff828a400c:	d10143ff 	sub	sp, sp, #0x50
    ffff828a4010:	f9400014 	ldr	x20, [x0]
    ffff828a4014:	910003e8 	mov	x8, sp
    ffff828a4018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffff828a401c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffff828a4020:	aa1f03f3 	mov	x19, xzr
    ffff828a4024:	91401108 	add	x8, x8, #0x4, lsl #12
    ffff828a4028:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffff828a402c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffff828a4030:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffff828a4034:	aa1403ec 	mov	x12, x20
    ffff828a4038:	b93fffff 	str	wzr, [sp, #16380]
    ffff828a403c:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffff828a4040:	b1000529 	adds	x9, x9, #0x1
    ffff828a4044:	d350658d 	ubfx	x13, x12, #16, #10
    ffff828a4048:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffff828a404c:	8b0d026d 	add	x13, x19, x13
    ffff828a4050:	910015b3 	add	x19, x13, #0x5
    ffff828a4054:	54ffff43 	b.cc	0xffff828a403c  // b.lo, b.ul, b.last
    ffff828a4058:	d1001101 	sub	x1, x8, #0x4
    ffff828a405c:	52800080 	mov	w0, #0x4                   	// #4
    ffff828a4060:	aa1303e2 	mov	x2, x19
    ffff828a4064:	9400000b 	bl	0xffff828a4090
    ffff828a4068:	b4000080 	cbz	x0, 0xffff828a4078
    ffff828a406c:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffff828a4070:	a9004c14 	stp	x20, x19, [x0]
    ffff828a4074:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffff828a4078:	aa1303e0 	mov	x0, x19
    ffff828a407c:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffff828a4080:	910143ff 	add	sp, sp, #0x50
    ffff828a4084:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffff828a4088:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffff828a408c:	d65f03c0 	ret
    ffff828a4090:	d2e00010 	movz	x16, #0x0, lsl #48
    ffff828a4094:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffff828a4098:	f2aff690 	movk	x16, #0x7fb4, lsl #16
    ffff828a409c:	f2864c10 	movk	x16, #0x3260
    ffff828a40a0:	d61f0200 	br	x16
	...
