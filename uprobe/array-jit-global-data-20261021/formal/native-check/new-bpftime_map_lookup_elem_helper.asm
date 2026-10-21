stp	x29, x30, [sp, #-48]!
cmp	w0, #0x0
mov	x29, sp
stp	x19, x20, [sp, #16]
adrp	x20, ADDR
add	x20, x20, #0x870
mov	x19, x0
ldr	x0, [x20, #72]
ccmp	x0, #0x0, #0x4, ge	// ge = tcont
b.ne	ADDR  // b.any
bl	ADDR
ldp	x19, x20, [sp, #16]
mov	w1, #0x2                   	// #2
str	w1, [x0]
mov	x0, #0x0                   	// #0
ldp	x29, x30, [sp], #48
ret
str	x21, [sp, #32]
mov	x21, x1
ldr	x1, [x0, #16]
cmp	x1, w19, sxtw
b.ls	ADDR  // b.plast
mov	w1, w19
bl	ADDR
ldrb	w0, [x0, #160]
cmp	w0, #0x1
b.ne	ADDR  // b.any
ldr	x0, [x20, #72]
mov	w1, w19
bl	ADDR
ldrb	w1, [x0, #160]
cmp	w1, #0x1
b.ne	ADDR  // b.any
ldp	x19, x20, [sp, #16]
mov	x1, x21
ldr	x21, [sp, #32]
mov	w2, #0x0                   	// #0
ldp	x29, x30, [sp], #48
b	ADDR
bl	ADDR
ldp	x19, x20, [sp, #16]
mov	w1, #0x2                   	// #2
ldr	x21, [sp, #32]
str	w1, [x0]
mov	x0, #0x0                   	// #0
ldp	x29, x30, [sp], #48
ret
cmp	w1, #0xff
cset	w0, eq	// eq = none
bl	ADDR
