stp	x29, x30, [sp, #-48]!
mov	x29, sp
cmp	w3, #0x2
b.hi	ADDR  // b.pmore
stp	x19, x20, [sp, #16]
mov	x20, x1
mov	x19, x0
stp	x21, x22, [sp, #32]
mov	x22, x3
mov	x21, x2
bl	ADDR
cbz	x20, ADDR
ldr	w2, [x19, #40]
ldr	w1, [x20]
cmp	w1, w2
b.cs	ADDR  // b.hs, b.nlast
cmp	x22, #0x1
b.eq	ADDR  // b.none
ldrsw	x3, [x19, #32]
mov	w1, w1
sxtw	x0, w0
ldr	w2, [x19, #36]
ldr	x4, [x19, #8]
add	x19, x19, #0x8
madd	x3, x3, x1, x0
cmp	x4, #0x1
add	x19, x19, x4
csetm	x1, ne	// ne = any
and	x1, x1, x19
mul	x3, x3, x2
add	x0, x1, x3
cmp	x2, #0x1
b.le	ADDR
mov	x1, x21
bl	ADDR
ldp	x19, x20, [sp, #16]
mov	x0, #0x0                   	// #0
ldp	x21, x22, [sp, #32]
ldp	x29, x30, [sp], #48
ret
bl	ADDR
mov	x1, x0
mov	w2, #0x16                  	// #22
mov	x0, #0xffffffffffffffff    	// #-1
str	w2, [x1]
ldp	x29, x30, [sp], #48
ret
b.ne	ADDR  // b.any
ldrb	w0, [x21]
strb	w0, [x1, x3]
b	ADDR
bl	ADDR
mov	w1, #0x7                   	// #7
str	w1, [x0]
ldp	x19, x20, [sp, #16]
mov	x0, #0xffffffffffffffff    	// #-1
ldp	x21, x22, [sp, #32]
b	ADDR
bl	ADDR
mov	w1, #0x2                   	// #2
str	w1, [x0]
b	ADDR
bl	ADDR
mov	w1, #0x11                  	// #17
str	w1, [x0]
b	ADDR
