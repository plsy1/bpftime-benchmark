stp	x29, x30, [sp, #-48]!
adrp	x4, ADDR
mov	x29, sp
ldr	x4, [x4, #2232]
cmp	x4, #0x0
ccmp	w0, #0x0, #0x1, ne	// ne = any
b.lt	ADDR  // b.tstop
ldr	x5, [x4, #16]
cmp	x5, w0, sxtw
b.ls	ADDR  // b.plast
stp	x19, x20, [sp, #16]
mov	w6, #0xa8                  	// #168
add	x4, x4, #0x8
ldr	x5, [x4]
smull	x0, w0, w6
cmp	x5, #0x1
add	x4, x4, x5
csetm	x19, ne	// ne = any
and	x19, x19, x4
adds	x19, x19, x0
b.eq	ADDR  // b.none
ldrb	w0, [x19, #160]
cmp	w0, #0x1
b.ne	ADDR  // b.any
ldr	w0, [x19, #72]
cmp	w0, #0x1
b.eq	ADDR  // b.none
mov	x0, x19
ldp	x19, x20, [sp, #16]
ldp	x29, x30, [sp], #48
b	ADDR
bl	ADDR
ldp	x19, x20, [sp, #16]
mov	w1, #0x2                   	// #2
str	w1, [x0]
mov	x0, #0xffffffffffffffff    	// #-1
ldp	x29, x30, [sp], #48
ret
bl	ADDR
mov	w1, #0x2                   	// #2
str	w1, [x0]
b	ADDR
stp	x21, x22, [sp, #32]
add	x22, x19, #0x74
mov	x20, x1
mov	x21, x2
mov	x0, x22
bl	ADDR
add	x19, x19, #0x78
ldr	x0, [x19]
mov	x2, x21
mov	x1, x20
cmp	x0, #0x1
add	x19, x19, x0
csetm	x0, ne	// ne = any
and	x0, x0, x19
bl	ADDR
mov	x0, x22
bl	ADDR
ldp	x19, x20, [sp, #16]
mov	x0, #0x0                   	// #0
ldp	x21, x22, [sp, #32]
b	ADDR
