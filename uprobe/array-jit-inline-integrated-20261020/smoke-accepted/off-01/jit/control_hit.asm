
/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020/smoke-accepted/off-01/jit/control_hit.bin:     file format binary


Disassembly of section .data:

0000ffffae8f2000 <.data>:
    ffffae8f2000:	a9be7bfd 	stp	x29, x30, [sp, #-32]!
    ffffae8f2004:	a9014ff4 	stp	x20, x19, [sp, #16]
    ffffae8f2008:	d14013ff 	sub	sp, sp, #0x4, lsl #12
    ffffae8f200c:	d10143ff 	sub	sp, sp, #0x50
    ffffae8f2010:	f9400014 	ldr	x20, [x0]
    ffffae8f2014:	910003e8 	mov	x8, sp
    ffffae8f2018:	528cc1aa 	mov	w10, #0x660d                	// #26125
    ffffae8f201c:	529e6beb 	mov	w11, #0xf35f                	// #62303
    ffffae8f2020:	aa1f03f3 	mov	x19, xzr
    ffffae8f2024:	91401108 	add	x8, x8, #0x4, lsl #12
    ffffae8f2028:	92807ce9 	mov	x9, #0xfffffffffffffc18    	// #-1000
    ffffae8f202c:	72a0032a 	movk	w10, #0x19, lsl #16
    ffffae8f2030:	72a78dcb 	movk	w11, #0x3c6e, lsl #16
    ffffae8f2034:	aa1403ec 	mov	x12, x20
    ffffae8f2038:	b93fffff 	str	wzr, [sp, #16380]
    ffffae8f203c:	9b0a2d8c 	madd	x12, x12, x10, x11
    ffffae8f2040:	b1000529 	adds	x9, x9, #0x1
    ffffae8f2044:	d350658d 	ubfx	x13, x12, #16, #10
    ffffae8f2048:	8b0d11ad 	add	x13, x13, x13, lsl #4
    ffffae8f204c:	8b0d026d 	add	x13, x19, x13
    ffffae8f2050:	910015b3 	add	x19, x13, #0x5
    ffffae8f2054:	54ffff43 	b.cc	0xffffae8f203c  // b.lo, b.ul, b.last
    ffffae8f2058:	d1001101 	sub	x1, x8, #0x4
    ffffae8f205c:	52800080 	mov	w0, #0x4                   	// #4
    ffffae8f2060:	aa1303e2 	mov	x2, x19
    ffffae8f2064:	9400000b 	bl	0xffffae8f2090
    ffffae8f2068:	b4000080 	cbz	x0, 0xffffae8f2078
    ffffae8f206c:	52807d08 	mov	w8, #0x3e8                 	// #1000
    ffffae8f2070:	a9004c14 	stp	x20, x19, [x0]
    ffffae8f2074:	a9017c08 	stp	x8, xzr, [x0, #16]
    ffffae8f2078:	aa1303e0 	mov	x0, x19
    ffffae8f207c:	914013ff 	add	sp, sp, #0x4, lsl #12
    ffffae8f2080:	910143ff 	add	sp, sp, #0x50
    ffffae8f2084:	a9414ff4 	ldp	x20, x19, [sp, #16]
    ffffae8f2088:	a8c27bfd 	ldp	x29, x30, [sp], #32
    ffffae8f208c:	d65f03c0 	ret
    ffffae8f2090:	d2e00010 	movz	x16, #0x0, lsl #48
    ffffae8f2094:	f2dffff0 	movk	x16, #0xffff, lsl #32
    ffffae8f2098:	f2b576f0 	movk	x16, #0xabb7, lsl #16
    ffffae8f209c:	f2887610 	movk	x16, #0x43b0
    ffffae8f20a0:	d61f0200 	br	x16
	...
