
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-release/off-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffb0053000 <.data>:
    ffffb0053000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb0053004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb0053008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb005300c:	d10143ff 	sub	sp, sp, #0x50
    ffffb0053010:	f9400014 	ldr	x20, [x0]
    ffffb0053014:	910003e8 	mov	x8, sp
    ffffb0053018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb005301c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb0053020:	aa1f03f3 	mov	x19, xzr
    ffffb0053024:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffb0053028:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffb005302c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb0053030:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb0053034:	aa1403ec 	mov	x12, x20
    ffffb0053038:	b93fffff 	str	wzr, [sp, #16380]
    ffffb005303c:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffb0053040:	b1000529 	adds	x9, x9, #0x1
    ffffb0053044:	d350658d 	ubfx	x13, x12, #16, #10
    ffffb0053048:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffb005304c:	8b0d026d 	add	x13, x19, x13
    ffffb0053050:	910015b3 	add	x19, x13, #0x5
    ffffb0053054:	54ffff43 	b.cc	0xffffb005303c  // b.lo, b.ul, b.last
    ffffb0053058:	d1001101 	sub	x1, x8, #0x4
    ffffb005305c:	52800080 	mov	w0, #0x4                   	// #4
    ffffb0053060:	aa1303e2 	mov	x2, x19
    ffffb0053064:	9400000b 	bl	0xffffb0053090
    ffffb0053068:	b4000080 	cbz	x0, 0xffffb0053078
    ffffb005306c:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffb0053070:	a9004c14 	stp	x20, x19, [x0]
    ffffb0053074:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffb0053078:	aa1303e0 	mov	x0, x19
    ffffb005307c:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb0053080:	910143ff 	add	sp, sp, #0x50
    ffffb0053084:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb0053088:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb005308c:	d65f03c0 	ret
    ffffb0053090:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb0053094:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb0053098:	f2b5a5d0 	movk	x16, #0xad2e, lsl #16
    ffffb005309c:	f2865610 	movk	x16, #0x32b0
    ffffb00530a0:	d61f0200 	br	x16
	...
