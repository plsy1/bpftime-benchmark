
/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019/smoke-all/inline-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffb36a4000 <.data>:
    ffffb36a4000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb36a4004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb36a4008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb36a400c:	d10143ff 	sub	sp, sp, #0x50
    ffffb36a4010:	f9400014 	ldr	x20, [x0]
    ffffb36a4014:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb36a4018:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb36a401c:	aa1f03f3 	mov	x19, xzr
    ffffb36a4020:	910003e8 	mov	x8, sp
    ffffb36a4024:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffb36a4028:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb36a402c:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb36a4030:	aa1403ec 	mov	x12, x20
    ffffb36a4034:	b93ffd1f 	str	wzr, [x8, #16380]
    ffffb36a4038:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffb36a403c:	b1000529 	adds	x9, x9, #0x1
    ffffb36a4040:	d350658d 	ubfx	x13, x12, #16, #10
    ffffb36a4044:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffb36a4048:	8b0d026d 	add	x13, x19, x13
    ffffb36a404c:	910015b3 	add	x19, x13, #0x5
    ffffb36a4050:	54ffff43 	b.cc	0xffffb36a4038  // b.lo, b.ul, b.last
    ffffb36a4054:	b97ffd08 	ldr	w8, [x8, #16380]
    ffffb36a4058:	71000d1f 	cmp	w8, #0x3
    ffffb36a405c:	540000c8 	b.hi	0xffffb36a4074  // b.pmore
    ffffb36a4060:	d29d3609 	mov	x9, #0xe9b0                	// #59824
    ffffb36a4064:	f2b59889 	movk	x9, #0xacc4, lsl #16
    ffffb36a4068:	f2dfffe9 	movk	x9, #0xffff, lsl #32
    ffffb36a406c:	8b081520 	add	x0, x9, x8, lsl #5
    ffffb36a4070:	14000008 	b	0xffffb36a4090
    ffffb36a4074:	5287ff88 	mov	w8, #0x3ffc                	// #16380
    ffffb36a4078:	910003e9 	mov	x9, sp
    ffffb36a407c:	8b080121 	add	x1, x9, x8
    ffffb36a4080:	52800080 	mov	w0, #0x4                   	// #4
    ffffb36a4084:	aa1303e2 	mov	x2, x19
    ffffb36a4088:	9400000b 	bl	0xffffb36a40b4
    ffffb36a408c:	b4000080 	cbz	x0, 0xffffb36a409c
    ffffb36a4090:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffb36a4094:	a9004c14 	stp	x20, x19, [x0]
    ffffb36a4098:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffb36a409c:	aa1303e0 	mov	x0, x19
    ffffb36a40a0:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb36a40a4:	910143ff 	add	sp, sp, #0x50
    ffffb36a40a8:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb36a40ac:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb36a40b0:	d65f03c0 	ret
    ffffb36a40b4:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb36a40b8:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb36a40bc:	f2b61270 	movk	x16, #0xb093, lsl #16
    ffffb36a40c0:	f2910610 	movk	x16, #0x8830
    ffffb36a40c4:	d61f0200 	br	x16
	...
