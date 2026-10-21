stp	x29, x30, [sp, #-96]!
adrp	x2, ADDR
mov	x29, sp
ldr	x2, [x2, #2232]
stp	x19, x20, [sp, #16]
cmp	x2, #0x0
ccmp	w0, #0x0, #0x1, ne	// ne = any
b.ge	ADDR  // b.tcont
bl	ADDR
mov	x19, #0x0                   	// #0
mov	w1, #0x2                   	// #2
str	w1, [x0]
mov	x0, x19
ldp	x19, x20, [sp, #16]
ldp	x29, x30, [sp], #96
ret
ldr	x3, [x2, #16]
cmp	x3, w0, sxtw
b.ls	ADDR  // b.plast
ldr	x3, [x2, #8]
mov	w4, #0xa8                  	// #168
add	x2, x2, #0x8
cmp	x3, #0x1
smull	x0, w0, w4
csetm	x20, ne	// ne = any
add	x2, x2, x3
and	x20, x20, x2
adds	x20, x20, x0
b.eq	ADDR  // b.none
ldrb	w0, [x20, #160]
cmp	w0, #0x1
b.ne	ADDR  // b.any
ldr	w0, [x20, #72]
mov	x19, x1
stp	x21, x22, [sp, #32]
mov	x22, x1
cmp	w0, #0x1
b.eq	ADDR  // b.none
ldp	x21, x22, [sp, #32]
mov	x0, x20
ldp	x19, x20, [sp, #16]
mov	w2, #0x0                   	// #0
ldp	x29, x30, [sp], #96
b	ADDR
add	x21, x20, #0x74
str	x27, [sp, #80]
mov	x0, x21
bl	ADDR
ldr	x0, [x20, #120]
add	x20, x20, #0x78
cmp	x0, #0x1
add	x20, x20, x0
csetm	x4, ne	// ne = any
and	x4, x4, x20
ldr	x27, [x4]
cmp	x27, #0x4
b.eq	ADDR  // b.none
stp	x23, x24, [sp, #48]
add	x2, x19, x27
mov	x0, #0x0                   	// #0
stp	x25, x26, [sp, #64]
cbz	x27, ADDR
ldrb	w1, [x19], #1
lsl	x3, x0, #5
sub	x0, x3, x0
add	x0, x1, x0
cmp	x2, x19
b.ne	ADDR  // b.any
mov	x2, x0
ldr	x24, [x4, #24]
add	x1, x4, #0x38
ldp	x25, x26, [x4, #8]
umulh	x24, x24, x2
ldr	x2, [x4, #56]
msub	x24, x26, x24, x0
add	x0, x1, x2
add	x25, x27, x25
cmp	x26, x24
sub	x1, x24, x26
csel	x24, x1, x24, ls	// ls = plast
cmp	x2, #0x1
csetm	x20, ne	// ne = any
add	x25, x25, #0x4
mov	x19, x24
and	x20, x20, x0
b	ADDR
add	x19, x19, #0x1
cmp	x26, x19
csel	x19, x19, xzr, ne	// ne = any
cmp	x24, x19
b.eq	ADDR  // b.none
mul	x23, x19, x25
ldr	w0, [x20, x23]
cbz	w0, ADDR
cmp	w0, #0x1
b.ne	ADDR  // b.any
add	x0, x23, #0x4
mov	x2, x27
add	x0, x20, x0
mov	x1, x22
bl	ADDR
cbnz	w0, ADDR
add	x0, x27, #0x4
ldp	x25, x26, [sp, #64]
add	x0, x0, x23
ldp	x23, x24, [sp, #48]
add	x19, x20, x0
nop
mov	x0, x21
bl	ADDR
ldp	x21, x22, [sp, #32]
ldr	x27, [sp, #80]
b	ADDR
ldrb	w5, [x19, #1]
mov	x8, #0x745f                	// #29791
ldrb	w0, [x19]
mov	w7, #0x1f                  	// #31
ldrb	w10, [x19, #2]
add	x2, x4, #0x38
lsl	x1, x5, #4
ldrb	w9, [x19, #3]
sub	x1, x1, x5
ldr	w6, [x19]
add	x1, x5, x1, lsl #6
ldp	x5, x3, [x4, #16]
umaddl	x7, w7, w10, x9
madd	x0, x0, x8, x1
ldr	x8, [x4, #56]
add	x0, x0, x7
ldr	x4, [x4, #8]
add	x1, x2, x8
umulh	x3, x0, x3
add	x4, x4, #0x8
msub	x3, x5, x3, x0
cmp	x5, x3
sub	x0, x3, x5
csel	x3, x0, x3, ls	// ls = plast
cmp	x8, #0x1
csetm	x19, ne	// ne = any
mov	x0, x3
and	x19, x19, x1
b	ADDR
cmp	x0, x5
csel	x0, x0, xzr, ne	// ne = any
cmp	x0, x3
b.eq	ADDR  // b.none
mul	x2, x0, x4
add	x0, x0, #0x1
ldr	w1, [x19, x2]
cbz	w1, ADDR
cmp	w1, #0x1
b.ne	ADDR  // b.any
add	x1, x19, x2
ldr	w1, [x1, #4]
cmp	w6, w1
b.ne	ADDR  // b.any
add	x2, x2, #0x8
add	x19, x19, x2
b	ADDR
ldp	x23, x24, [sp, #48]
mov	x19, #0x0                   	// #0
ldp	x25, x26, [sp, #64]
b	ADDR
mov	x19, #0x0                   	// #0
b	ADDR
mov	x2, #0x0                   	// #0
b	ADDR
