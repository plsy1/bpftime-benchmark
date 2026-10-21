stp	x29, x30, [sp, #-128]!
mov	x29, sp
stp	x23, x24, [sp, #48]
ldr	x23, [x0]
stp	x19, x20, [sp, #16]
stp	x21, x22, [sp, #32]
mov	x22, x0
stp	x25, x26, [sp, #64]
stp	x27, x28, [sp, #80]
mov	x28, x1
str	x2, [sp, #104]
cmp	x23, #0x4
b.eq	ADDR  // b.none
mov	x2, x1
add	x4, x23, x1
mov	x3, #0x0                   	// #0
mov	x0, #0x0                   	// #0
cbz	x23, ADDR
ldrb	w0, [x2], #1
lsl	x1, x3, #5
sub	x3, x1, x3
add	x3, x0, x3
cmp	x4, x2
b.ne	ADDR  // b.any
mov	x0, x3
ldr	x26, [x22, #24]
mov	x1, x22
ldp	x4, x27, [x22, #8]
umulh	x26, x26, x0
ldr	x0, [x1, #56]!
stp	x4, x1, [sp, #112]
msub	x26, x27, x26, x3
add	x5, x23, x4
add	x1, x0, x1
mov	x25, x27
cmp	x27, x26
sub	x2, x26, x27
csel	x26, x2, x26, ls	// ls = plast
cmp	x0, #0x1
csetm	x21, ne	// ne = any
mov	x19, x26
add	x24, x5, #0x4
and	x21, x21, x1
b	ADDR
cmp	x23, #0x4
b.eq	ADDR  // b.none
bl	ADDR
cbz	w0, ADDR
add	x19, x19, #0x1
cmp	x27, x19
csel	x19, x19, xzr, ne	// ne = any
cmp	x19, x26
b.eq	ADDR  // b.none
mul	x20, x19, x24
mov	x2, x23
mov	x1, x28
add	x4, x20, #0x4
add	x0, x21, x4
ldr	w3, [x21, x20]
cbz	w3, ADDR
cmp	w3, #0x2
b.ne	ADDR  // b.any
cmp	x27, x25
csel	x25, x25, x19, ne	// ne = any
add	x19, x19, #0x1
cmp	x27, x19
csel	x19, x19, xzr, ne	// ne = any
cmp	x19, x26
b.ne	ADDR  // b.any
cmp	x27, x25
b.eq	ADDR  // b.none
ldp	x0, x1, [x22, #32]
cmp	x1, x0
b.cc	ADDR  // b.lo, b.ul, b.last
ldp	x19, x20, [sp, #16]
ldp	x21, x22, [sp, #32]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #128
ret
ldr	w1, [x21, x4]
ldr	w0, [x28]
cmp	w1, w0
b.ne	ADDR  // b.any
add	x0, x23, #0x4
add	x0, x0, x20
ldp	x19, x20, [sp, #16]
add	x0, x21, x0
ldp	x21, x22, [sp, #32]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x1, x2, [sp, #104]
ldp	x29, x30, [sp], #128
b	ADDR
ldp	x0, x1, [x22, #32]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
cmp	x27, x25
b.eq	ADDR  // b.none
mul	x20, x25, x24
mov	x19, x25
add	x20, x20, #0x4
mov	x2, x23
add	x0, x21, x20
mov	x1, x28
bl	ADDR
ldp	x0, x2, [x22]
ldr	x3, [x22, #56]
ldr	x20, [sp, #120]
cmp	x3, #0x1
add	x5, x0, #0x4
add	x0, x0, x2
add	x0, x0, #0x4
csetm	x4, ne	// ne = any
ldr	x1, [sp, #104]
add	x3, x20, x3
and	x3, x4, x3
madd	x0, x0, x19, x5
add	x0, x3, x0
bl	ADDR
ldp	x0, x2, [x22]
mov	w3, #0x1                   	// #1
ldr	x1, [x22, #56]
ldp	x23, x24, [sp, #48]
cmp	x1, #0x1
add	x0, x0, x2
add	x1, x20, x1
add	x0, x0, #0x4
csetm	x2, ne	// ne = any
and	x2, x2, x1
ldr	x1, [x22, #40]
mul	x0, x0, x19
ldp	x19, x20, [sp, #16]
ldp	x25, x26, [sp, #64]
str	w3, [x2, x0]
add	x0, x1, #0x1
str	x0, [x22, #40]
ldp	x21, x22, [sp, #32]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #128
ret
ldrb	w2, [x1, #1]
mov	x4, #0x745f                	// #29791
ldrb	w3, [x1]
mov	w1, #0x1f                  	// #31
ldrb	w7, [x28, #2]
lsl	x0, x2, #4
ldrb	w5, [x28, #3]
sub	x0, x0, x2
add	x0, x2, x0, lsl #6
umaddl	x1, w1, w7, x5
madd	x3, x3, x4, x0
add	x3, x3, x1
mov	x0, x3
b	ADDR
madd	x0, x25, x24, x21
mov	x2, x23
mov	x1, x28
add	x0, x0, #0x4
bl	ADDR
ldp	x0, x2, [x22]
ldr	x3, [x22, #56]
ldr	x19, [sp, #120]
cmp	x3, #0x1
add	x5, x0, #0x4
add	x0, x0, x2
add	x0, x0, #0x4
csetm	x4, ne	// ne = any
ldr	x1, [sp, #104]
add	x3, x19, x3
and	x3, x4, x3
madd	x0, x0, x25, x5
add	x0, x3, x0
bl	ADDR
ldp	x0, x2, [x22]
mov	w3, #0x1                   	// #1
ldr	x1, [x22, #56]
cmp	x1, #0x1
add	x24, x19, x1
add	x0, x0, x2
csetm	x2, ne	// ne = any
add	x0, x0, #0x4
and	x2, x2, x24
ldr	x1, [x22, #40]
mul	x0, x0, x25
str	w3, [x2, x0]
add	x0, x1, #0x1
str	x0, [x22, #40]
b	ADDR
