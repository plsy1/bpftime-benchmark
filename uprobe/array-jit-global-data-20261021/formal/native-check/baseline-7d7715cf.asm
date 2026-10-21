stp	x29, x30, [sp, #-16]!
mov	x29, sp
cmp	w3, #0x2
b.hi	ADDR  // b.pmore
mov	x4, x1
mov	x1, x2
ldr	w2, [x0, #36]
ldr	w5, [x4]
cmp	w2, w5
b.hi	ADDR  // b.pmore
bl	ADDR
mov	w1, #0x7                   	// #7
str	w1, [x0]
mov	x0, #0xffffffffffffffff    	// #-1
b	ADDR
bl	ADDR
mov	w1, #0x16                  	// #22
str	w1, [x0]
mov	x0, #0xffffffffffffffff    	// #-1
b	ADDR
cmp	x3, #0x1
b.eq	ADDR  // b.none
ldr	w3, [x0, #32]
add	x4, x0, #0x8
ldr	x0, [x0, #8]
mov	w2, w3
cmp	x0, #0x1
mul	w3, w3, w5
add	x0, x4, x0
csetm	x4, ne	// ne = any
and	x4, x4, x0
add	x0, x4, w3, uxtw
cmp	x2, #0x1
b.le	ADDR
bl	ADDR
mov	x0, #0x0                   	// #0
ldp	x29, x30, [sp], #16
ret
b.ne	ADDR  // b.any
ldrb	w0, [x1]
strb	w0, [x4, w3, uxtw]
b	ADDR
bl	ADDR
mov	w1, #0x11                  	// #17
str	w1, [x0]
mov	x0, #0xffffffffffffffff    	// #-1
b	ADDR
