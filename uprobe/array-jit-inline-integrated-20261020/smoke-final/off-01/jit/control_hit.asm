
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-final/off-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffb9194000 <.data>:
    ffffb9194000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffb9194004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffb9194008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffb919400c:	d10143ff 	sub	sp, sp, #0x50
    ffffb9194010:	f9400014 	ldr	x20, [x0]
    ffffb9194014:	910003e8 	mov	x8, sp
    ffffb9194018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffb919401c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffb9194020:	aa1f03f3 	mov	x19, xzr
    ffffb9194024:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffb9194028:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffb919402c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffb9194030:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffb9194034:	aa1403ec 	mov	x12, x20
    ffffb9194038:	b93fffff 	str	wzr, [sp, #16380]
    ffffb919403c:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffb9194040:	b1000529 	adds	x9, x9, #0x1
    ffffb9194044:	d350658d 	ubfx	x13, x12, #16, #10
    ffffb9194048:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffb919404c:	8b0d026d 	add	x13, x19, x13
    ffffb9194050:	910015b3 	add	x19, x13, #0x5
    ffffb9194054:	54ffff43 	b.cc	0xffffb919403c  // b.lo, b.ul, b.last
    ffffb9194058:	d1001101 	sub	x1, x8, #0x4
    ffffb919405c:	52800080 	mov	w0, #0x4                   	// #4
    ffffb9194060:	aa1303e2 	mov	x2, x19
    ffffb9194064:	9400000b 	bl	0xffffb9194090
    ffffb9194068:	b4000080 	cbz	x0, 0xffffb9194078
    ffffb919406c:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffb9194070:	a9004c14 	stp	x20, x19, [x0]
    ffffb9194074:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffb9194078:	aa1303e0 	mov	x0, x19
    ffffb919407c:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffb9194080:	910143ff 	add	sp, sp, #0x50
    ffffb9194084:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffb9194088:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffb919408c:	d65f03c0 	ret
    ffffb9194090:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffb9194094:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffb9194098:	f2b6c870 	movk	x16, #0xb643, lsl #16
    ffffb919409c:	f2865a10 	movk	x16, #0x32d0
    ffffb91940a0:	d61f0200 	br	x16
	...
