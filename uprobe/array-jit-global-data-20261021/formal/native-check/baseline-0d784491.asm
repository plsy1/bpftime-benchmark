stp	x29, x30, [sp, #-96]!
adrp	x4, ADDR
ldr	x4, [x4, #1160]
mov	x29, sp
sub	sp, sp, #0x230
ldr	x5, [x4]
str	x5, [sp, #552]
mov	x5, #0x0                   	// #0
cmp	w3, #0x2
b.hi	ADDR  // b.pmore
str	x20, [sp, #584]
mov	x20, x0
str	x19, [sp, #576]
str	x21, [sp, #592]
str	x22, [sp, #600]
mov	x22, x2
str	x23, [sp, #608]
mov	x23, x1
str	x24, [sp, #616]
bl	ADDR
ldr	w2, [x20, #144]
mov	w21, w0
cmp	w2, #0x4
b.eq	ADDR  // b.none
ldr	x1, [x20, #200]
add	x0, x20, #0xc8
sbfiz	x3, x21, #5, #32
mov	w2, w2
cmp	x1, #0x1
add	x0, x0, x1
csetm	x19, ne	// ne = any
mov	x1, x23
and	x19, x19, x0
add	x19, x19, x3
add	x0, x19, #0x8
ldr	x3, [x19, #8]
cmp	x3, #0x1
add	x0, x0, x3
csetm	x3, ne	// ne = any
and	x0, x3, x0
bl	ADDR
add	x0, sp, #0x1e0
mov	x1, x19
mov	x8, x0
mov	x19, x0
mov	x0, x20
bl	ADDR
ldr	x1, [sp, #480]
cmp	x1, #0x1
add	x1, x19, x1
csetm	x0, ne	// ne = any
ands	x0, x0, x1
b.ne	ADDR  // b.any
str	x25, [sp, #624]
str	x26, [sp, #632]
ldr	w0, [x20, #144]
cmp	w0, #0x4
b.eq	ADDR  // b.none
add	x19, sp, #0x180
add	x1, x20, #0xa0
mov	x0, x19
str	x19, [sp, #48]
str	x27, [sp, #640]
str	x28, [sp, #648]
bl	ADDR
ldr	w3, [x20, #148]
add	x0, sp, #0x188
ldr	x2, [sp, #392]
mov	x1, x22
cmp	x2, #0x1
add	x0, x0, x2
mov	w2, w3
mul	w3, w21, w3
csetm	x4, ne	// ne = any
sbfiz	x21, x21, #5, #32
and	x0, x4, x0
add	x0, x0, x3
bl	ADDR
ldr	x0, [x20, #200]
add	x3, sp, #0x1e0
add	x1, x20, #0xc8
str	x3, [sp, #56]
cmp	x0, #0x1
add	x1, x1, x0
csetm	x2, ne	// ne = any
mov	x0, x3
and	x1, x2, x1
add	x1, x1, x21
bl	ADDR
add	x0, sp, #0x200
mov	x1, x19
str	x0, [sp, #80]
bl	ADDR
ldp	x0, x3, [sp, #488]
add	x22, sp, #0x1e8
cmp	x0, #0x1
add	x0, x0, x22
csetm	x1, ne	// ne = any
and	x1, x1, x0
cmp	x3, #0x4
b.eq	ADDR  // b.none
mov	x4, #0x79b9                	// #31161
mov	x2, #0x615d                	// #24925
movk	x4, #0x9e37, lsl #16
add	x21, x3, x4
movk	x2, #0x9b1a, lsl #16
eor	x21, x21, x21, lsr #32
movk	x2, #0x46af, lsl #32
movk	x2, #0xe98, lsl #48
mul	x21, x21, x2
eor	x21, x21, x21, lsr #32
mul	x21, x21, x2
eor	x21, x21, x21, lsr #28
cbz	x3, ADDR
add	x3, x1, x3
nop
ldrb	w0, [x1], #1
add	x0, x0, x4
add	x0, x0, x21
eor	x0, x0, x0, lsr #32
mul	x0, x0, x2
eor	x0, x0, x0, lsr #32
mul	x21, x0, x2
eor	x21, x21, x21, lsr #28
cmp	x3, x1
b.ne	ADDR  // b.any
add	x0, x20, #0x20
str	x0, [sp, #64]
ldr	x0, [x20, #40]
cmp	x0, #0x1c
b.hi	ADDR  // b.pmore
adrp	x3, ADDR
add	x3, x3, #0x9f8
adrp	x2, ADDR
add	x2, x2, #0xae0
lsr	x1, x21, #32
ldr	x3, [x3, x0, lsl #3]
add	w1, w1, w21
ldr	x0, [x2, x0, lsl #3]
mul	x1, x1, x3
mov	w0, w0
umulh	x1, x1, x0
ldr	x19, [sp, #64]
add	x24, sp, #0x120
mov	x8, x24
str	x24, [sp, #72]
mov	x0, x19
bl	ADDR
ldp	x4, x3, [sp, #288]
mov	x5, x20
ldr	x0, [x19, #16]
add	x26, sp, #0x148
ldr	x2, [x5, #56]!
str	x5, [sp, #160]
cmp	x4, #0x1
add	x23, sp, #0x158
csetm	x27, ne	// ne = any
cmp	x3, #0x1
csetm	x25, ne	// ne = any
cmp	x2, #0x1
csetm	x1, ne	// ne = any
sub	x5, x5, x26
and	x1, x1, x5
add	x0, x2, x0, lsl #3
add	x0, x0, x1
sub	x1, x26, x23
cmp	x0, #0x1
add	x19, sp, #0x168
csetm	x2, ne	// ne = any
add	x6, sp, #0x1a0
and	x2, x2, x1
sub	x5, x23, x6
add	x2, x2, x0
sub	x0, x24, x19
cmp	x2, #0x1
and	x0, x27, x0
csetm	x1, ne	// ne = any
add	x0, x0, x4
and	x1, x1, x5
cmp	x0, #0x1
add	x1, x1, x2
add	x5, sp, #0x128
add	x28, sp, #0x170
csetm	x2, ne	// ne = any
cmp	x1, #0x1
str	x5, [sp, #144]
sub	x5, x5, x28
str	x0, [sp, #360]
add	x0, x19, x0
add	x1, x6, x1
and	x5, x25, x5
and	x0, x2, x0
csetm	x2, ne	// ne = any
add	x5, x5, x3
and	x1, x2, x1
mov	x2, #0x1                   	// #1
str	x6, [sp]
str	x2, [sp, #224]
str	x5, [sp, #368]
cmp	x0, x1
b.eq	ADDR  // b.none
ldr	x2, [x0]
add	x24, sp, #0xe0
sub	x1, x0, x24
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, x2
str	x0, [sp, #224]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldp	x1, x2, [sp, #488]
str	x26, [sp, #16]
mov	x26, x25
mov	x25, x24
stp	x23, x19, [sp, #32]
mov	x23, x3
mov	x19, x4
str	x28, [sp, #88]
mov	x28, x20
cmp	x1, #0x1
add	x22, x22, x1
csetm	x5, ne	// ne = any
mov	x20, x27
and	x5, x5, x22
mov	x24, x2
mov	x22, x0
mov	x27, x5
add	x22, x25, x22
add	x0, x22, #0x8
ldr	x1, [x0, #16]
cmp	x24, x1
b.ne	ADDR  // b.any
cmp	x24, #0x4
b.eq	ADDR  // b.none
cbz	x24, ADDR
ldr	x7, [x22, #16]
add	x1, x0, #0x8
mov	x2, x24
mov	x0, x27
cmp	x7, #0x1
add	x1, x1, x7
csetm	x7, ne	// ne = any
and	x1, x7, x1
bl	ADDR
cbnz	w0, ADDR
ldr	x1, [sp, #224]
mov	x24, x25
mov	x4, x19
mov	x27, x20
mov	x3, x23
mov	x25, x26
ldr	x26, [sp, #16]
mov	x20, x28
ldp	x23, x19, [sp, #32]
ldr	x28, [sp, #88]
cmp	x1, #0x1
b.eq	ADDR  // b.none
ldr	x2, [sp, #72]
sub	x0, x24, x26
add	x0, x0, x1
add	x6, sp, #0x160
ldr	x1, [sp, #144]
sub	x2, x2, x23
and	x2, x27, x2
cmp	x0, #0x1
sub	x1, x1, x6
add	x2, x2, x4
and	x1, x25, x1
sub	x26, x26, x19
add	x1, x1, x3
csetm	x4, ne	// ne = any
cmp	x2, #0x1
add	x5, sp, #0x178
csetm	x3, ne	// ne = any
sub	x23, x23, x28
cmp	x1, #0x1
and	x4, x4, x26
add	x4, x4, x0
sub	x6, x6, x5
and	x3, x3, x23
csetm	x0, ne	// ne = any
ldr	x8, [sp]
add	x2, x3, x2
cmp	x4, #0x1
and	x0, x0, x6
add	x0, x0, x1
csetm	x6, ne	// ne = any
cmp	x2, #0x1
add	x1, sp, #0x1b0
csetm	x3, ne	// ne = any
add	x7, sp, #0x1a8
cmp	x0, #0x1
sub	x5, x5, x1
sub	x7, x28, x7
sub	x19, x19, x8
csetm	x1, ne	// ne = any
and	x6, x6, x19
and	x3, x3, x7
and	x1, x1, x5
add	x4, x6, x4
add	x2, x3, x2
add	x0, x1, x0
stp	x4, x2, [sp, #416]
str	x0, [sp, #432]
strb	wzr, [sp, #440]
ldr	x0, [sp, #80]
bl	ADDR
ldr	x0, [sp, #56]
bl	ADDR
ldr	x0, [sp, #48]
bl	ADDR
ldr	x25, [sp, #624]
ldr	x26, [sp, #632]
ldr	x27, [sp, #640]
ldr	x28, [sp, #648]
b	ADDR
ldr	x1, [x0, #48]
add	x0, x0, #0x30
cmp	x1, #0x1
add	x0, x0, x1
csetm	x1, ne	// ne = any
and	x0, x1, x0
cbz	x0, ADDR
ldr	w3, [x20, #148]
mov	x1, x22
mov	w2, w3
mul	w3, w21, w3
add	x0, x0, x3
bl	ADDR
ldr	x19, [sp, #576]
mov	x0, #0x0                   	// #0
ldr	x20, [sp, #584]
ldr	x21, [sp, #592]
ldr	x22, [sp, #600]
ldr	x23, [sp, #608]
ldr	x24, [sp, #616]
adrp	x1, ADDR
ldr	x1, [x1, #1160]
ldr	x3, [sp, #552]
ldr	x2, [x1]
subs	x3, x3, x2
mov	x2, #0x0                   	// #0
b.ne	ADDR  // b.any
add	sp, sp, #0x230
ldp	x29, x30, [sp], #96
ret
ldr	x1, [x22, #16]
add	x0, x0, #0x8
ldr	w2, [x27]
cmp	x1, #0x1
add	x0, x0, x1
csetm	x1, ne	// ne = any
and	x0, x1, x0
ldr	w0, [x0]
cmp	w2, w0
b.eq	ADDR  // b.none
nop
ldr	x1, [x22]
sub	x22, x22, x25
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x22
add	x22, x0, x1
str	x22, [sp, #224]
cmp	x22, #0x1
b.ne	ADDR  // b.any
ldp	x23, x19, [sp, #32]
mov	x20, x28
ldr	x26, [sp, #16]
ldr	x28, [sp, #88]
mov	x1, #0x1                   	// #1
ldr	x0, [x20, #32]
add	x2, sp, #0x130
ldr	x3, [sp, #64]
cmp	x0, x1
csetm	x25, ne	// ne = any
stp	x3, x1, [sp, #416]
add	x0, x3, x0
and	x25, x25, x0
str	x2, [sp, #152]
cmp	x25, #0x0
sub	x0, x25, x2
sub	x0, x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x0
mov	x0, x25
add	x1, x1, #0x1
str	x1, [sp, #304]
adrp	x24, ADDR
ldr	x24, [x24, #1160]
bl	ADDR
add	x27, sp, #0xd8
add	x22, sp, #0xd0
mov	x5, #0x1                   	// #1
mov	x4, x27
mov	x3, x22
mov	w1, w5
mov	x2, #0x48                  	// #72
mov	x0, x25
stp	x2, xzr, [sp, #208]
bl	ADDR
mov	x24, x0
mov	x0, x25
bl	ADDR
cbz	x24, ADDR
sub	x24, x24, x19
add	x4, sp, #0x1a8
cmp	x24, #0x1
sub	x1, x19, x4
csetm	x0, ne	// ne = any
mov	x3, #0x1                   	// #1
and	x0, x0, x1
str	x4, [sp, #168]
movi	v0.4s, #0x0
add	x0, x0, x24
cmp	x0, x3
str	x0, [sp, #424]
csetm	x2, ne	// ne = any
add	x0, x4, x0
and	x0, x2, x0
ldr	x1, [sp, #56]
str	x3, [x0], #8
adrp	x24, ADDR
ldr	x24, [x24, #1160]
stp	q0, q0, [x0]
stp	q0, q0, [x0, #32]
ldr	x0, [sp, #424]
cmp	x0, x3
add	x2, x4, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
add	x0, x0, #0x8
sub	x2, x0, x26
str	x2, [sp, #328]
cmp	x2, x3
csetm	x25, ne	// ne = any
and	x25, x25, x0
mov	x0, x25
bl	ADDR
ldr	x0, [sp, #512]
add	x1, sp, #0x230
sub	x3, x1, x25
ldr	x1, [sp, #80]
cmp	x0, #0x1
sub	x3, x3, #0x50
add	x1, x1, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
cmp	x0, #0x0
sub	x0, x0, x25
csetm	x1, ne	// ne = any
sub	x0, x0, #0x21
and	x0, x1, x0
add	x0, x0, #0x1
str	x0, [x25, #32]
adrp	x0, ADDR
ldr	x1, [sp, #520]
ldr	q0, [x0, #1296]
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x1
str	x0, [x25, #40]
ldr	x4, [sp, #168]
ldr	q1, [sp, #528]
sub	x2, x4, x19
ldr	x0, [sp, #64]
add	x4, sp, #0x138
str	q1, [x25, #48]
ldr	x3, [sp, #424]
str	x0, [sp, #304]
add	x0, sp, #0x2d0
str	x4, [sp, #184]
cmp	x3, #0x1
sub	x4, x19, x4
csetm	x1, ne	// ne = any
and	x1, x1, x2
stur	q0, [x0, #-200]
add	x1, x1, x3
ldr	x3, [x20, #8]
cmp	x1, #0x1
ldr	x2, [x20, #24]
csetm	x0, ne	// ne = any
and	x0, x0, x4
str	xzr, [sp, #536]
add	x0, x0, x1
add	x1, x3, #0x1
str	x0, [sp, #312]
cmp	x1, x2
b.ls	ADDR  // b.plast
ucvtf	s2, x1
ldr	s3, [x20, #16]
ucvtf	s1, x3
fmov	x1, d0
fdiv	s2, s2, s3
fdiv	s1, s1, s3
fcvtpu	x2, s2
fcvtpu	x0, s1
cmp	x2, #0x0
csel	x2, x2, x1, ne	// ne = any
cmp	x0, #0x0
ccmp	x3, #0x0, #0x4, eq	// eq = none
b.eq	ADDR  // b.none
adrp	x3, ADDR
mov	x1, #0xd                   	// #13
add	x3, x3, #0xae0
mov	x0, #0x0                   	// #0
b	ADDR
add	x0, x0, #0x1
cmp	x0, #0x25
b.eq	ADDR  // b.none
ldr	x1, [x3, x0, lsl #3]
cmp	x2, x1
b.hi	ADDR  // b.pmore
ldr	x2, [sp, #64]
ldr	x0, [x2, #16]
cmp	x0, x1
b.ne	ADDR  // b.any
ldr	x22, [sp, #64]
mov	x0, x21
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x1, [x22, #8]
bl	ADDR
ldr	x8, [sp]
add	x1, sp, #0xa8
fmov	d1, x8
ldr	d0, [sp, #72]
ld1	{v1.d}[1], [x1]
mov	x1, x0
add	x0, sp, #0x90
ld1	{v0.d}[1], [x0]
mov	x0, x22
sub	v0.2d, v1.2d, v0.2d
str	q0, [sp, #16]
bl	ADDR
adrp	x0, ADDR
ldr	q2, [sp, #416]
ldr	q1, [x0, #48]
ldr	q0, [sp, #16]
cmeq	v1.2d, v2.2d, v1.2d
ldr	x0, [sp, #312]
bic	v0.16b, v0.16b, v1.16b
add	v0.2d, v0.2d, v2.2d
str	q0, [sp, #288]
cmp	x0, #0x1
ldr	x1, [sp, #184]
add	x24, sp, #0x118
ldr	x27, [sp, #72]
csetm	x21, ne	// ne = any
sub	x1, x1, x24
mov	x22, #0x1                   	// #1
and	x21, x21, x1
mov	x1, x27
add	x21, x21, x0
mov	x0, x19
str	x21, [sp, #280]
str	x22, [sp, #312]
bl	ADDR
cmp	x21, x22
sub	x0, x24, x23
csetm	x2, ne	// ne = any
mov	x1, x19
and	x2, x2, x0
sub	x24, x24, x26
ldr	x25, [sp]
add	x2, x2, x21
str	x2, [sp, #344]
sub	x26, x26, x19
mov	x0, x25
bl	ADDR
ldr	x0, [sp, #64]
mov	x1, x25
bl	ADDR
ldr	x0, [sp, #360]
ldr	x1, [sp, #344]
cmp	x0, x22
add	x2, x19, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
cmp	x1, x22
csetm	x4, ne	// ne = any
add	x1, x23, x1
and	x4, x4, x1
mov	x1, x27
ldr	x5, [x0]
sub	x2, x0, x4
mov	x0, x23
cmp	x5, x22
csetm	x3, ne	// ne = any
and	x2, x3, x2
add	x2, x2, x5
str	x2, [x4]
ldr	x2, [sp, #360]
ldr	x4, [sp, #344]
cmp	x2, x22
csetm	x3, ne	// ne = any
add	x2, x19, x2
and	x3, x3, x2
cmp	x4, x22
sub	x5, x23, x3
csetm	x2, ne	// ne = any
and	x2, x2, x5
add	x2, x2, x4
str	x2, [x3]
ldr	x4, [sp, #280]
ldr	x3, [x20, #8]
cmp	x4, x22
csetm	x2, ne	// ne = any
and	x2, x2, x24
add	x3, x3, x22
add	x2, x2, x4
str	x3, [x20, #8]
str	x2, [sp, #328]
bl	ADDR
ldr	x3, [sp, #328]
mov	x1, x23
mov	x0, x28
cmp	x3, x22
csetm	x2, ne	// ne = any
and	x2, x2, x26
add	x2, x2, x3
str	x2, [sp, #360]
bl	ADDR
mov	x0, x25
mov	x1, x19
bl	ADDR
strb	w22, [sp, #440]
ldr	x0, [sp, #152]
bl	ADDR
b	ADDR
bl	ADDR
mov	w1, #0x16                  	// #22
str	w1, [x0]
mov	x0, #0xffffffffffffffff    	// #-1
b	ADDR
ldr	x0, [x20, #80]
str	x25, [sp, #624]
str	x26, [sp, #632]
ldr	w19, [x23]
cbnz	x0, ADDR
ldr	w23, [x20, #352]
cmn	w23, #0x1
b.eq	ADDR  // b.none
ldr	x0, [x20, #296]
add	x24, x20, #0x128
add	x25, x20, #0x108
cmp	x0, #0x1
add	x0, x24, x0
csetm	x1, ne	// ne = any
and	x0, x1, x0
ldr	w0, [x0, w23, uxtw #2]
str	w0, [x20, #352]
ldr	w5, [x20, #360]
mov	w1, #0x0                   	// #0
ldr	x3, [x20, #264]
ldrsw	x6, [x20, #152]
udiv	w0, w23, w5
cmp	x3, #0x1
add	x4, x25, x3
csetm	x3, ne	// ne = any
and	x3, x3, x4
ldr	w2, [x20, #148]
add	x25, sp, #0x148
ubfiz	x4, x0, #5, #32
msub	w0, w0, w5, w23
add	x3, x3, x4
add	x4, x3, #0x8
mul	x2, x2, x6
ldr	x3, [x3, #8]
cmp	x3, #0x1
add	x4, x4, x3
csetm	x3, ne	// ne = any
and	x3, x3, x4
madd	x3, x0, x2, x3
mov	x0, x3
bl	ADDR
sub	x0, x0, x25
cmp	x0, #0x1
str	x0, [sp, #328]
str	w23, [sp, #336]
csetm	x1, ne	// ne = any
add	x0, x25, x0
ands	x0, x1, x0
b.eq	ADDR  // b.none
ldr	w3, [x20, #148]
mov	x1, x22
str	x27, [sp, #640]
add	x22, x20, #0x140
mov	w2, w3
str	x28, [sp, #648]
add	x28, sp, #0x158
mul	w3, w21, w3
sub	x25, x25, x28
add	x0, x0, x3
bl	ADDR
ldr	x1, [sp, #328]
ldr	x21, [x22, #16]
cmp	x1, #0x1
csetm	x0, ne	// ne = any
ldr	w2, [sp, #336]
and	x0, x0, x25
str	w2, [sp, #352]
add	x0, x0, x1
str	x0, [sp, #344]
cbnz	x21, ADDR
cmp	x0, #0x1
add	x27, sp, #0x1a8
ldr	x3, [x20, #112]
csetm	x1, ne	// ne = any
sub	x28, x28, x27
str	w19, [sp, #416]
and	x1, x1, x28
str	w2, [sp, #432]
add	x0, x1, x0
str	x0, [sp, #424]
add	x23, x20, #0x68
mov	w22, w19
add	x25, x20, #0x48
cmp	x3, #0x1c
b.hi	ADDR  // b.pmore
adrp	x1, ADDR
add	x1, x1, #0x9f8
adrp	x0, ADDR
add	x0, x0, #0xae0
ldr	x1, [x1, x3, lsl #3]
ldr	x0, [x0, x3, lsl #3]
mul	x1, x22, x1
mov	w0, w0
umulh	x1, x1, x0
add	x26, sp, #0x130
mov	x0, x23
mov	x8, x26
str	x26, [sp, #152]
bl	ADDR
add	x19, sp, #0x168
ldp	x6, x5, [sp, #304]
add	x8, sp, #0x120
ldr	x7, [x20, #128]
add	x2, x20, #0x80
ldr	x0, [x23, #16]
sub	x2, x2, x8
cmp	x6, #0x1
str	x8, [sp, #72]
csetm	x1, ne	// ne = any
cmp	x5, #0x1
csetm	x4, ne	// ne = any
cmp	x7, #0x1
csetm	x3, ne	// ne = any
add	x0, x7, x0, lsl #3
and	x2, x3, x2
sub	x3, x8, x19
add	x0, x0, x2
add	x7, sp, #0x180
cmp	x0, #0x1
add	x8, sp, #0x1e0
csetm	x2, ne	// ne = any
add	x28, sp, #0x138
and	x2, x2, x3
sub	x3, x19, x8
add	x2, x2, x0
sub	x0, x26, x7
cmp	x2, #0x1
and	x1, x1, x0
csetm	x0, ne	// ne = any
add	x1, x1, x6
and	x0, x0, x3
cmp	x1, #0x1
add	x0, x0, x2
add	x26, sp, #0x188
csetm	x3, ne	// ne = any
sub	x2, x28, x26
cmp	x0, #0x1
and	x2, x4, x2
add	x6, x7, x1
add	x0, x8, x0
csetm	x4, ne	// ne = any
add	x2, x2, x5
and	x3, x3, x6
mov	x5, #0x1                   	// #1
and	x0, x4, x0
str	x7, [sp, #48]
str	x8, [sp, #56]
str	x5, [sp, #272]
str	x1, [sp, #384]
str	x2, [sp, #392]
cmp	x3, x0
b.eq	ADDR  // b.none
ldr	x1, [x3]
add	x2, sp, #0x110
sub	x3, x3, x2
cmp	x1, x5
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x1
str	x0, [sp, #272]
cmp	x0, x5
b.eq	ADDR  // b.none
ldr	w4, [sp, #416]
b	ADDR
ldr	x3, [x2, x0]
sub	x1, x1, x2
cmp	x3, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, x3
str	x0, [sp, #272]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x1, x2, x0
ldr	w3, [x1, #8]
cmp	w4, w3
b.ne	ADDR  // b.any
ldr	x0, [sp, #272]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x0, [x20, #296]
ldr	w1, [sp, #336]
cmp	x0, #0x1
add	x24, x24, x0
csetm	x0, ne	// ne = any
ldr	w2, [x20, #352]
and	x0, x0, x24
ldr	x25, [sp, #624]
str	w2, [x0, w1, uxtw #2]
ldr	x26, [sp, #632]
ldr	x27, [sp, #640]
ldr	x28, [sp, #648]
str	w1, [x20, #352]
b	ADDR
ldr	w21, [x1]
b	ADDR
ldr	w0, [x20, #156]
ldr	w23, [x20, #356]
cmp	w23, w0
b.cs	ADDR  // b.hs, b.nlast
ldr	w0, [x20, #360]
add	w1, w23, #0x1
str	w1, [x20, #356]
add	x1, sp, #0x1e0
str	x1, [sp, #56]
add	x25, x20, #0x108
udiv	w1, w23, w0
msub	w1, w1, w0, w23
cbz	w1, ADDR
add	x0, x20, #0x120
add	x24, x20, #0x128
ldr	x2, [x20, #296]
ldp	x3, x4, [x0, #16]
cmp	x2, #0x1
csetm	x1, ne	// ne = any
add	x2, x24, x2
and	x1, x1, x2
add	x1, x1, x3, lsl #2
cmp	x3, x4
b.eq	ADDR  // b.none
ldr	x5, [sp, #56]
cmp	x1, #0x0
csetm	x2, ne	// ne = any
mov	w4, #0xffffffff            	// #-1
sub	x1, x1, x5
add	x3, x3, #0x1
sub	x1, x1, #0x1
and	x1, x2, x1
cmp	x1, #0x0
add	x1, x1, #0x1
add	x1, x5, x1
csetm	x2, ne	// ne = any
and	x1, x2, x1
str	w4, [x1]
str	x3, [x0, #16]
b	ADDR
ldr	x0, [x20, #328]
sub	x21, x21, #0x1
add	x26, x20, #0x148
add	x1, sp, #0x180
cmp	x0, #0x1
add	x21, x21, x21, lsl #1
csetm	x25, ne	// ne = any
add	x0, x0, x26
and	x25, x25, x0
lsl	x21, x21, #3
add	x2, x25, x21
str	x1, [sp, #48]
sub	x4, x2, x1
add	x23, x2, #0x8
ldr	x3, [x25, x21]
strb	wzr, [sp, #400]
cmp	x3, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x4
add	x0, x0, x3
stp	x0, xzr, [sp, #384]
ldrb	w0, [x2, #16]
tbnz	w0, #0, ADDR
mov	x23, #0x1                   	// #1
str	x23, [x25, x21]
add	x21, sp, #0x1e0
ldr	x0, [x22, #16]
ldr	x1, [x20, #328]
add	x0, x0, x0, lsl #1
cmp	x1, x23
add	x1, x26, x1
csetm	x2, ne	// ne = any
and	x1, x2, x1
add	x0, x1, x0, lsl #3
sub	x0, x0, #0x18
cmp	x0, #0x0
sub	x0, x0, x21
csetm	x1, ne	// ne = any
sub	x0, x0, #0x1
and	x0, x1, x0
add	x0, x0, x23
str	x0, [sp, #480]
cmp	x0, x23
add	x0, x21, x0
csetm	x1, ne	// ne = any
and	x0, x1, x0
bl	ADDR
ldr	x0, [sp, #384]
dup	v0.2d, x23
ldr	x3, [sp, #48]
cmp	x0, x23
ldr	x2, [x22, #16]
csetm	x1, ne	// ne = any
add	x3, x3, x0
and	x1, x1, x3
sub	x2, x2, #0x1
str	x2, [x22, #16]
add	x4, x1, #0x10
sub	x28, x28, x4
str	w19, [x1, #8]
ldr	x5, [sp, #344]
cmp	x5, x23
csetm	x2, ne	// ne = any
and	x2, x2, x28
add	x2, x2, x5
str	x2, [x1, #16]
ldr	w1, [sp, #352]
str	w1, [x4, #8]
stp	x23, xzr, [sp, #432]
str	q0, [sp, #416]
stp	x23, xzr, [sp, #448]
str	xzr, [sp, #464]
cmp	x0, x23
b.eq	ADDR  // b.none
sub	x0, x3, x21
str	x21, [sp, #56]
cmp	x0, #0x1
add	x23, x20, #0x68
csetm	x21, ne	// ne = any
add	x25, x20, #0x48
and	x21, x21, x3
ldr	x1, [x23, #8]
ldr	w0, [x21, #8]
str	x0, [sp]
bl	ADDR
add	x1, sp, #0x130
add	x19, sp, #0x168
mov	x2, x1
mov	x1, x0
mov	x8, x2
mov	x0, x23
str	x2, [sp, #152]
add	x27, sp, #0x138
bl	ADDR
add	x28, sp, #0x170
ldp	x6, x5, [sp, #304]
add	x2, sp, #0x118
ldr	x0, [x23, #16]
add	x3, x20, #0x80
ldr	x7, [x20, #128]
sub	x3, x3, x2
cmp	x6, #0x1
add	x9, sp, #0x120
csetm	x1, ne	// ne = any
cmp	x5, #0x1
csetm	x4, ne	// ne = any
cmp	x7, #0x1
add	x0, x7, x0, lsl #3
csetm	x7, ne	// ne = any
and	x3, x7, x3
sub	x7, x27, x28
add	x0, x0, x3
sub	x3, x2, x9
cmp	x0, #0x1
and	x4, x4, x7
csetm	x2, ne	// ne = any
add	x4, x4, x5
and	x2, x2, x3
str	x9, [sp, #72]
add	x2, x2, x0
ldr	x0, [sp, #152]
cmp	x2, #0x1
sub	x0, x0, x19
and	x1, x1, x0
csetm	x0, ne	// ne = any
add	x1, x1, x6
stp	x1, x4, [sp, #360]
cmp	x1, #0x1
ldr	x6, [sp, #56]
add	x5, x19, x1
sub	x3, x9, x6
and	x0, x0, x3
csetm	x3, ne	// ne = any
add	x0, x0, x2
and	x3, x3, x5
cmp	x0, #0x1
add	x0, x6, x0
csetm	x2, ne	// ne = any
and	x0, x2, x0
mov	x2, #0x1                   	// #1
str	x2, [sp, #272]
cmp	x3, x0
b.eq	ADDR  // b.none
ldr	x1, [x3]
add	x2, sp, #0x110
sub	x3, x3, x2
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x1
str	x0, [sp, #272]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	w4, [x21, #8]
b	ADDR
ldr	x3, [x2, x0]
sub	x1, x1, x2
cmp	x3, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, x3
str	x0, [sp, #272]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x1, x2, x0
ldr	w3, [x1, #8]
cmp	w4, w3
b.ne	ADDR  // b.any
ldr	x0, [sp, #272]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x21, [sp, #72]
add	x23, sp, #0x1a0
ldr	x1, [sp, #152]
sub	x2, x2, x21
add	x2, x2, x0
mov	x0, x19
str	x2, [sp, #288]
bl	ADDR
ldr	x3, [sp, #288]
add	x0, sp, #0x1e8
ldr	x25, [sp, #56]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
mov	x1, x19
sub	x4, x21, x25
mov	x19, x0
and	x2, x2, x4
add	x21, sp, #0x1c0
add	x2, x2, x3
str	x2, [sp, #480]
bl	ADDR
ldr	x1, [sp, #48]
mov	x0, x21
bl	ADDR
strb	wzr, [sp, #440]
ldp	x5, x3, [sp, #480]
add	x4, sp, #0x1a8
ldr	x2, [sp, #496]
sub	x0, x25, x23
sub	x8, x19, x4
add	x1, sp, #0x1b0
ldr	x11, [sp, #448]
cmp	x5, #0x1
csetm	x7, ne	// ne = any
cmp	x3, #0x1
and	x7, x7, x0
csetm	x6, ne	// ne = any
add	x0, sp, #0x1f0
cmp	x2, #0x1
add	x7, x7, x5
and	x6, x6, x8
csetm	x5, ne	// ne = any
sub	x8, x0, x1
add	x6, x6, x3
cmp	x7, #0x1
and	x5, x5, x8
csetm	x12, ne	// ne = any
add	x5, x5, x2
cmp	x6, #0x1
csetm	x3, ne	// ne = any
cmp	x5, #0x1
add	x8, sp, #0x200
csetm	x2, ne	// ne = any
cmp	x11, #0x1
sub	x9, x4, x19
sub	x10, x23, x25
sub	x4, x1, x0
csetm	x1, ne	// ne = any
str	x8, [sp, #80]
sub	x8, x21, x8
and	x2, x2, x4
and	x1, x1, x8
and	x3, x3, x9
and	x0, x12, x10
add	x4, x3, x6
ldrb	w8, [sp, #464]
add	x3, x2, x5
add	x0, x0, x7
add	x2, x1, x11
mov	w1, #0x0                   	// #0
stp	x7, x6, [sp, #416]
str	x5, [sp, #432]
b	ADDR
ldr	x1, [x20, #112]
mov	w0, w19
add	x24, x20, #0x68
bl	ADDR
mov	x1, x0
add	x0, sp, #0x180
mov	x2, x0
mov	x0, x24
mov	x8, x2
mov	x24, x2
str	x2, [sp, #48]
bl	ADDR
ldr	x0, [sp, #384]
add	x3, sp, #0x158
cmp	x0, #0x1
add	x1, x24, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
sub	x1, x0, x3
ldr	x0, [x0]
cmp	x0, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x1
add	x2, x2, x0
str	x2, [sp, #344]
cmp	x2, #0x1
b.eq	ADDR  // b.none
add	x0, x3, x2
ldr	w1, [x0, #8]
cmp	w19, w1
b.ne	ADDR  // b.any
add	x19, sp, #0x168
add	x24, sp, #0x1a0
ldr	x1, [sp, #48]
sub	x3, x3, x19
add	x2, x3, x2
mov	x0, x24
str	x2, [sp, #360]
add	x25, sp, #0x1e0
sub	x19, x19, x25
bl	ADDR
ldr	x3, [sp, #360]
mov	x1, x24
add	x0, sp, #0x1e8
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x19
add	x2, x2, x3
str	x2, [sp, #480]
bl	ADDR
ldr	x1, [sp, #480]
cmp	x1, #0x1
add	x1, x25, x1
csetm	x0, ne	// ne = any
ands	x0, x0, x1
b.eq	ADDR  // b.none
ldr	x1, [x0, #16]
add	x0, x0, #0x10
ldr	x25, [sp, #624]
cmp	x1, #0x1
add	x1, x0, x1
csetm	x0, ne	// ne = any
ldr	x26, [sp, #632]
and	x0, x0, x1
b	ADDR
ldr	x0, [x25, #8]
ldr	x1, [x25, #24]
add	x0, x0, #0x1
cmp	x0, x1
b.hi	ADDR  // b.pmore
ldr	x3, [sp, #384]
add	x21, sp, #0x110
ldr	x0, [sp, #48]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
sub	x0, x0, x21
ldr	x1, [x23, #8]
and	x2, x2, x0
ldr	x0, [sp]
add	x2, x2, x3
str	x2, [sp, #272]
bl	ADDR
ldr	x8, [sp, #56]
add	x2, sp, #0x1e8
ldr	d0, [sp, #152]
fmov	d2, x8
mov	x1, x0
mov	x0, x23
mov	v0.d[1], x27
mov	v2.d[1], x2
mov	x27, x2
sub	v2.2d, v2.2d, v0.2d
str	q2, [sp]
bl	ADDR
adrp	x0, ADDR
ldr	q3, [sp, #480]
ldr	q1, [x0, #48]
mov	x0, x19
ldr	q2, [sp]
cmeq	v0.2d, v3.2d, v1.2d
ldr	x1, [sp, #152]
bic	v0.16b, v2.16b, v0.16b
add	v0.2d, v0.2d, v3.2d
str	q0, [sp, #304]
bl	ADDR
sub	x7, x28, x27
ldp	x1, x5, [sp, #360]
ldr	x6, [sp, #272]
ldr	x8, [sp, #56]
cmp	x6, #0x1
ldr	x0, [sp, #72]
csetm	x4, ne	// ne = any
cmp	x1, #0x1
csetm	x3, ne	// ne = any
cmp	x5, #0x1
sub	x21, x21, x0
csetm	x2, ne	// ne = any
sub	x0, x19, x8
and	x2, x2, x7
and	x3, x3, x0
and	x4, x4, x21
add	x4, x4, x6
add	x2, x2, x5
add	x3, x3, x1
mov	x0, x23
mov	x1, x8
mov	x23, x8
str	x4, [sp, #288]
stp	x3, x2, [sp, #480]
bl	ADDR
ldr	x2, [sp, #360]
ldr	x0, [sp, #288]
cmp	x2, #0x1
add	x3, x19, x2
csetm	x2, ne	// ne = any
and	x2, x2, x3
cmp	x0, #0x1
ldr	x28, [sp, #72]
csetm	x4, ne	// ne = any
ldr	x5, [x2]
add	x0, x28, x0
and	x4, x4, x0
mov	x0, x19
cmp	x5, #0x1
sub	x2, x2, x4
csetm	x3, ne	// ne = any
and	x2, x3, x2
add	x2, x2, x5
str	x2, [x4]
ldr	x1, [sp, #152]
ldr	x2, [sp, #360]
ldr	x4, [sp, #288]
cmp	x2, #0x1
csetm	x3, ne	// ne = any
add	x2, x19, x2
and	x3, x3, x2
cmp	x4, #0x1
sub	x5, x28, x3
csetm	x2, ne	// ne = any
and	x2, x2, x5
add	x2, x2, x4
str	x2, [x3]
adrp	x2, ADDR
ldr	x4, [sp, #272]
ldr	x3, [x25, #8]
cmp	x4, #0x1
ldr	q1, [x2, #48]
csetm	x2, ne	// ne = any
and	x2, x2, x21
add	x3, x3, #0x1
add	x2, x2, x4
str	x3, [x25, #8]
str	x2, [sp, #288]
str	d1, [sp, #384]
bl	ADDR
ldr	x3, [sp, #288]
sub	x4, x28, x23
mov	x1, x19
mov	x0, x27
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x4
add	x2, x2, x3
str	x2, [sp, #480]
bl	ADDR
ldp	x2, x8, [sp, #480]
add	x3, sp, #0x1a0
ldr	x4, [sp, #496]
sub	x0, x23, x3
ldr	x11, [sp, #448]
add	x1, sp, #0x1a8
cmp	x2, #0x1
sub	x10, x27, x1
csetm	x7, ne	// ne = any
cmp	x8, #0x1
and	x7, x7, x0
csetm	x6, ne	// ne = any
cmp	x4, #0x1
add	x7, x7, x2
add	x9, sp, #0x1f0
add	x0, sp, #0x1b0
csetm	x5, ne	// ne = any
and	x6, x6, x10
cmp	x11, #0x1
add	x6, x6, x8
sub	x10, x9, x0
csetm	x2, ne	// ne = any
cmp	x7, #0x1
and	x5, x5, x10
csetm	x10, ne	// ne = any
cmp	x6, #0x1
add	x5, x5, x4
sub	x1, x1, x27
csetm	x4, ne	// ne = any
sub	x8, x3, x23
and	x4, x4, x1
add	x1, sp, #0x200
str	x1, [sp, #80]
adrp	x1, ADDR
cmp	x5, #0x1
sub	x0, x0, x9
ldr	q1, [x1, #48]
and	x8, x10, x8
csetm	x3, ne	// ne = any
and	x2, x2, #0xffffffffffffffc0
and	x3, x3, x0
add	x0, x8, x7
fmov	w1, s1
ldrb	w8, [sp, #464]
add	x2, x2, x11
add	x4, x4, x6
add	x3, x3, x5
stp	x7, x6, [sp, #416]
str	x5, [sp, #432]
str	b1, [sp, #440]
stp	x0, x4, [sp, #480]
str	x3, [sp, #496]
strb	w1, [sp, #504]
str	x2, [sp, #512]
str	xzr, [sp, #520]
strb	wzr, [sp, #528]
tbnz	w8, #0, ADDR
and	w19, w1, #0x1
tbnz	w1, #0, ADDR
ldp	x0, x3, [x22, #16]
ldr	x1, [x20, #328]
cmp	x1, #0x1
add	x26, x26, x1
csetm	x2, ne	// ne = any
add	x1, x0, x0, lsl #1
and	x2, x2, x26
add	x1, x2, x1, lsl #3
cmp	x0, x3
b.eq	ADDR  // b.none
add	x2, sp, #0x1a0
cmp	x1, #0x0
sub	x0, x1, x2
csetm	x1, ne	// ne = any
sub	x0, x0, #0x1
and	x1, x1, x0
add	x1, x1, #0x1
str	x1, [sp, #416]
cmp	x1, #0x1
add	x2, x2, x1
ldr	x4, [sp, #512]
csetm	x0, ne	// ne = any
and	x2, x0, x2
ldr	x0, [sp, #80]
cmp	x4, #0x1
mov	x19, x2
csetm	x3, ne	// ne = any
sub	x0, x0, x2
and	x3, x3, x0
add	x1, x3, x4
str	x1, [x2]
str	xzr, [x19, #8]!
strb	wzr, [x19, #8]
ldrb	w0, [sp, #528]
tbnz	w0, #0, ADDR
ldr	x0, [x22, #16]
ldrb	w1, [sp, #504]
add	x0, x0, #0x1
str	x0, [x22, #16]
and	w19, w1, #0x1
ldr	x0, [sp, #384]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x3, [sp, #392]
add	x4, sp, #0x188
ldr	x1, [sp, #48]
cmp	x3, #0x1
add	x4, x4, x3
csetm	x3, ne	// ne = any
add	x2, x1, x0
and	x3, x3, x4
add	x1, sp, #0x1a0
cmp	x3, #0x0
add	x0, sp, #0x168
sub	x2, x2, x1
csetm	x5, ne	// ne = any
add	x6, sp, #0x1e8
cmp	x2, #0x1
sub	x3, x3, x0
sub	x3, x3, #0x1
csetm	x4, ne	// ne = any
sub	x7, x1, x6
and	x5, x5, x3
and	x3, x4, x7
add	x4, x5, #0x1
add	x2, x3, x2
str	x4, [sp, #360]
stp	x0, x2, [sp, #480]
cmp	x2, #0x1
b.ne	ADDR  // b.any
cbz	w19, ADDR
ldr	x25, [sp, #624]
ldr	x26, [sp, #632]
ldr	x27, [sp, #640]
ldr	x28, [sp, #648]
b	ADDR
bl	ADDR
mov	w1, #0x7                   	// #7
str	w1, [x0]
ldr	x19, [sp, #576]
mov	x0, #0xffffffffffffffff    	// #-1
ldr	x20, [sp, #584]
ldr	x21, [sp, #592]
ldr	x22, [sp, #600]
ldr	x23, [sp, #608]
ldr	x24, [sp, #616]
ldr	x25, [sp, #624]
ldr	x26, [sp, #632]
b	ADDR
ldr	x0, [x20, #32]
add	x5, sp, #0x108
mov	x4, #0x1                   	// #1
str	x5, [sp, #112]
cmp	x0, #0x1
add	x2, x2, x0
csetm	x0, ne	// ne = any
stp	x4, x4, [sp, #440]
and	x0, x0, x2
cmp	x0, #0x0
sub	x0, x0, x5
csetm	x2, ne	// ne = any
sub	x0, x0, #0x1
and	x0, x2, x0
adrp	x3, ADDR
add	x0, x0, x4
add	x3, x3, #0xae0
cmp	x0, x4
add	x5, x5, x0
csetm	x4, ne	// ne = any
mov	x2, #0xd                   	// #13
and	x4, x4, x5
str	x0, [sp, #264]
ldr	x5, [sp]
cmp	x4, #0x0
mov	x0, #0x0                   	// #0
sub	x4, x4, x5
csetm	x5, ne	// ne = any
sub	x4, x4, #0x1
and	x4, x5, x4
add	x4, x4, #0x1
str	x4, [sp, #416]
b	ADDR
add	x0, x0, #0x1
cmp	x0, #0x25
b.eq	ADDR  // b.none
ldr	x2, [x3, x0, lsl #3]
cmp	x2, x1
b.cc	ADDR  // b.lo, b.ul, b.last
add	x6, x2, #0x1
mov	x3, #0xffffffff            	// #4294967295
orr	x5, x6, #0x8
mov	x1, #0x1fffffffffffffff    	// #2305843009213693951
cmp	x5, x3
lsr	x3, x2, #6
cset	w5, hi	// hi = pmore
cmp	x6, x1
cset	w1, hi	// hi = pmore
str	x6, [sp, #16]
and	w5, w5, w1
str	x3, [sp, #32]
add	x3, x3, #0x1
str	x3, [sp, #96]
ldr	x1, [sp]
cmp	x4, #0x1
add	x3, sp, #0x110
add	x25, sp, #0x118
add	x4, x1, x4
csetm	x1, ne	// ne = any
and	x1, x1, x4
str	x3, [sp, #104]
cmp	x1, #0x0
sub	x3, x1, x3
sub	x1, x1, x25
sub	x3, x3, #0x1
sub	x1, x1, #0x1
stp	x0, x2, [sp, #424]
csetm	x0, ne	// ne = any
and	x2, x3, x0
and	x0, x1, x0
add	x0, x0, #0x1
add	x1, x2, #0x1
stp	x1, x0, [sp, #272]
cbnz	w5, ADDR
ldr	x2, [sp, #16]
cmp	x1, #0x1
ldr	x0, [sp, #104]
lsl	x2, x2, #3
str	x2, [sp, #88]
add	x1, x0, x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
str	x0, [sp, #40]
adrp	x24, ADDR
ldr	x24, [x24, #1160]
bl	ADDR
ldr	x0, [sp, #40]
mov	x5, #0x1                   	// #1
ldr	x2, [sp, #88]
mov	x4, x27
mov	w1, w5
mov	x3, x22
stp	x2, xzr, [sp, #208]
bl	ADDR
mov	x24, x0
ldr	x0, [sp, #40]
bl	ADDR
cbz	x24, ADDR
ldr	x0, [sp, #280]
sub	x24, x24, x19
add	x1, sp, #0x1b8
cmp	x24, #0x1
mov	x2, x1
csetm	x1, ne	// ne = any
cmp	x0, #0x1
str	x2, [sp, #40]
sub	x2, x19, x2
add	x0, x25, x0
and	x1, x1, x2
csetm	x2, ne	// ne = any
and	x0, x2, x0
add	x1, x1, x24
ldr	x2, [sp, #96]
str	x0, [sp, #120]
adrp	x24, ADDR
ldr	x24, [x24, #1160]
str	x1, [sp, #440]
lsl	x2, x2, #5
str	x2, [sp, #96]
bl	ADDR
ldr	x2, [sp, #96]
mov	x3, x22
ldr	x24, [sp, #120]
mov	x5, #0x1                   	// #1
mov	x4, x27
mov	w1, w5
mov	x0, x24
stp	x2, xzr, [sp, #208]
bl	ADDR
mov	x22, x0
mov	x0, x24
bl	ADDR
cbz	x22, ADDR
sub	x22, x22, x19
add	x7, sp, #0x1c0
cmp	x22, #0x1
sub	x2, x19, x7
ldr	x0, [sp, #440]
csetm	x1, ne	// ne = any
and	x1, x1, x2
ldr	x2, [sp, #40]
cmp	x0, #0x1
add	x1, x1, x22
str	x1, [sp, #448]
add	x2, x2, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
ldr	x2, [sp, #16]
cbz	x2, ADDR
ldr	x1, [sp, #88]
mov	x2, #0x1                   	// #1
add	x1, x0, x1
str	x2, [x0], #8
cmp	x1, x0
b.ne	ADDR  // b.any
ldr	x0, [sp, #448]
cmp	x0, #0x1
add	x1, x7, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
adrp	x1, ADDR
adrp	x2, ADDR
ldr	q0, [x1, #1296]
ldr	x1, [sp, #96]
ldr	q1, [x2, #48]
add	x1, x0, x1
stp	q0, q1, [x0]
add	x0, x0, #0x20
cmp	x0, x1
b.ne	ADDR  // b.any
ldp	x5, x2, [sp, #440]
sub	x1, x7, x19
sub	x3, x19, x26
add	x27, sp, #0xe8
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
ldr	x1, [sp, #40]
add	x0, x0, x2
sub	x4, x1, x23
ldr	x1, [sp, #32]
add	x2, x0, x1, lsl #5
ldr	x0, [sp, #432]
cmp	x2, #0x1
csetm	x1, ne	// ne = any
cmp	x5, #0x1
and	x1, x1, x3
and	x0, x0, #0xffffffffffffffc0
csetm	x3, ne	// ne = any
add	x1, x1, x2
and	x3, x3, x4
add	x4, x26, x1
add	x0, x3, x0, lsl #3
sub	x3, x23, x19
add	x0, x0, x5
str	x1, [sp, #328]
cmp	x0, #0x1
csetm	x2, ne	// ne = any
cmp	x1, #0x1
and	x1, x2, x3
csetm	x2, ne	// ne = any
add	x0, x1, x0
and	x2, x2, x4
cmp	x0, #0x1
sub	x3, x19, x2
csetm	x1, ne	// ne = any
fmov	x4, d0
and	x1, x1, x3
add	x0, x1, x0
str	x0, [x2]
ldr	x1, [sp, #328]
cmp	x1, #0x1
add	x2, x26, x1
csetm	x0, ne	// ne = any
and	x2, x0, x2
add	x3, x2, #0x18
sub	x3, x26, x3
and	x0, x0, x3
add	x1, x0, x1
cmp	x1, #0x1
ldr	x0, [sp, #432]
cset	x3, ne	// ne = any
str	x1, [x2, #24]
add	x1, x1, x3, lsl #3
str	x1, [x2, #16]
ldr	x1, [sp, #64]
lsl	x0, x4, x0
str	x0, [x2, #8]
ldr	x0, [x20, #56]
ldr	x11, [x1, #16]
cmp	x0, #0x1
ldr	x1, [sp, #160]
csetm	x5, ne	// ne = any
add	x0, x1, x0
and	x5, x5, x0
add	x11, x5, x11, lsl #3
sub	x6, x27, x5
cmp	x5, x11
b.eq	ADDR  // b.none
mov	x22, #0x615d                	// #24925
add	x9, sp, #0x160
movk	x22, #0x9b1a, lsl #16
add	x10, sp, #0x150
movk	x22, #0x46af, lsl #32
mov	x3, x6
mov	x24, x5
sub	x0, x10, x9
movk	x22, #0xe98, lsl #48
stp	x28, x7, [sp, #88]
stp	x0, x9, [sp, #120]
str	x10, [sp, #136]
str	x11, [sp, #176]
stp	x20, x21, [sp, #192]
nop
neg	x0, x3
ldr	x2, [x24]
add	x28, sp, #0xf8
mov	x20, #0x79b9                	// #31161
add	x21, sp, #0xf0
cmp	x2, #0x1
sub	x1, x27, x28
csetm	x7, ne	// ne = any
movk	x20, #0x9e37, lsl #16
and	x0, x0, x7
add	x0, x0, x2
str	x0, [sp, #232]
cmp	x0, #0x1
b.eq	ADDR  // b.none
str	x3, [sp, #16]
str	x1, [sp, #32]
ldr	x5, [x27, x0]
ldr	x1, [sp, #32]
cmp	x5, #0x1
csetm	x3, ne	// ne = any
add	x1, x1, x0
add	x0, x27, x0
cmp	x1, #0x1
add	x2, x1, x28
sub	x0, x0, x21
csetm	x4, ne	// ne = any
and	x3, x3, x0
and	x0, x4, x2
add	x2, x3, x5
stp	x2, x1, [sp, #240]
add	x5, x0, #0x10
ldr	x2, [x0, #16]
ldr	x3, [x0, #24]
cmp	x2, #0x1
add	x0, x5, x2
csetm	x2, ne	// ne = any
and	x2, x2, x0
cmp	x3, #0x4
b.eq	ADDR  // b.none
add	x0, x3, x20
eor	x0, x0, x0, lsr #32
mul	x0, x0, x22
eor	x0, x0, x0, lsr #32
mul	x0, x0, x22
eor	x0, x0, x0, lsr #28
cbz	x3, ADDR
add	x3, x2, x3
nop
ldrb	w5, [x2], #1
add	x5, x5, x20
add	x0, x5, x0
eor	x0, x0, x0, lsr #32
mul	x0, x0, x22
eor	x0, x0, x0, lsr #32
mul	x0, x0, x22
eor	x0, x0, x0, lsr #28
cmp	x3, x2
b.ne	ADDR  // b.any
ldr	x3, [sp, #424]
cmp	x3, #0x1c
b.hi	ADDR  // b.pmore
adrp	x6, ADDR
add	x6, x6, #0x9f8
adrp	x5, ADDR
add	x5, x5, #0xae0
lsr	x2, x0, #32
ldr	x6, [x6, x3, lsl #3]
add	w2, w2, w0
ldr	x0, [x5, x3, lsl #3]
ldp	x5, x3, [sp, #432]
mul	x2, x2, x6
mov	w0, w0
umulh	x2, x2, x0
cmp	x3, #0x1
csetm	x0, ne	// ne = any
cbnz	x5, ADDR
ldr	x2, [sp, #40]
sub	x6, x23, x19
sub	x5, x19, x26
sub	x2, x2, x23
and	x0, x0, x2
add	x0, x0, x3
mov	x3, #0x1                   	// #1
cmp	x0, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x6
mov	x6, x3
add	x2, x2, x0
cmp	x2, x3
csetm	x0, ne	// ne = any
and	x0, x0, x5
add	x0, x0, x2
stp	x0, x3, [sp, #328]
cmp	x0, #0x1
sub	x5, x26, x23
csetm	x2, ne	// ne = any
and	x2, x2, x5
sub	x5, x28, x25
add	x2, x2, x0
sub	x0, x23, x19
cmp	x2, #0x1
str	x3, [sp, #368]
csetm	x3, ne	// ne = any
and	x4, x4, x5
and	x3, x3, x0
add	x4, x4, x1
ldr	x0, [sp]
add	x3, x3, x2
mov	x1, x19
str	x4, [sp, #280]
stp	x2, x6, [sp, #344]
str	x3, [sp, #360]
bl	ADDR
ldr	x0, [sp, #344]
sub	x3, x21, x27
ldr	x1, [sp, #280]
cmp	x0, #0x1
add	x2, x23, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
cmp	x1, #0x1
csetm	x2, ne	// ne = any
add	x1, x25, x1
and	x2, x2, x1
ldr	x4, [x0]
sub	x0, x0, x2
cmp	x4, #0x1
csetm	x1, ne	// ne = any
and	x0, x1, x0
add	x0, x0, x4
str	x0, [x2]
ldr	x0, [sp, #344]
ldr	x2, [sp, #280]
cmp	x0, #0x1
csetm	x1, ne	// ne = any
add	x0, x23, x0
and	x1, x1, x0
cmp	x2, #0x1
sub	x4, x25, x1
csetm	x0, ne	// ne = any
and	x0, x0, x4
add	x0, x0, x2
str	x0, [x1]
ldr	x2, [sp, #16]
ldr	x1, [sp, #240]
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x1
str	x0, [sp, #232]
cmp	x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x2
add	x0, x1, x0
str	x0, [x24]
ldr	x0, [sp, #232]
cmp	x0, #0x1
b.ne	ADDR  // b.any
mov	x3, x2
ldr	x0, [sp, #176]
add	x24, x24, #0x8
sub	x3, x3, #0x8
cmp	x0, x24
b.ne	ADDR  // b.any
ldp	x28, x7, [sp, #88]
ldp	x20, x21, [sp, #192]
str	x7, [sp, #16]
ldr	x22, [sp, #64]
mov	x0, x22
bl	ADDR
adrp	x1, ADDR
add	x0, sp, #0x200
ldr	x7, [sp, #16]
ldr	x2, [sp, #160]
ldr	q0, [x1, #48]
ldr	x1, [sp, #40]
ldur	q1, [x0, #-72]
add	x0, x22, #0x20
sub	x1, x1, x2
sub	x0, x7, x0
add	x2, sp, #0x200
fmov	d2, x1
mov	v2.d[1], x0
add	x0, sp, #0x200
ldur	q3, [x2, #-88]
stp	xzr, xzr, [x2, #-88]
stur	q0, [x0, #-72]
mov	x0, v3.d[1]
cmeq	v0.2d, v1.2d, v0.2d
stur	q3, [x20, #40]
bic	v0.16b, v2.16b, v0.16b
add	v1.2d, v1.2d, v0.2d
stur	q1, [x20, #56]
cbz	x0, ADDR
ldr	s0, [x20, #16]
ucvtf	d2, x0
mov	x0, #0x43f0000000000000    	// #4895412794951729152
fmov	d1, x0
mov	x0, #0xffffffffffffffff    	// #-1
fcvt	d0, s0
fmul	d0, d0, d2
fcmpe	d0, d1
b.ge	ADDR  // b.tcont
fcvtzu	x0, d0
str	x0, [x20, #24]
ldr	x0, [sp]
bl	ADDR
b	ADDR
cmp	x2, x0
csel	x2, x2, x0, cs	// cs = hs, nlast
b	ADDR
sub	x6, x6, x1
add	x2, x6, x2
str	x2, [sp, #416]
bl	ADDR
b	ADDR
mov	x0, x19
add	x1, sp, #0x208
bl	ADDR
mov	w0, #0x1                   	// #1
strb	w0, [x19, #8]
b	ADDR
ucvtf	s0, x0
ldr	s2, [x25, #16]
fmov	s1, #1.000000000000000000e+00
mov	x0, x25
fdiv	s0, s0, s2
frintp	s0, s0
fadd	s0, s0, s1
fcvtzu	x1, s0
bl	ADDR
b	ADDR
ldr	x3, [sp, #424]
ldr	w0, [x2]
cmp	x3, #0x1c
b.ls	ADDR  // b.plast
sub	x3, x3, #0x1d
adrp	x1, ADDR
add	x1, x1, #0x3b8
ldr	x1, [x1, x3, lsl #3]
blr	x1
ldp	x5, x3, [sp, #432]
mov	x2, x0
ldr	x1, [sp, #248]
cmp	x1, #0x1
csetm	x4, ne	// ne = any
cmp	x3, #0x1
csetm	x0, ne	// ne = any
cbz	x5, ADDR
nop
ldr	x5, [sp, #40]
ldr	x7, [sp, #104]
ldr	x8, [sp, #448]
sub	x6, x5, x7
and	x0, x0, x6
add	x5, sp, #0x100
ldr	x6, [sp, #96]
add	x0, x0, x3
sub	x7, x7, x25
sub	x9, x6, x5
lsr	x6, x2, #6
add	x2, x0, x2, lsl #3
cmp	x2, #0x1
csetm	x0, ne	// ne = any
cmp	x8, #0x1
csetm	x3, ne	// ne = any
and	x0, x0, x7
and	x3, x3, x9
add	x0, x0, x2
ldr	x7, [sp, #112]
add	x3, x3, x8
add	x6, x3, x6, lsl #5
sub	x8, x25, x19
cmp	x6, #0x1
sub	x5, x5, x7
csetm	x2, ne	// ne = any
cmp	x0, #0x1
ldr	x9, [sp, #88]
and	x2, x2, x5
add	x2, x2, x6
csetm	x3, ne	// ne = any
cmp	x2, #0x1
sub	x7, x7, x9
csetm	x5, ne	// ne = any
and	x3, x3, x8
and	x5, x5, x7
add	x3, x3, x0
add	x5, x5, x2
cmp	x3, #0x1
ldr	x2, [sp, #136]
csetm	x0, ne	// ne = any
cmp	x5, #0x1
sub	x7, x19, x26
sub	x6, x9, x2
csetm	x2, ne	// ne = any
and	x0, x0, x7
and	x2, x2, x6
ldr	x6, [sp, #128]
add	x2, x2, x5
add	x0, x0, x3
cmp	x2, #0x1
ldr	x3, [sp, #120]
sub	x7, x6, x9
csetm	x6, ne	// ne = any
stp	x0, x2, [sp, #328]
and	x6, x6, x3
add	x6, x6, x2
cmp	x6, #0x1
csetm	x3, ne	// ne = any
and	x3, x3, x7
add	x3, x3, x6
b	ADDR
sub	x1, x0, #0x1d
adrp	x2, ADDR
add	x2, x2, #0x3b8
mov	x0, x21
ldr	x1, [x2, x1, lsl #3]
blr	x1
mov	x1, x0
b	ADDR
add	x1, sp, #0x200
mov	x2, x0
mov	x3, x0
mov	x4, x0
mov	w8, #0x0                   	// #0
str	x1, [sp, #80]
mov	w1, #0x0                   	// #0
b	ADDR
ldr	x0, [x20, #104]
mov	x2, #0x1                   	// #1
adrp	x24, ADDR
ldr	x24, [x24, #1160]
cmp	x0, x2
csetm	x20, ne	// ne = any
add	x0, x23, x0
and	x20, x20, x0
stp	x23, x2, [sp, #480]
cmp	x20, #0x0
ldr	x0, [sp, #72]
csetm	x1, ne	// ne = any
sub	x0, x20, x0
sub	x0, x0, #0x1
and	x1, x1, x0
mov	x0, x20
add	x1, x1, x2
str	x1, [sp, #288]
bl	ADDR
mov	x5, #0x1                   	// #1
mov	w1, w5
mov	x0, x20
mov	x2, #0x20                  	// #32
add	x4, sp, #0xd8
add	x3, sp, #0xd0
stp	x2, xzr, [sp, #208]
bl	ADDR
mov	x1, x0
mov	x0, x20
mov	x20, x1
bl	ADDR
cbz	x20, ADDR
ldr	x4, [sp, #48]
mov	x2, #0x1                   	// #1
add	x24, sp, #0x170
sub	x1, x20, x4
add	x20, sp, #0x1e8
cmp	x1, #0x1
sub	x3, x4, x20
csetm	x0, ne	// ne = any
and	x0, x0, x3
mov	x3, x4
add	x0, x0, x1
str	x0, [sp, #488]
cmp	x0, x2
add	x0, x20, x0
csetm	x1, ne	// ne = any
sub	x5, x3, x24
and	x0, x1, x0
sub	x4, x20, x4
mov	x1, x0
str	x2, [x1], #8
stp	xzr, xzr, [x0, #8]
str	xzr, [x1, #16]
ldr	x0, [sp, #488]
ldr	w6, [sp, #416]
cmp	x0, x2
add	x1, x20, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, #0x8
sub	x1, x0, x3
cmp	x1, x2
csetm	x3, ne	// ne = any
and	x3, x3, x0
mov	x1, x3
str	w6, [x1], #8
ldr	x6, [sp, #424]
sub	x27, x27, x1
cmp	x6, x2
csetm	x0, ne	// ne = any
and	x0, x0, x27
add	x0, x0, x6
str	x0, [x3, #8]
ldr	w0, [sp, #432]
ldr	x3, [x25, #8]
ldr	x7, [x25, #24]
str	w0, [x1, #8]
add	x6, x3, x2
ldr	x0, [sp, #488]
cmp	x0, x2
csetm	x1, ne	// ne = any
and	x1, x1, x4
add	x1, x1, x0
cmp	x1, x2
csetm	x0, ne	// ne = any
and	x0, x0, x5
add	x0, x0, x1
stp	x23, x0, [sp, #360]
cmp	x6, x7
b.ls	ADDR  // b.plast
ucvtf	s1, x6
ldr	s2, [x25, #16]
ucvtf	s0, x3
fdiv	s1, s1, s2
fdiv	s0, s0, s2
fcvtpu	x0, s1
fcvtpu	x1, s0
cmp	x0, #0x0
csel	x0, x0, x2, ne	// ne = any
cmp	x1, #0x0
ccmp	x3, #0x0, #0x4, eq	// eq = none
b.ne	ADDR  // b.any
cmp	x0, x1
csel	x0, x0, x1, cs	// cs = hs, nlast
adrp	x2, ADDR
mov	x1, #0xd                   	// #13
add	x2, x2, #0xae0
b	ADDR
add	x21, x21, #0x1
cmp	x21, #0x25
b.eq	ADDR  // b.none
ldr	x1, [x2, x21, lsl #3]
cmp	x1, x0
b.cc	ADDR  // b.lo, b.ul, b.last
ldr	x0, [x23, #16]
cmp	x0, x1
b.eq	ADDR  // b.none
mov	x0, x25
bl	ADDR
ldr	x1, [x23, #8]
mov	x0, x22
bl	ADDR
ldr	x8, [sp, #56]
fmov	d1, x8
ldr	d0, [sp, #152]
mov	v1.d[1], x20
mov	x1, x0
mov	x0, x23
mov	v0.d[1], x28
sub	v1.2d, v1.2d, v0.2d
str	q1, [sp]
bl	ADDR
adrp	x0, ADDR
ldr	q2, [sp, #480]
ldr	q0, [x0, #48]
ldr	q1, [sp]
cmeq	v0.2d, v2.2d, v0.2d
ldr	x0, [sp, #368]
bic	v0.16b, v1.16b, v0.16b
add	v0.2d, v0.2d, v2.2d
str	q0, [sp, #304]
cmp	x0, #0x1
add	x22, sp, #0x118
csetm	x21, ne	// ne = any
sub	x24, x24, x22
mov	x2, #0x1                   	// #1
ldr	x28, [sp, #152]
and	x21, x21, x24
ldr	x24, [sp, #48]
add	x21, x21, x0
mov	x1, x28
str	x21, [sp, #280]
mov	x0, x24
str	x2, [sp, #368]
bl	ADDR
sub	x26, x26, x20
ldp	x1, x5, [sp, #384]
cmp	x21, #0x1
ldr	x6, [sp, #56]
csetm	x4, ne	// ne = any
ldr	x27, [sp, #72]
sub	x0, x24, x6
cmp	x1, #0x1
csetm	x3, ne	// ne = any
cmp	x5, #0x1
sub	x22, x22, x27
csetm	x2, ne	// ne = any
and	x2, x2, x26
and	x3, x3, x0
and	x4, x4, x22
add	x2, x2, x5
add	x3, x3, x1
add	x4, x4, x21
mov	x1, x6
mov	x0, x23
mov	x23, x6
str	x4, [sp, #288]
stp	x3, x2, [sp, #480]
bl	ADDR
mov	x1, x28
ldr	x2, [sp, #384]
ldr	x0, [sp, #288]
cmp	x2, #0x1
add	x3, x24, x2
csetm	x2, ne	// ne = any
and	x2, x2, x3
cmp	x0, #0x1
csetm	x4, ne	// ne = any
add	x0, x27, x0
and	x4, x4, x0
mov	x0, x24
ldr	x5, [x2]
sub	x2, x2, x4
cmp	x5, #0x1
csetm	x3, ne	// ne = any
and	x2, x3, x2
add	x2, x2, x5
str	x2, [x4]
ldr	x2, [sp, #384]
ldr	x4, [sp, #288]
cmp	x2, #0x1
csetm	x3, ne	// ne = any
add	x2, x24, x2
and	x3, x3, x2
cmp	x4, #0x1
sub	x5, x27, x3
csetm	x2, ne	// ne = any
and	x2, x2, x5
add	x2, x2, x4
str	x2, [x3]
ldr	x4, [sp, #280]
ldr	x3, [x25, #8]
cmp	x4, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x22
add	x3, x3, #0x1
add	x2, x2, x4
str	x3, [x25, #8]
str	x2, [sp, #288]
bl	ADDR
ldr	x3, [sp, #288]
sub	x4, x27, x23
mov	x1, x24
mov	x0, x20
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x4
add	x2, x2, x3
str	x2, [sp, #480]
bl	ADDR
mov	x0, x19
bl	ADDR
ldr	x25, [sp, #624]
ldr	x26, [sp, #632]
ldr	x27, [sp, #640]
ldr	x28, [sp, #648]
b	ADDR
add	x1, sp, #0x1c8
add	x0, sp, #0x208
bl	ADDR
mov	w0, #0x1                   	// #1
ldrb	w1, [sp, #504]
strb	w0, [sp, #528]
b	ADDR
ldr	x3, [sp, #512]
cmp	x3, #0x1
b.eq	ADDR  // b.none
ldr	x2, [sp, #520]
add	x4, sp, #0x208
add	x1, sp, #0x168
add	x0, sp, #0x130
cmp	x2, #0x1
add	x4, x4, x2
csetm	x2, ne	// ne = any
sub	x3, x3, x1
and	x2, x2, x4
add	x6, sp, #0x1a8
ldr	x4, [sp, #80]
cmp	x2, #0x0
csetm	x5, ne	// ne = any
sub	x2, x2, x0
add	x3, x4, x3
sub	x2, x2, #0x1
cmp	x3, #0x1
sub	x7, x1, x6
csetm	x4, ne	// ne = any
and	x5, x5, x2
and	x2, x4, x7
add	x4, x5, #0x1
add	x2, x2, x3
str	x4, [sp, #304]
stp	x0, x2, [sp, #416]
cmp	x2, #0x1
b.eq	ADDR  // b.none
sub	x6, x6, x1
add	x2, x6, x2
str	x2, [sp, #360]
bl	ADDR
b	ADDR
add	x0, sp, #0x188
mov	x1, x23
bl	ADDR
mov	w0, #0x1                   	// #1
strb	w0, [sp, #400]
strb	wzr, [x23, #8]
b	ADDR
ldr	w19, [x23]
b	ADDR
mov	w1, #0xfffffff3            	// #-13
movk	x1, #0x17f, lsl #32
b	ADDR
ldrsw	x2, [x20, #152]
mov	w3, w0
ldr	w26, [x20, #148]
add	x24, sp, #0x1a0
mov	x0, x24
add	x1, x20, #0xa0
mul	x26, x26, x2
mul	x26, x26, x3
bl	ADDR
mov	x2, x24
mov	x1, x26
ldr	x26, [sp, #56]
mov	x0, x26
bl	ADDR
add	x0, x20, #0x100
ldr	x1, [x20, #264]
ldp	x3, x4, [x0, #16]
cmp	x1, #0x1
add	x2, x25, x1
csetm	x1, ne	// ne = any
and	x1, x1, x2
add	x1, x1, x3, lsl #5
cmp	x3, x4
b.eq	ADDR  // b.none
cmp	x1, #0x0
sub	x1, x1, x24
ldr	x2, [sp, #480]
csetm	x3, ne	// ne = any
sub	x1, x1, #0x1
add	x4, sp, #0x1e8
and	x1, x3, x1
add	x5, x26, x2
cmp	x1, #0x0
add	x1, x1, #0x1
csetm	x3, ne	// ne = any
cmp	x2, #0x1
add	x2, x24, x1
csetm	x1, ne	// ne = any
and	x2, x3, x2
and	x1, x1, x5
cmp	x1, #0x0
mov	x5, x2
sub	x1, x1, #0x1
csetm	x3, ne	// ne = any
sub	x1, x1, x2
and	x1, x3, x1
add	x1, x1, #0x1
str	x1, [x5], #8
adrp	x1, ADDR
sub	x3, x4, x5
ldr	x4, [sp, #488]
ldr	q0, [x1, #1296]
cmp	x4, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x3
add	x1, x1, x4
str	x1, [x2, #8]
ldr	q1, [sp, #496]
str	q1, [x2, #16]
add	x2, sp, #0x2d0
ldr	x1, [x0, #16]
stur	q0, [x2, #-232]
add	x1, x1, #0x1
str	x1, [x0, #16]
str	xzr, [sp, #504]
ldr	x0, [sp, #56]
bl	ADDR
b	ADDR
sub	x3, x3, #0x1d
adrp	x1, ADDR
add	x1, x1, #0x3b8
mov	x0, x22
ldr	x1, [x1, x3, lsl #3]
blr	x1
mov	x1, x0
b	ADDR
ldr	x1, [x3, x2]
sub	x0, x0, x3
cmp	x1, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x0
add	x2, x2, x1
str	x2, [sp, #344]
cmp	x2, #0x1
b.ne	ADDR  // b.any
b	ADDR
mov	w2, #0xfffffff4            	// #-12
mov	x1, #0x600000000           	// #25769803776
movk	x2, #0x17f, lsl #32
mov	w5, #0x0                   	// #0
str	x2, [sp, #16]
sub	x2, x2, #0x1
str	x1, [sp, #96]
sub	x1, x1, #0x1
str	x1, [sp, #32]
b	ADDR
ldr	x2, [sp, #80]
mov	x0, x22
add	x8, sp, #0x1a0
bl	ADDR
ldr	x3, [sp, #512]
ldrb	w1, [sp, #504]
and	w19, w1, #0x1
cmp	x3, #0x1
b.ne	ADDR  // b.any
b	ADDR
ldr	x8, [sp, #56]
adrp	x2, ADDR
add	x2, x2, #0x3b8
add	x2, x2, #0x290
bl	ADDR
b	ADDR
mov	w1, #0xfffffff3            	// #-13
movk	x1, #0x17f, lsl #32
b	ADDR
cmp	x1, #0x1
add	x1, x7, x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
b	ADDR
ldr	x2, [sp, #56]
mov	x8, x24
bl	ADDR
b	ADDR
ldr	x0, [sp, #384]
cmp	x0, #0x1
b.eq	ADDR  // b.none
b	ADDR
str	x19, [sp, #576]
str	x20, [sp, #584]
str	x21, [sp, #592]
str	x22, [sp, #600]
str	x23, [sp, #608]
str	x24, [sp, #616]
str	x25, [sp, #624]
str	x26, [sp, #632]
str	x27, [sp, #640]
str	x28, [sp, #648]
bl	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
mov	x19, x0
b	ADDR
mov	x19, x0
ldr	x0, [sp, #152]
bl	ADDR
ldr	x0, [sp, #80]
bl	ADDR
ldr	x0, [sp, #56]
bl	ADDR
b	ADDR
mov	x19, x0
add	x0, sp, #0x1c0
bl	ADDR
ldr	x0, [sp, #48]
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #552]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
mov	x0, x19
bl	ADDR
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
ldr	x28, [sp, #88]
ldr	x20, [sp, #192]
bl	ADDR
ldr	x1, [sp, #432]
cbnz	x1, ADDR
ldr	x0, [sp, #440]
mov	x6, #0x1                   	// #1
ldr	x2, [sp, #40]
cmp	x0, #0x1
csetm	x3, ne	// ne = any
sub	x5, x26, x19
sub	x2, x2, x26
sub	x4, x19, x23
and	x3, x3, x2
add	x3, x3, x0
cmp	x3, x6
csetm	x2, ne	// ne = any
and	x2, x2, x5
add	x2, x2, x3
cmp	x2, x6
csetm	x3, ne	// ne = any
and	x3, x3, x4
add	x2, x3, x2
stp	x2, x6, [sp, #344]
sub	x27, x26, x25
ldr	x2, [sp, #40]
adrp	x22, ADDR
ldr	x22, [x22, #1160]
sub	x21, x2, x25
cmp	x0, #0x1
add	x0, x0, x1, lsl #3
csetm	x1, ne	// ne = any
sub	x2, x25, x26
and	x1, x1, x21
sub	x3, x26, x19
add	x0, x0, x1
cmp	x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x2
add	x1, x1, x0
ldr	x2, [sp, #344]
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x3
cmp	x2, #0x1
add	x0, x0, x1
csetm	x1, ne	// ne = any
cmp	x0, #0x1
add	x2, x23, x2
add	x0, x19, x0
csetm	x4, ne	// ne = any
and	x1, x1, x2
and	x0, x4, x0
cmp	x1, x0
b.eq	ADDR  // b.none
ldr	x2, [x1]
sub	x1, x1, x25
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, x2
str	x0, [sp, #280]
ldr	x0, [sp, #280]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x4, [x25, x0]
add	x0, x25, x0
sub	x5, x0, x26
sub	x2, x0, x19
cmp	x4, #0x1
mov	x24, x22
csetm	x3, ne	// ne = any
mov	x1, x19
and	x3, x3, x5
mov	x0, x20
add	x3, x3, x4
str	x3, [sp, #328]
str	x2, [sp, #360]
bl	ADDR
ldr	x2, [sp, #328]
ldr	x1, [x20, #8]
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x27
sub	x1, x1, #0x1
add	x0, x0, x2
str	x1, [x20, #8]
str	x0, [sp, #280]
b	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, x0
ldr	x0, [sp, #56]
adrp	x24, ADDR
ldr	x24, [x24, #1160]
bl	ADDR
ldr	x0, [sp, #48]
bl	ADDR
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.eq	ADDR  // b.none
b	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, x0
ldr	x0, [sp, #80]
bl	ADDR
b	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
mov	x19, x0
b	ADDR
ldr	x1, [sp, #488]
mov	x20, x0
cmp	x1, #0x1
b.eq	ADDR  // b.none
add	x0, sp, #0x1e8
ldr	x4, [sp, #48]
add	x0, x0, x1
sub	x0, x0, x19
str	x0, [sp, #360]
cmp	x0, #0x1
sub	x3, x19, x4
ldr	x2, [sp, #480]
csetm	x1, ne	// ne = any
and	x1, x1, x3
add	x0, x1, x0
str	x0, [sp, #384]
add	x1, x4, x0
ldr	x3, [x2]
cmp	x3, #0x1
add	x2, x2, x3
cset	x19, eq	// eq = none
cmp	x0, #0x1
sub	x19, x19, #0x1
csetm	x0, ne	// ne = any
and	x19, x19, x2
ands	x21, x0, x1
b.ne	ADDR  // b.any
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
mov	x0, x20
bl	ADDR
ldr	x1, [sp, #424]
mov	x20, x0
cmp	x1, #0x1
b.eq	ADDR  // b.none
add	x0, sp, #0x1a8
sub	x3, x23, x19
add	x0, x0, x1
sub	x0, x0, x23
str	x0, [sp, #344]
cmp	x0, #0x1
ldr	x2, [sp, #416]
csetm	x1, ne	// ne = any
and	x1, x1, x3
add	x0, x1, x0
str	x0, [sp, #360]
add	x19, x19, x0
ldr	x1, [x2]
cmp	x1, #0x1
add	x2, x2, x1
cset	x21, eq	// eq = none
cmp	x0, #0x1
sub	x21, x21, #0x1
csetm	x0, ne	// ne = any
and	x21, x21, x2
ands	x19, x0, x19
b.ne	ADDR  // b.any
mov	x19, x20
b	ADDR
mov	x0, x19
bl	ADDR
mov	x1, x21
mov	x0, x19
bl	ADDR
mov	x0, x19
bl	ADDR
b	ADDR
mov	x1, x0
mov	x0, x19
mov	x19, x1
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #552]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
b.eq	ADDR  // b.none
b	ADDR
bl	ADDR
ldr	x3, [sp, #440]
mov	x1, x19
ldr	x0, [sp, #40]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
sub	x4, x0, x19
ldr	x0, [sp, #104]
and	x2, x2, x4
add	x2, x2, x3
str	x2, [sp, #360]
bl	ADDR
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, x0
ldr	x0, [sp, #56]
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #552]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
str	x27, [sp, #640]
str	x28, [sp, #648]
b.eq	ADDR  // b.none
b	ADDR
ldr	x0, [sp]
mov	x8, x19
bl	ADDR
mov	x0, x19
bl	ADDR
ldp	x6, x7, [sp, #360]
add	x4, sp, #0x160
sub	x4, x28, x4
sub	x5, x19, x23
ldp	x1, x0, [sp, #432]
cmp	x6, #0x1
csetm	x3, ne	// ne = any
cmp	x7, #0x1
csetm	x2, ne	// ne = any
and	x3, x3, x5
and	x2, x2, x4
add	x3, x3, x6
add	x2, x2, x7
stp	x3, x2, [sp, #344]
b	ADDR
mov	x19, x0
bl	ADDR
b	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
mov	x0, x21
bl	ADDR
mov	x1, x19
mov	x0, x21
bl	ADDR
mov	x19, x20
mov	x0, x21
bl	ADDR
b	ADDR
ldr	x1, [sp, #64]
sub	x6, x19, x23
ldr	x4, [x20, #64]
add	x1, x1, #0x20
ldur	x0, [x1, #-16]
cmp	x4, #0x1
sub	x5, x1, x19
sub	x2, x1, x26
csetm	x1, ne	// ne = any
lsr	x0, x0, #6
and	x5, x1, x5
and	x1, x1, x2
add	x0, x5, x0, lsl #5
add	x1, x1, x4
add	x0, x0, x4
str	x1, [sp, #328]
cmp	x0, #0x1
csetm	x4, ne	// ne = any
and	x4, x4, x6
add	x4, x4, x0
adrp	x0, ADDR
add	x23, x23, x4
str	x4, [sp, #344]
ldr	q0, [x0, #48]
ldr	x5, [sp, #328]
cmp	x5, #0x1
add	x0, x26, x5
csetm	x1, ne	// ne = any
cmp	x4, #0x1
csetm	x2, ne	// ne = any
and	x0, x1, x0
and	x2, x2, x23
cmp	x0, x2
b.eq	ADDR  // b.none
ldr	x2, [x0]
cmp	x2, #0x1
b.ne	ADDR  // b.any
add	x5, x5, #0x20
str	x5, [sp, #328]
b	ADDR
ldr	x4, [sp, #64]
mov	x2, #0x0                   	// #0
ldr	x1, [x4, #16]
and	x1, x1, #0x3f
cmp	x2, x1
b.cs	ADDR  // b.hs, b.nlast
ldr	x1, [x0]
cmp	x1, #0x1
add	x1, x0, x1
csetm	x3, ne	// ne = any
and	x1, x3, x1
ldr	x1, [x1, x2, lsl #3]
cmp	x1, #0x1
b.ne	ADDR  // b.any
ldr	x3, [x0, #8]
lsl	x1, x1, x2
bic	x1, x3, x1
str	x1, [x0, #8]
add	x2, x2, #0x1
b	ADDR
sub	x2, x2, x19
mov	x7, #0x0                   	// #0
add	x2, x0, x2
add	x8, x19, x2
cmp	x2, #0x1
str	x2, [sp, #360]
csetm	x6, ne	// ne = any
and	x6, x6, x8
ldr	x9, [x0, #8]
ldr	x6, [x6, x7, lsl #3]
cmp	x6, #0x1
b.ne	ADDR  // b.any
lsl	x6, x6, x7
bic	x6, x9, x6
str	x6, [x0, #8]
add	x7, x7, #0x1
cmp	x7, #0x40
b.ne	ADDR  // b.any
ldr	x2, [x0, #8]
cbnz	x2, ADDR
ldr	x0, [x0, #16]
cmp	x0, #0x1
b.eq	ADDR  // b.none
and	x0, x3, x1
add	x0, x0, x5
cmp	x0, #0x1
add	x1, x19, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x6, x0, #0x10
add	x7, x0, #0x18
ldp	x1, x8, [x0, #16]
cmp	x1, #0x1
add	x1, x1, x6
csetm	x2, ne	// ne = any
cmp	x8, #0x1
and	x2, x2, x1
csetm	x1, ne	// ne = any
sub	x9, x0, x2
and	x1, x1, x9
add	x1, x1, x8
str	x1, [x2, #24]
ldp	x8, x1, [x0, #16]
cmp	x1, #0x1
add	x1, x7, x1
csetm	x2, ne	// ne = any
cmp	x8, #0x1
and	x2, x2, x1
csetm	x1, ne	// ne = any
add	x7, x2, #0x10
sub	x6, x6, x7
and	x1, x1, x6
add	x1, x1, x8
str	x1, [x2, #16]
str	q0, [x0, #16]
b	ADDR
mov	x0, x23
bl	ADDR
ldp	x1, x0, [sp, #432]
b	ADDR
adrp	x24, ADDR
ldr	x24, [x24, #1160]
ldr	x0, [sp, #552]
ldr	x1, [x24]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, x0
bl	ADDR
ldr	x0, [sp]
bl	ADDR
b	ADDR
