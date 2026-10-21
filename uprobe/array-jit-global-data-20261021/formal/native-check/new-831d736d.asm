stp	x29, x30, [sp, #-96]!
mov	x29, sp
adrp	x4, ADDR
ldr	x4, [x4, #1160]
stp	x19, x20, [sp, #16]
mov	x19, x1
stp	x21, x22, [sp, #32]
mov	x22, x0
mov	x20, x2
stp	x23, x24, [sp, #48]
mov	x21, x3
stp	x25, x26, [sp, #64]
stp	x27, x28, [sp, #80]
sub	sp, sp, #0x390
ldr	w0, [x0, #72]
ldr	x5, [x4]
str	x5, [sp, #904]
mov	x5, #0x0                   	// #0
cmp	w0, #0x1e
b.gt	ADDR
cmp	w0, #0x0
b.le	ADDR
sub	w1, w0, #0x1
cmp	w1, #0x1d
b.ls	ADDR  // b.plast
sbfiz	x2, x0, #1, #32
adrp	x1, ADDR
add	x0, x2, w0, sxtw
add	x1, x1, #0xe30
add	x0, x1, x0, lsl #4
ldr	x5, [x0, #24]
cbz	x5, ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #904]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
ldr	w0, [x22, #112]
add	sp, sp, #0x390
mov	x3, x21
mov	x2, x20
mov	x1, x19
mov	x16, x5
mov	w4, #0x0                   	// #0
ldp	x19, x20, [sp, #16]
ldp	x21, x22, [sp, #32]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #96
br	x16
adrp	x2, ADDR
add	x2, x2, #0x180
ldrh	w2, [x2, w1, uxtw #1]
adr	x1, ADDR
add	x2, x1, w2, sxth #2
br	x2
cmp	w0, #0x3ec
b.eq	ADDR  // b.none
b.gt	ADDR
cmp	w0, #0x3e9
b.eq	ADDR  // b.none
cmp	w0, #0x3ea
b.ne	ADDR  // b.any
ldr	x0, [x22, #120]
add	x22, x22, #0x78
cmp	x0, #0x1
add	x0, x22, x0
csetm	x22, ne	// ne = any
and	x22, x22, x0
ldrb	w0, [x22, #48]
tbnz	w0, #0, ADDR
ldr	w0, [x1]
ldr	w1, [x22, #36]
cmp	w0, w1
b.cs	ADDR  // b.hs, b.nlast
ldr	w1, [x22, #32]
add	x3, x22, #0x8
ldr	x4, [x22, #8]
mov	w2, w1
cmp	x4, #0x1
mul	w1, w0, w1
add	x0, x3, x4
csetm	x3, ne	// ne = any
and	x3, x3, x0
add	x0, x3, w1, uxtw
cmp	x2, #0x1
b.le	ADDR
mov	x1, x20
bl	ADDR
mov	x19, #0x0                   	// #0
nop
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #904]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
add	sp, sp, #0x390
mov	x0, x19
ldp	x19, x20, [sp, #16]
ldp	x21, x22, [sp, #32]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #96
ret
ldr	x0, [x22, #120]
add	x22, x22, #0x78
ldr	w1, [x1]
cmp	x0, #0x1
add	x22, x22, x0
csetm	x0, ne	// ne = any
and	x0, x0, x22
ldr	w0, [x0, #12]
str	w1, [sp, #152]
tbnz	w1, #31, ADDR
mov	w0, w0
cmp	x0, w1, sxtw
b.ls	ADDR  // b.plast
ldr	w0, [x2]
str	w0, [sp, #176]
bl	ADDR
mov	w3, #0xbd                  	// #189
adrp	x2, ADDR
add	x2, x2, #0x98
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x7, ADDR
add	x7, x7, #0xf70
ldp	x8, x9, [sp, #200]
stp	x8, x9, [sp, #112]
add	x6, sp, #0xb0
str	x7, [sp, #216]
add	x5, sp, #0x98
str	x7, [x1, #16]
mov	x4, #0x4d                  	// #77
adrp	x3, ADDR
mov	w2, #0x2                   	// #2
add	x3, x3, #0x1a0
bl	ADDR
b	ADDR
ldr	x0, [x22, #120]
add	x22, x22, #0x78
adrp	x1, ADDR
ldr	x1, [x1, #1160]
cmp	x0, #0x1
add	x22, x22, x0
cset	x0, eq	// eq = none
ldr	x3, [sp, #904]
ldr	x2, [x1]
subs	x3, x3, x2
mov	x2, #0x0                   	// #0
b.ne	ADDR  // b.any
add	sp, sp, #0x390
sub	x0, x0, #0x1
mov	x3, x21
and	x0, x0, x22
mov	x2, x20
ldp	x21, x22, [sp, #32]
mov	x1, x19
ldp	x19, x20, [sp, #16]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #96
b	ADDR
ldr	x0, [x22, #120]
add	x21, x22, #0x78
add	x22, x22, #0x74
cmp	x0, #0x1
add	x21, x21, x0
cset	x23, eq	// eq = none
mov	x0, x22
bl	ADDR
mov	x2, x20
mov	x1, x19
sub	x0, x23, #0x1
and	x0, x0, x21
bl	ADDR
mov	x0, x22
bl	ADDR
b	ADDR
ldr	x0, [x22, #120]
add	x22, x22, #0x78
adrp	x1, ADDR
ldr	x1, [x1, #1160]
cmp	x0, #0x1
add	x22, x22, x0
cset	x0, eq	// eq = none
ldr	x3, [sp, #904]
ldr	x2, [x1]
subs	x3, x3, x2
mov	x2, #0x0                   	// #0
b.ne	ADDR  // b.any
add	sp, sp, #0x390
sub	x0, x0, #0x1
mov	x3, x21
and	x0, x0, x22
mov	x2, x20
ldp	x21, x22, [sp, #32]
mov	x1, x19
ldp	x19, x20, [sp, #16]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #96
b	ADDR
ldr	x0, [x22, #120]
add	x22, x22, #0x78
adrp	x1, ADDR
ldr	x1, [x1, #1160]
cmp	x0, #0x1
add	x22, x22, x0
cset	x0, eq	// eq = none
ldr	x3, [sp, #904]
ldr	x2, [x1]
subs	x3, x3, x2
mov	x2, #0x0                   	// #0
b.ne	ADDR  // b.any
add	sp, sp, #0x390
sub	x0, x0, #0x1
mov	x3, x21
and	x0, x0, x22
mov	x2, x20
ldp	x21, x22, [sp, #32]
mov	x1, x19
ldp	x19, x20, [sp, #16]
ldp	x23, x24, [sp, #48]
ldp	x25, x26, [sp, #64]
ldp	x27, x28, [sp, #80]
ldp	x29, x30, [sp], #96
b	ADDR
ldr	x0, [x22, #120]
add	x1, x22, #0x78
add	x22, x22, #0x74
cmp	x0, #0x1
add	x1, x1, x0
csetm	x21, ne	// ne = any
mov	x0, x22
and	x21, x21, x1
bl	ADDR
ldr	w0, [x19]
tbnz	w0, #31, ADDR
ldr	x1, [x21, #16]
sxtw	x0, w0
cmp	x0, x1
b.cs	ADDR  // b.hs, b.nlast
ldr	x1, [x21, #8]
add	x21, x21, #0x8
ldr	w2, [x20]
mov	x19, #0x0                   	// #0
cmp	x1, #0x1
add	x21, x21, x1
csetm	x1, ne	// ne = any
and	x1, x1, x21
str	w2, [x1, x0, lsl #2]
mov	x0, x22
bl	ADDR
b	ADDR
ldr	x1, [x22, #120]
add	x22, x22, #0x78
ldr	w0, [x19]
cmp	x1, #0x1
add	x22, x22, x1
csetm	x19, ne	// ne = any
and	x19, x19, x22
tbnz	w0, #31, ADDR
ldr	x1, [x19, #16]
sxtw	x21, w0
cmp	x21, x1
b.cs	ADDR  // b.hs, b.nlast
adrp	x0, ADDR
ldr	w20, [x20]
str	w20, [sp, #152]
ldr	x0, [x0, #2232]
cmp	w20, #0x0
ccmp	x0, #0x0, #0x4, ge	// ge = tcont
b.ne	ADDR  // b.any
adrp	x1, ADDR
add	x4, sp, #0x260
movi	v0.4s, #0x0
mov	w0, #0xe8                  	// #232
ldr	x5, [x1, #552]
str	w20, [sp, #464]
str	w0, [sp, #468]
add	x2, sp, #0x1d0
str	x4, [sp, #472]
mov	x3, #0x10                  	// #16
str	xzr, [sp, #832]
mov	w1, #0xf                   	// #15
mov	x0, #0x118                 	// #280
stp	q0, q0, [x4]
stp	q0, q0, [x4, #32]
stp	q0, q0, [x4, #64]
stp	q0, q0, [x4, #96]
stp	q0, q0, [x4, #128]
stp	q0, q0, [x4, #160]
stp	q0, q0, [x4, #192]
blr	x5
cmp	w0, #0x0
cbz	w0, ADDR
str	w0, [sp, #176]
b.lt	ADDR  // b.tstop
ldr	x1, [x19, #8]
add	x0, x19, #0x8
ldr	w2, [sp, #612]
cmp	x1, #0x1
add	x0, x0, x1
csetm	x1, ne	// ne = any
and	x0, x1, x0
str	w2, [x0, x21, lsl #2]
b	ADDR
ldr	x0, [x22, #120]
add	x1, x22, #0x78
add	x22, x22, #0x74
cmp	x0, #0x1
add	x1, x1, x0
csetm	x23, ne	// ne = any
mov	x0, x22
and	x23, x23, x1
bl	ADDR
str	x21, [sp, #144]
cbz	x19, ADDR
bl	ADDR
mov	w3, #0x127                 	// #295
adrp	x2, ADDR
add	x2, x2, #0xe48
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #200]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #216]
add	x3, x3, #0xe0
str	x5, [x1, #16]
mov	x4, #0x3e                  	// #62
mov	w2, #0x3                   	// #3
bl	ADDR
cbz	x20, ADDR
ldr	x0, [sp, #144]
cbnz	x0, ADDR
ldr	w25, [x23, #8]
add	x26, x23, #0x30
add	x19, x23, #0x38
mov	w24, #0x0                   	// #0
mov	w21, #0x1                   	// #1
cbz	w25, ADDR
nop
ldp	w1, w2, [x23, #12]
mov	w5, w24
ldr	w3, [x23, #40]
mov	x4, x20
ldr	w0, [x23]
bl	ADDR
lsr	w2, w0, #3
ldr	x1, [x26, #16]
lsr	w3, w0, #3
cmp	x2, x1
b.cs	ADDR  // b.hs, b.nlast
ldr	x1, [x19]
and	w0, w0, #0x7
cmp	x1, #0x1
add	x2, x19, x1
csetm	x1, ne	// ne = any
lsl	w0, w21, w0
and	x1, x1, x2
ldrb	w2, [x1, w3, uxtw]
orr	w0, w0, w2
strb	w0, [x1, w3, uxtw]
ldr	w25, [x23, #8]
add	w24, w24, #0x1
cmp	w24, w25
b.cc	ADDR  // b.lo, b.ul, b.last
mov	x19, #0x0                   	// #0
b	ADDR
bl	ADDR
mov	w3, #0x76                  	// #118
adrp	x2, ADDR
add	x2, x2, #0xce8
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #200]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #216]
add	x3, x3, #0xf80
str	x5, [x1, #16]
mov	x4, #0x41                  	// #65
mov	w2, #0x4                   	// #4
bl	ADDR
bl	ADDR
mov	w1, #0x5f                  	// #95
str	w1, [x0]
mov	x19, #0xffffffffffffffff    	// #-1
b	ADDR
ldr	x0, [x22, #120]
add	x1, x22, #0x78
add	x22, x22, #0x74
cmp	x0, #0x1
add	x1, x1, x0
csetm	x23, ne	// ne = any
mov	x0, x22
and	x23, x23, x1
bl	ADDR
cbz	x19, ADDR
bl	ADDR
mov	w3, #0x41                  	// #65
adrp	x2, ADDR
add	x2, x2, #0xe80
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #200]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #216]
add	x3, x3, #0x68
str	x5, [x1, #16]
mov	x4, #0x3e                  	// #62
mov	w2, #0x3                   	// #3
bl	ADDR
cbz	x20, ADDR
ldr	x3, [x23, #24]
add	x25, x23, #0x8
ldp	w0, w1, [x23]
udiv	x0, x3, x0
cmp	x1, x0
b.ls	ADDR  // b.plast
ldr	x4, [x23, #16]
add	x24, sp, #0x260
add	x26, sp, #0x1d0
add	x19, sp, #0x1b0
cmp	x4, #0x1
csetm	x5, ne	// ne = any
add	x1, x25, #0x8
sub	x6, x24, x19
sub	x0, x1, x24
sub	x19, x19, x26
and	x0, x0, x5
add	x0, x0, x4
cmp	x0, #0x1
add	x0, x0, x3
csetm	x2, ne	// ne = any
and	x2, x2, x6
add	x2, x2, x0
str	x2, [sp, #432]
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x19
add	x0, x0, x2
str	x0, [sp, #464]
cmp	x0, #0x1
add	x0, x26, x0
ldr	x6, [x25, #24]
csetm	x2, ne	// ne = any
ldr	w19, [x23]
and	x0, x2, x0
sub	x2, x6, x3
cmp	x19, x2
b.hi	ADDR  // b.pmore
add	x1, x1, x4
and	x1, x1, x5
add	x1, x1, x3
cmp	x0, x1
b.eq	ADDR  // b.none
cbz	x19, ADDR
mov	x3, x20
mov	x2, x19
bl	ADDR
ldr	x3, [x25, #16]
add	x19, x19, x3
str	x19, [x25, #16]
mov	x19, #0x0                   	// #0
b	ADDR
ldr	x0, [x22, #120]
add	x1, x22, #0x78
add	x22, x22, #0x74
cmp	x0, #0x1
add	x1, x1, x0
csetm	x23, ne	// ne = any
mov	x0, x22
and	x23, x23, x1
bl	ADDR
cbz	x19, ADDR
bl	ADDR
mov	w3, #0x46                  	// #70
adrp	x2, ADDR
add	x2, x2, #0xdd8
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #200]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #216]
add	x3, x3, #0xff0
str	x5, [x1, #16]
mov	x4, #0x3e                  	// #62
mov	w2, #0x3                   	// #3
bl	ADDR
cbz	x20, ADDR
ldr	w1, [x23, #4]
ldr	w0, [x23, #48]
cmp	w1, w0
b.hi	ADDR  // b.pmore
cmp	x21, #0x2
b.ne	ADDR  // b.any
ldr	w2, [x23, #40]
sub	w0, w0, #0x1
str	w0, [x23, #48]
add	w0, w2, #0x1
udiv	w2, w0, w1
msub	w0, w2, w1, w0
str	w0, [x23, #40]
ldr	w3, [x23]
add	x4, x23, #0x10
ldr	w5, [x23, #44]
mov	x1, x20
ldr	x0, [x23, #16]
mov	w2, w3
mov	x19, #0x0                   	// #0
mul	w3, w3, w5
add	x4, x4, x0
cmp	x0, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x4
add	x0, x0, x3
bl	ADDR
ldp	w0, w1, [x23, #44]
ldr	w2, [x23, #4]
add	w0, w0, #0x1
add	w1, w1, #0x1
str	w1, [x23, #48]
udiv	w1, w0, w2
msub	w0, w1, w2, w0
str	w0, [x23, #44]
b	ADDR
ldr	x0, [x22, #120]
add	x1, x22, #0x78
add	x2, x22, #0x74
str	x2, [sp, #32]
cmp	x0, #0x1
add	x1, x1, x0
csetm	x25, ne	// ne = any
mov	x0, x2
and	x25, x25, x1
bl	ADDR
str	x21, [sp, #144]
add	x0, x25, #0x38
str	x0, [sp, #40]
bl	ADDR
cmp	x20, #0x0
ccmp	x19, #0x0, #0x4, ne	// ne = any
b.eq	ADDR  // b.none
ldr	x0, [sp, #144]
str	x0, [sp, #16]
cmp	x0, #0x2
b.hi	ADDR  // b.pmore
ldr	w0, [x19]
ldr	w1, [x25, #16]
str	w0, [sp]
str	w1, [sp, #8]
cmp	w0, w1
b.hi	ADDR  // b.pmore
ldr	x26, [x25, #32]
add	x3, x25, #0x20
cmp	x26, #0x1
b.ne	ADDR  // b.any
ldr	x0, [sp, #16]
cmp	x0, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
add	x24, sp, #0x260
mov	x1, x20
mov	x8, x24
mov	x0, x25
mov	w2, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #616]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x1, sp, #0x268
add	x3, sp, #0x1d0
add	x1, x1, x0
ldr	w5, [x19]
sub	x3, x1, x3
mov	x0, x25
cmp	x3, #0x1
mov	x2, x19
csetm	x4, ne	// ne = any
and	x1, x4, x1
str	w5, [x1, #32]
str	x3, [sp, #464]
bl	ADDR
mov	x1, x24
add	x0, x25, #0x18
bl	ADDR
mov	x19, #0x0                   	// #0
ldr	x0, [x25, #40]
add	x0, x0, #0x1
str	x0, [x25, #40]
mov	x0, x24
bl	ADDR
ldr	x0, [sp, #40]
bl	ADDR
ldr	x0, [sp, #32]
bl	ADDR
b	ADDR
add	x23, x22, #0x74
add	x24, x22, #0x78
ldr	x22, [x22, #120]
mov	x0, x23
bl	ADDR
cmp	x21, #0x2
b.hi	ADDR  // b.pmore
cmp	x22, #0x1
add	x24, x24, x22
csetm	x22, ne	// ne = any
mov	x1, x19
and	x22, x22, x24
add	x25, x22, #0x80
mov	x0, x25
ldr	x2, [x22, #104]
add	x2, x19, x2
bl	ADDR
ldr	x0, [x22, #8]
cbnz	x0, ADDR
adrp	x0, ADDR
ldr	q0, [x0, #544]
str	q0, [sp, #400]
str	d0, [sp, #416]
cmp	x21, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x22, #8]
ldr	x0, [x22, #120]
cmp	x1, x0
b.eq	ADDR  // b.none
ldr	x2, [x22, #112]
add	x19, x22, #0xa0
mov	x1, x20
mov	x0, x19
add	x2, x20, x2
bl	ADDR
add	x20, sp, #0x160
mov	x1, x25
mov	x8, x20
mov	x0, x22
str	x20, [sp, #40]
bl	ADDR
add	x26, sp, #0x1d0
mov	x1, x19
mov	x0, x26
bl	ADDR
add	x19, sp, #0x1f0
mov	x1, x20
mov	x0, x19
add	x24, sp, #0x260
str	x19, [sp, #56]
bl	ADDR
mov	x1, x25
mov	x0, x24
bl	ADDR
add	x0, sp, #0x280
mov	x1, x26
str	x0, [sp, #48]
bl	ADDR
mov	x1, x19
add	x25, x22, #0x20
add	x0, sp, #0x2a0
str	x0, [sp, #32]
bl	ADDR
mov	x0, x24
bl	ADDR
ldr	x2, [x25, #8]
str	x0, [sp, #64]
cmp	x2, #0x1c
b.hi	ADDR  // b.pmore
ldr	x4, [sp, #64]
adrp	x3, ADDR
add	x3, x3, #0xaf8
adrp	x0, ADDR
add	x0, x0, #0xbe0
lsr	x1, x4, #32
ldr	x3, [x3, x2, lsl #3]
add	w1, w1, w4
ldr	x0, [x0, x2, lsl #3]
mul	x1, x1, x3
mov	w0, w0
umulh	x1, x1, x0
add	x0, sp, #0x150
add	x21, sp, #0x180
mov	x19, x0
mov	x8, x0
mov	x0, x25
str	x19, [sp, #96]
bl	ADDR
add	x20, sp, #0x148
ldp	x3, x2, [sp, #336]
sub	x0, x19, x21
mov	x5, #0xffffffffffffffd0    	// #-48
add	x28, sp, #0x170
sub	x6, x20, x28
add	x19, sp, #0x1b0
cmp	x3, #0x1
csetm	x1, ne	// ne = any
cmp	x2, #0x1
and	x1, x1, x0
csetm	x0, ne	// ne = any
and	x0, x0, x5
add	x4, x1, x3
add	x0, x0, x2
stp	x4, x0, [sp, #384]
add	x1, x22, #0x38
ldr	x3, [x22, #56]
sub	x1, x1, x20
ldr	x0, [x25, #16]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
mov	x5, #0x1                   	// #1
and	x1, x2, x1
sub	x2, x28, x19
add	x0, x3, x0, lsl #3
add	x3, x21, x4
add	x0, x0, x1
str	x5, [sp, #296]
cmp	x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x6
add	x1, x1, x0
add	x0, sp, #0x158
cmp	x1, x5
str	x0, [sp, #104]
csetm	x0, ne	// ne = any
cmp	x4, x5
and	x0, x0, x2
csetm	x2, ne	// ne = any
add	x0, x0, x1
and	x1, x2, x3
cmp	x0, x5
add	x0, x19, x0
csetm	x2, ne	// ne = any
and	x0, x2, x0
cmp	x1, x0
b.eq	ADDR  // b.none
ldr	x2, [x1]
add	x4, sp, #0x128
sub	x1, x1, x4
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x1
add	x0, x0, x2
str	x0, [sp, #296]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x7, [sp, #616]
add	x5, sp, #0x268
add	x6, sp, #0x130
add	x3, sp, #0x138
cmp	x7, #0x1
sub	x2, x5, x6
csetm	x1, ne	// ne = any
sub	x5, x5, x3
and	x2, x1, x2
and	x1, x1, x5
add	x2, x2, x7
add	x1, x1, x7
ldr	x5, [sp, #624]
cmp	x2, #0x1
csetm	x8, ne	// ne = any
cmp	x1, #0x1
sub	x3, x3, x28
csetm	x7, ne	// ne = any
sub	x6, x6, x19
and	x3, x7, x3
add	x1, x5, x1
add	x27, sp, #0x140
add	x1, x3, x1
and	x6, x8, x6
add	x2, x6, x2
stp	x2, x1, [sp, #72]
sub	x1, x27, x20
str	x1, [sp, #88]
b	ADDR
ldr	x1, [x3]
sub	x2, x3, x4
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x2
add	x0, x0, x1
str	x0, [sp, #296]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x3, x0, x4
add	x0, x3, #0x8
ldr	x1, [x0, #16]
cmp	x5, x1
b.ne	ADDR  // b.any
ldr	x1, [sp, #80]
str	x1, [sp, #368]
ldr	x1, [sp, #72]
str	x1, [sp, #432]
add	x0, x0, #0x8
stp	x3, x4, [sp]
sub	x0, x0, x27
ldr	x7, [x3, #16]
mov	x2, x20
ldr	x3, [sp, #88]
cmp	x7, #0x1
csetm	x6, ne	// ne = any
mov	x1, x28
and	x0, x6, x0
str	x5, [sp, #16]
add	x0, x0, x7
cmp	x0, #0x1
csetm	x6, ne	// ne = any
and	x6, x6, x3
add	x0, x6, x0
str	x0, [sp, #328]
mov	x0, x19
bl	ADDR
ldp	x3, x4, [sp]
ldr	x5, [sp, #16]
tbz	w0, #0, ADDR
ldr	x0, [sp, #296]
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	x0, [sp, #32]
bl	ADDR
ldr	x0, [sp, #48]
bl	ADDR
mov	x0, x24
bl	ADDR
ldr	x0, [sp, #56]
bl	ADDR
mov	x0, x26
bl	ADDR
ldr	x0, [sp, #40]
bl	ADDR
mov	x19, #0x0                   	// #0
mov	x0, x23
bl	ADDR
b	ADDR
add	x22, x22, #0x74
mov	x0, x22
bl	ADDR
bl	ADDR
mov	w3, #0x42                  	// #66
adrp	x2, ADDR
add	x2, x2, #0xda8
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #200]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #216]
add	x3, x3, #0xfc8
str	x5, [x1, #16]
mov	x4, #0x22                  	// #34
mov	w2, #0x4                   	// #4
bl	ADDR
mov	x0, x22
mov	x19, #0xffffffffffffffa1    	// #-95
bl	ADDR
b	ADDR
cmp	w0, #0x3ee
b.ne	ADDR  // b.any
ldr	x0, [x22, #120]
add	x22, x22, #0x78
cmp	x0, #0x1
add	x22, x22, x0
csetm	x0, ne	// ne = any
and	x0, x0, x22
ldr	w0, [x0, #8]
bl	ADDR
sxtw	x19, w0
b	ADDR
ldr	x0, [x22, #120]
add	x22, x22, #0x78
cmp	x0, #0x1
add	x0, x22, x0
csetm	x22, ne	// ne = any
and	x22, x22, x0
ldr	w0, [x22, #76]
tbnz	w0, #31, ADDR
mov	x1, x19
mov	x3, x21
mov	x2, x20
bl	ADDR
sxtw	x19, w0
b	ADDR
cmp	x21, #0x2
b.ne	ADDR  // b.any
ldr	x4, [x23, #16]
add	x24, sp, #0x260
add	x0, x25, #0x8
add	x26, sp, #0x1d0
cmp	x4, #0x1
sub	x1, x0, x24
csetm	x5, ne	// ne = any
add	x19, sp, #0x1b0
and	x1, x5, x1
sub	x6, x24, x26
add	x1, x1, x4
sub	x8, x0, x19
cmp	x1, #0x1
and	x8, x5, x8
csetm	x2, ne	// ne = any
add	x8, x8, x4
and	x2, x2, x6
add	x7, sp, #0x180
add	x2, x2, x1
sub	x9, x19, x7
cmp	x2, #0x1
sub	x10, x26, x24
csetm	x6, ne	// ne = any
cmp	x8, #0x1
csetm	x1, ne	// ne = any
and	x6, x6, x10
and	x1, x1, x9
add	x6, x6, x2
add	x1, x1, x8
str	x1, [sp, #384]
str	x2, [sp, #464]
cmp	x1, #0x1
str	x6, [sp, #608]
sub	x2, x7, x19
csetm	x10, ne	// ne = any
add	x7, sp, #0x170
ldr	w9, [x23]
and	x10, x10, x2
add	x2, sp, #0x190
sub	x11, x19, x7
add	x9, x9, x1
sub	x8, x7, x2
mov	x1, x2
add	x2, x9, x10
cmp	x2, #0x1
sub	x7, x1, x19
csetm	x1, ne	// ne = any
add	x9, x24, x6
and	x1, x1, x11
add	x1, x1, x2
cmp	x1, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x8
add	x2, x2, x1
str	x2, [sp, #400]
cmp	x2, #0x1
csetm	x1, ne	// ne = any
cmp	x6, #0x1
and	x1, x1, x7
csetm	x6, ne	// ne = any
add	x1, x1, x2
and	x6, x6, x9
cmp	x1, #0x1
add	x7, x19, x1
csetm	x2, ne	// ne = any
str	x1, [sp, #432]
and	x1, x2, x7
cmp	x6, x1
b.eq	ADDR  // b.none
add	x21, x4, x0
cmp	x6, #0x0
and	x21, x5, x21
ccmp	x1, #0x0, #0x4, ne	// ne = any
add	x21, x21, x3
ccmp	x1, x21, #0x4, ne	// ne = any
b.eq	ADDR  // b.none
sub	x27, x21, x1
mov	x0, x6
mov	x2, x27
bl	ADDR
ldr	x3, [x25, #16]
add	x6, x0, x27
sub	x21, x21, x6
sub	x3, x3, x21
str	x3, [x25, #16]
ldr	x4, [x23, #16]
cmp	x4, #0x1
csetm	x5, ne	// ne = any
b	ADDR
add	x24, sp, #0x260
mov	x27, x3
sub	x0, x26, x24
sub	x1, x3, x24
add	x0, x3, x0
add	x22, x25, #0x18
mov	x28, #0xffffffffffffffff    	// #-1
mov	x23, #0x0                   	// #0
mov	x21, #0x0                   	// #0
cmp	x0, #0x1
b.ne	ADDR  // b.any
b	ADDR
add	x0, x19, x0, lsr #3
mvn	w1, w1
and	w1, w1, #0x7
ldrb	w22, [x0, #4]
asr	w22, w22, w1
ubfiz	x22, x22, #4, #1
add	x3, x22, #0x8
add	x22, x21, x22
ldr	x26, [x22, #8]
add	x27, x21, x3
sub	x1, x27, x24
cmp	x26, #0x1
csetm	x28, ne	// ne = any
and	x0, x28, x1
add	x0, x0, x26
cmp	x0, #0x1
b.eq	ADDR  // b.none
and	x1, x28, x1
mov	x2, x19
add	x1, x1, x26
mov	x0, x25
cmp	x1, #0x1
add	x1, x24, x1
csetm	x21, ne	// ne = any
and	x21, x21, x1
mov	x1, x21
bl	ADDR
mov	x23, x0
ldr	w0, [sp]
ldr	w1, [x21, #32]
ldr	w2, [sp, #8]
cmp	w0, w1
mov	w0, w1
ccmp	w2, w1, #0x4, ne	// ne = any
ccmp	x23, x0, #0x0, ne	// ne = any
b.ne	ADDR  // b.any
ldr	w2, [x25, #12]
cmp	x0, x2, lsl #3
b.cc	ADDR  // b.lo, b.ul, b.last
mov	x22, x21
mov	x3, #0x8                   	// #8
b	ADDR
ldr	x1, [x0, #16]
cmp	x1, w20, sxtw
b.ls	ADDR  // b.plast
mov	w1, w20
bl	ADDR
ldrb	w0, [x0, #160]
cmp	w0, #0x3
b.ne	ADDR  // b.any
ldr	x2, [x19, #8]
mov	w0, #0xfffffffe            	// #-2
add	x19, x19, #0x8
sub	w1, w0, w20
cmp	x2, #0x1
add	x19, x19, x2
csetm	x0, ne	// ne = any
and	x0, x0, x19
str	w1, [x0, x21, lsl #2]
b	ADDR
mov	x0, x22
bl	ADDR
tbz	w0, #0, ADDR
ldr	x3, [x22, #56]
cbz	x3, ADDR
ldr	w0, [x19]
ldr	w1, [x22, #36]
cmp	w0, w1
b.cs	ADDR  // b.hs, b.nlast
ldr	w1, [x22, #32]
mov	w2, w1
mul	w1, w0, w1
add	x0, x3, x1
cmp	x2, #0x1
b.gt	ADDR
b.ne	ADDR  // b.any
ldrb	w0, [x20]
strb	w0, [x3, x1]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
mov	x19, #0xffffffffffffffea    	// #-22
b	ADDR
mov	x0, x22
bl	ADDR
ldr	w0, [x22, #76]
b	ADDR
sub	x0, x27, x24
and	x0, x28, x0
add	x0, x0, x26
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	w0, [sp]
cmp	x23, w0, uxtw
ldr	x0, [sp, #16]
b.eq	ADDR  // b.none
cmp	x0, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
add	x26, sp, #0x1d0
mov	x1, x20
mov	x8, x26
mov	x0, x25
mov	w2, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #472]
cmp	x0, #0x1
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0xc                   	// #12
str	w1, [x0]
mov	x0, x26
bl	ADDR
b	ADDR
sub	x0, x27, x24
and	x0, x28, x0
add	x0, x0, x26
cmp	x0, #0x1
b.eq	ADDR  // b.none
ldr	w0, [x21, #32]
ldr	w1, [sp]
cmp	w1, w0
b.ne	ADDR  // b.any
mov	x1, x21
mov	x2, x19
mov	x0, x25
bl	ADDR
ldr	w1, [sp]
cmp	x0, w1, uxtw
b.eq	ADDR  // b.none
ldr	x0, [sp, #16]
cmp	x0, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
add	x26, sp, #0x1d0
mov	x1, x20
mov	x8, x26
mov	x0, x25
mov	w2, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #472]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x1, sp, #0x1d8
ldr	w5, [x19]
add	x1, x1, x0
mov	x2, x19
sub	x4, x1, x24
mov	x0, x25
cmp	x4, #0x1
csetm	x3, ne	// ne = any
and	x1, x3, x1
str	w5, [x1, #32]
str	x4, [sp, #608]
bl	ADDR
mov	x8, x24
mov	x0, x25
mov	w2, #0x1                   	// #1
mov	x1, #0x0                   	// #0
bl	ADDR
ldr	x1, [sp, #616]
cmp	x1, #0x1
b.eq	ADDR  // b.none
add	x20, sp, #0x268
add	x21, sp, #0x1b0
add	x1, x1, x20
sub	x4, x1, x21
mov	x2, x19
cmp	x4, #0x1
mov	x0, x25
csetm	x3, ne	// ne = any
and	x1, x3, x1
str	w23, [x1, #32]
str	x4, [sp, #432]
bl	ADDR
mov	x2, x23
add	x1, x19, #0x4
mov	x0, x25
bl	ADDR
cbz	w0, ADDR
ldr	x2, [sp, #616]
sub	x20, x20, x21
mov	x1, x22
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x20
add	x0, x0, x2
str	x0, [sp, #432]
cmp	x0, #0x1
add	x0, x21, x0
csetm	x2, ne	// ne = any
and	x0, x2, x0
bl	ADDR
ldr	x2, [sp, #616]
mov	x1, x26
cmp	x2, #0x1
mov	x19, #0x0                   	// #0
csetm	x0, ne	// ne = any
and	x0, x0, x20
add	x0, x0, x2
str	x0, [sp, #432]
cmp	x0, #0x1
add	x0, x21, x0
csetm	x2, ne	// ne = any
and	x0, x2, x0
add	x0, x0, #0x10
bl	ADDR
mov	x0, x22
mov	x1, x24
bl	ADDR
ldr	x0, [x25, #40]
add	x0, x0, #0x1
str	x0, [x25, #40]
mov	x0, x24
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x1c                  	// #28
str	w1, [x0]
b	ADDR
cmp	x19, #0x0
ccmp	x0, #0x0, #0x4, ne	// ne = any
b.eq	ADDR  // b.none
mov	x1, x20
mov	x2, x19
bl	ADDR
ldr	x3, [x25, #16]
b	ADDR
add	x19, x22, #0x20
mov	x0, x25
bl	ADDR
ldr	x1, [x19, #8]
bl	ADDR
add	x26, sp, #0x1d0
mov	x1, x0
mov	x8, x26
mov	x0, x19
bl	ADDR
add	x28, sp, #0x170
ldr	x0, [sp, #464]
cmp	x0, #0x1
add	x1, x26, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
sub	x1, x0, x28
ldr	x0, [x0]
cmp	x0, #0x1
csetm	x19, ne	// ne = any
and	x19, x19, x1
add	x19, x19, x0
str	x19, [sp, #368]
cmp	x19, #0x1
b.eq	ADDR  // b.none
ldr	x6, [x25, #16]
b	ADDR
ldr	x0, [x4]
sub	x4, x4, x28
cmp	x0, #0x1
csetm	x19, ne	// ne = any
and	x19, x19, x4
add	x19, x19, x0
str	x19, [sp, #368]
cmp	x19, #0x1
b.eq	ADDR  // b.none
add	x4, x28, x19
add	x0, x4, #0x8
ldr	x1, [x0, #16]
cmp	x6, x1
b.ne	ADDR  // b.any
ldr	x7, [x22, #136]
add	x3, sp, #0x1b0
add	x5, x22, #0x88
add	x27, sp, #0x190
cmp	x7, #0x1
sub	x2, x5, x3
csetm	x1, ne	// ne = any
sub	x5, x5, x27
and	x2, x1, x2
and	x1, x1, x5
add	x2, x2, x7
add	x1, x1, x7
cmp	x2, #0x1
add	x24, sp, #0x260
csetm	x5, ne	// ne = any
cmp	x1, #0x1
sub	x9, x3, x24
sub	x8, x27, x3
csetm	x7, ne	// ne = any
add	x1, x6, x1
and	x5, x5, x9
and	x7, x7, x8
add	x2, x5, x2
add	x1, x7, x1
str	x1, [sp, #432]
add	x0, x0, #0x8
str	x2, [sp, #608]
add	x1, sp, #0x180
sub	x5, x0, x1
sub	x8, x1, x27
ldr	x9, [x4, #16]
mov	x2, x27
mov	x1, x3
mov	x0, x24
cmp	x9, #0x1
stp	x3, x4, [sp]
csetm	x7, ne	// ne = any
and	x5, x7, x5
str	x6, [sp, #16]
add	x5, x5, x9
cmp	x5, #0x1
csetm	x7, ne	// ne = any
and	x7, x7, x8
add	x5, x7, x5
str	x5, [sp, #400]
bl	ADDR
ldp	x4, x6, [sp, #8]
tbz	w0, #0, ADDR
ldr	x3, [sp]
mov	x1, x26
mov	x0, x24
sub	x2, x28, x3
add	x2, x2, x19
str	x2, [sp, #432]
bl	ADDR
ldr	x4, [sp, #432]
mov	x1, x24
ldr	x3, [sp]
cmp	x4, #0x1
csetm	x2, ne	// ne = any
add	x0, sp, #0x198
sub	x3, x3, x27
and	x2, x2, x3
add	x2, x2, x4
str	x2, [sp, #400]
bl	ADDR
ldr	x1, [sp, #400]
cmp	x1, #0x1
add	x1, x27, x1
csetm	x0, ne	// ne = any
ands	x0, x0, x1
ccmp	x21, #0x1, #0x0, ne	// ne = any
b.eq	ADDR  // b.none
cmp	x0, #0x0
ccmp	x21, #0x2, #0x0, eq	// eq = none
b.eq	ADDR  // b.none
cbz	x0, ADDR
ldr	x2, [x22, #112]
add	x0, x0, #0x28
mov	x1, x20
add	x2, x20, x2
bl	ADDR
ldr	x1, [sp, #400]
mov	x0, x24
cmp	x1, #0x1
add	x27, x27, x1
csetm	x1, ne	// ne = any
and	x1, x1, x27
add	x1, x1, #0x48
bl	ADDR
mov	x1, x24
mov	x0, x22
bl	ADDR
mov	x0, x24
bl	ADDR
b	ADDR
ldr	x0, [sp, #16]
cmp	x0, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
mov	x1, x20
mov	x8, x24
mov	x0, x25
mov	w2, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #616]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x1, sp, #0x268
add	x3, sp, #0x1d0
add	x1, x1, x0
ldr	w5, [x19]
sub	x3, x1, x3
mov	x2, x19
cmp	x3, #0x1
mov	x0, x25
csetm	x4, ne	// ne = any
and	x1, x4, x1
str	w5, [x1, #32]
str	x3, [sp, #464]
bl	ADDR
mov	x1, x24
mov	x0, x22
b	ADDR
add	x1, sp, #0x1d8
ldr	w5, [x19]
add	x1, x1, x0
mov	x2, x19
sub	x4, x1, x24
mov	x0, x25
cmp	x4, #0x1
csetm	x3, ne	// ne = any
and	x1, x3, x1
str	w5, [x1, #32]
str	x4, [sp, #608]
bl	ADDR
mov	x8, x24
mov	x0, x25
mov	w2, #0x1                   	// #1
mov	x1, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #616]
cmp	x0, #0x1
b.ne	ADDR  // b.any
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0xc                   	// #12
str	w1, [x0]
b	ADDR
tbz	w0, #0, ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
ldr	w1, [sp, #16]
str	w1, [x0]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0xc                   	// #12
str	w1, [x0]
b	ADDR
cmp	x0, #0x2
b.eq	ADDR  // b.none
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
mov	x1, x20
mov	x8, x24
mov	x0, x25
mov	w2, #0x0                   	// #0
bl	ADDR
ldr	x0, [sp, #616]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x26, sp, #0x268
add	x20, sp, #0x1d0
add	x1, x0, x26
ldr	w5, [x19]
sub	x4, x1, x20
mov	x2, x19
cmp	x4, #0x1
mov	x0, x25
csetm	x3, ne	// ne = any
sub	x26, x26, x20
and	x1, x3, x1
str	w5, [x1, #32]
str	x4, [sp, #464]
bl	ADDR
ldr	x3, [x21, #56]
add	x1, x21, #0x38
mov	x2, x23
mov	x0, x25
cmp	x3, #0x1
add	x1, x1, x3
csetm	x3, ne	// ne = any
and	x1, x3, x1
bl	ADDR
ldr	x3, [sp, #616]
mov	x1, x22
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x26
add	x2, x2, x3
str	x2, [sp, #464]
cmp	x2, #0x1
add	x20, x20, x2
csetm	x2, ne	// ne = any
and	x2, x2, x20
add	x0, x2, w0, sxtw #4
bl	ADDR
b	ADDR
add	x27, sp, #0x140
ldr	x0, [x22, #32]
mov	x1, #0x1                   	// #1
stp	x25, x1, [sp, #432]
cmp	x0, x1
csetm	x1, ne	// ne = any
add	x0, x25, x0
and	x2, x1, x0
str	x2, [sp, #8]
cmp	x2, #0x0
sub	x0, x2, x27
sub	x0, x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x0
mov	x0, x2
add	x1, x1, #0x1
str	x1, [sp, #320]
adrp	x1, ADDR
ldr	x1, [x1, #1160]
str	x1, [sp]
bl	ADDR
ldr	x0, [sp, #8]
mov	x2, #0x58                  	// #88
mov	x5, #0x1                   	// #1
add	x4, sp, #0xc8
mov	w1, w5
add	x3, sp, #0xb0
str	x2, [sp, #176]
str	xzr, [sp, #200]
bl	ADDR
mov	x1, x0
str	x1, [sp]
ldr	x0, [sp, #8]
bl	ADDR
ldr	x1, [sp]
cbz	x1, ADDR
sub	x2, x1, x21
add	x4, sp, #0x1b8
cmp	x2, #0x1
sub	x1, x21, x4
csetm	x0, ne	// ne = any
mov	x3, #0x1                   	// #1
and	x0, x0, x1
mov	x1, x24
add	x0, x0, x2
str	x0, [sp, #440]
cmp	x0, x3
add	x2, x4, x0
csetm	x0, ne	// ne = any
str	x4, [sp, #8]
movi	v0.4s, #0x0
and	x0, x0, x2
adrp	x2, ADDR
ldr	x2, [x2, #1160]
str	x2, [sp]
str	x3, [x0], #8
stp	q0, q0, [x0]
stp	q0, q0, [x0, #32]
str	q0, [x0, #64]
ldr	x0, [sp, #440]
cmp	x0, x3
add	x2, x4, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
add	x0, x0, #0x8
sub	x2, x0, x20
str	x2, [sp, #328]
cmp	x2, x3
csetm	x2, ne	// ne = any
and	x5, x2, x0
str	x5, [sp, #16]
mov	x0, x5
bl	ADDR
ldr	x5, [sp, #16]
add	x1, sp, #0x390
ldr	x0, [sp, #640]
sub	x3, x1, x5
ldr	x1, [sp, #48]
cmp	x0, #0x1
sub	x3, x3, #0x130
mov	x4, x5
add	x2, x1, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
add	x7, sp, #0x270
cmp	x0, #0x0
sub	x0, x0, x5
csetm	x2, ne	// ne = any
sub	x0, x0, #0x21
and	x0, x2, x0
add	x1, sp, #0x2a8
add	x0, x0, #0x1
str	x0, [x5, #32]
adrp	x0, ADDR
add	x6, sp, #0x178
ldr	x2, [sp, #648]
str	x6, [sp]
ldr	q0, [x0, #544]
adrp	x0, ADDR
cmp	x2, #0x1
ldr	q2, [x0, #1792]
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x2
str	x0, [x5, #40]
add	x2, sp, #0x270
adrp	x0, ADDR
ldr	q3, [sp, #656]
ldr	q1, [x0, #624]
str	q3, [x5, #48]
str	q0, [x4, #64]!
ldr	x0, [sp, #672]
stur	q2, [x2, #24]
ldr	x2, [sp, #32]
cmp	x0, #0x1
stur	q1, [x7, #40]
add	x2, x2, x0
csetm	x0, ne	// ne = any
and	x0, x0, x2
cmp	x0, #0x0
sub	x0, x0, #0x1
csetm	x2, ne	// ne = any
sub	x0, x0, x4
and	x0, x2, x0
sub	x2, x21, x6
add	x0, x0, #0x1
str	x0, [x5, #64]
ldr	x3, [sp, #8]
ldr	x0, [sp, #680]
str	d0, [sp, #680]
sub	x3, x3, x21
cmp	x0, #0x1
add	x1, x1, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
cmp	x0, #0x0
sub	x0, x0, x5
csetm	x1, ne	// ne = any
sub	x0, x0, #0x49
and	x0, x1, x0
add	x0, x0, #0x1
str	x0, [x5, #72]
ldr	x0, [sp, #440]
cmp	x0, #0x1
csetm	x1, ne	// ne = any
and	x1, x1, x3
add	x1, x1, x0
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x2
add	x0, x0, x1
stp	x25, x0, [sp, #368]
ldr	x1, [x22, #8]
ldr	x3, [x22, #24]
add	x2, x1, #0x1
cmp	x2, x3
b.ls	ADDR  // b.plast
ucvtf	s2, x2
ldr	s3, [x22, #16]
ucvtf	s1, x1
fmov	x3, d0
fdiv	s2, s2, s3
fdiv	s1, s1, s3
fcvtpu	x2, s2
fcvtpu	x0, s1
cmp	x2, #0x0
csel	x2, x2, x3, ne	// ne = any
cmp	x0, #0x0
ccmp	x1, #0x0, #0x4, eq	// eq = none
b.ne	ADDR  // b.any
cmp	x2, x0
csel	x2, x2, x0, cs	// cs = hs, nlast
adrp	x3, ADDR
mov	x1, #0xd                   	// #13
add	x3, x3, #0xbe0
mov	x0, #0x0                   	// #0
b	ADDR
add	x0, x0, #0x1
cmp	x0, #0x25
b.eq	ADDR  // b.none
ldr	x1, [x3, x0, lsl #3]
cmp	x1, x2
b.cc	ADDR  // b.lo, b.ul, b.last
ldr	x0, [x25, #16]
cmp	x1, x0
b.eq	ADDR  // b.none
mov	x0, x22
bl	ADDR
ldr	x1, [x25, #8]
ldr	x0, [sp, #64]
bl	ADDR
ldr	d0, [sp, #96]
mov	x1, x0
add	x2, sp, #0x8
add	x0, sp, #0x68
fmov	d1, x19
mov	x8, x19
ld1	{v0.d}[1], [x0]
mov	x0, x25
ld1	{v1.d}[1], [x2]
sub	v1.2d, v1.2d, v0.2d
str	q1, [sp, #16]
bl	ADDR
adrp	x0, ADDR
ldr	q2, [sp, #432]
ldr	q0, [x0, #544]
ldr	q1, [sp, #16]
cmeq	v0.2d, v2.2d, v0.2d
ldr	x0, [sp, #376]
bic	v0.16b, v1.16b, v0.16b
add	v0.2d, v0.2d, v2.2d
str	q0, [sp, #336]
cmp	x0, #0x1
ldr	x1, [sp]
csetm	x2, ne	// ne = any
mov	x3, #0x1                   	// #1
str	x3, [sp, #376]
sub	x1, x1, x27
sub	x27, x27, x20
and	x2, x2, x1
ldr	x1, [sp, #96]
add	x2, x2, x0
mov	x0, x21
str	x2, [sp]
str	x2, [sp, #320]
bl	ADDR
ldr	x2, [sp]
mov	x1, x21
mov	x0, x19
cmp	x2, #0x1
csetm	x3, ne	// ne = any
and	x3, x3, x27
add	x2, x3, x2
str	x2, [sp, #328]
bl	ADDR
mov	x1, x19
mov	x0, x25
bl	ADDR
ldr	x2, [sp, #384]
ldr	x0, [sp, #328]
cmp	x2, #0x1
add	x3, x21, x2
csetm	x2, ne	// ne = any
and	x2, x2, x3
cmp	x0, #0x1
csetm	x4, ne	// ne = any
add	x0, x20, x0
and	x4, x4, x0
mov	x0, x21
ldr	x5, [x2]
sub	x2, x2, x4
ldr	x1, [sp, #96]
cmp	x5, #0x1
csetm	x3, ne	// ne = any
and	x2, x3, x2
add	x2, x2, x5
str	x2, [x4]
ldr	x2, [sp, #384]
ldr	x4, [sp, #328]
cmp	x2, #0x1
csetm	x3, ne	// ne = any
add	x2, x21, x2
cmp	x4, #0x1
and	x3, x3, x2
sub	x5, x20, x3
csetm	x2, ne	// ne = any
and	x2, x2, x5
sub	x20, x20, x19
add	x2, x2, x4
str	x2, [x3]
ldr	x2, [x22, #8]
add	x2, x2, #0x1
str	x2, [x22, #8]
ldr	x3, [sp, #320]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x27
add	x2, x2, x3
str	x2, [sp, #328]
bl	ADDR
ldr	x3, [sp, #328]
mov	x1, x21
ldr	x0, [sp, #8]
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x20
add	x2, x2, x3
str	x2, [sp, #432]
bl	ADDR
mov	x0, x28
bl	ADDR
b	ADDR
add	x20, sp, #0x268
add	x21, sp, #0x1b0
add	x1, x0, x20
b	ADDR
mov	x1, x0
mov	x8, x24
mov	x3, x20
mov	x2, x19
mov	x0, x25
bl	ADDR
mov	x19, #0x0                   	// #0
b	ADDR
ldr	x1, [x22, #88]
add	x0, x22, #0x58
add	x27, sp, #0x140
str	x0, [sp, #32]
cmp	x1, #0x1
sub	x2, x0, x27
csetm	x0, ne	// ne = any
add	x24, x22, #0x20
and	x0, x0, x2
add	x0, x0, x1
str	x0, [sp, #320]
cmp	x0, #0x1
add	x0, x27, x0
csetm	x21, ne	// ne = any
and	x21, x21, x0
mov	x0, x21
bl	ADDR
ldr	x1, [x24, #8]
bl	ADDR
add	x19, sp, #0x1b0
mov	x1, x0
mov	x8, x19
mov	x0, x24
add	x24, sp, #0x260
bl	ADDR
mov	x0, x24
mov	x1, x19
bl	ADDR
ldr	x0, [x22, #8]
cbz	x0, ADDR
ldr	x0, [sp, #608]
cmp	x0, #0x1
add	x0, x24, x0
csetm	x4, ne	// ne = any
and	x4, x4, x0
ldr	x0, [x4]
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x27, x21, #0x8
add	x28, sp, #0x180
ldr	x5, [x21, #16]
sub	x2, x27, x28
add	x21, sp, #0x1d0
add	x26, sp, #0x170
str	x2, [sp, #40]
sub	x2, x28, x21
add	x1, sp, #0x160
str	x2, [sp, #48]
sub	x2, x27, x26
str	x1, [sp, #16]
sub	x1, x1, x26
str	x2, [sp, #56]
sub	x2, x26, x28
str	x2, [sp, #64]
str	x1, [sp, #72]
b	ADDR
ldr	x0, [x3]
mov	x4, x3
cmp	x0, #0x1
b.eq	ADDR  // b.none
add	x3, x4, x0
add	x0, x3, #0x8
ldr	x1, [x0, #16]
cmp	x5, x1
b.ne	ADDR  // b.any
ldr	x1, [sp, #16]
add	x0, x0, #0x8
ldr	x9, [x27]
sub	x8, x0, x1
ldr	x1, [sp, #40]
cmp	x9, #0x1
str	x4, [sp, #80]
csetm	x0, ne	// ne = any
ldr	x4, [sp, #56]
and	x1, x0, x1
add	x1, x1, x9
stp	x3, x5, [sp]
and	x0, x0, x4
ldr	x4, [sp, #48]
cmp	x1, #0x1
add	x0, x0, x9
csetm	x9, ne	// ne = any
and	x9, x9, x4
cmp	x0, #0x1
ldr	x4, [sp, #64]
add	x9, x9, x1
csetm	x1, ne	// ne = any
add	x0, x5, x0
and	x1, x1, x4
str	x9, [sp, #464]
add	x0, x1, x0
str	x0, [sp, #384]
mov	x2, x26
mov	x1, x28
ldr	x10, [x3, #16]
mov	x0, x21
ldr	x3, [sp, #72]
cmp	x10, #0x1
csetm	x9, ne	// ne = any
and	x8, x9, x8
add	x8, x8, x10
cmp	x8, #0x1
csetm	x9, ne	// ne = any
and	x9, x9, x3
add	x8, x9, x8
str	x8, [sp, #368]
bl	ADDR
ldp	x3, x5, [sp]
tbz	w0, #0, ADDR
ldr	x4, [sp, #80]
add	x27, sp, #0x150
mov	x1, x19
mov	x0, x21
sub	x7, x4, x27
str	x4, [sp]
ldr	x5, [x4]
cmp	x5, #0x1
csetm	x3, ne	// ne = any
and	x3, x3, x7
add	x3, x3, x5
str	x3, [sp, #336]
bl	ADDR
ldr	x4, [sp]
ldr	x0, [x4]
cmp	x0, #0x1
add	x1, x4, x0
csetm	x0, ne	// ne = any
and	x0, x0, x1
sub	x3, x0, x4
ldr	x1, [x0]
cmp	x1, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x3
add	x0, x0, x1
str	x0, [x4]
ldr	x0, [sp, #464]
cmp	x0, #0x1
add	x0, x21, x0
csetm	x1, ne	// ne = any
and	x0, x1, x0
ldr	x19, [x0]
cmp	x19, #0x1
b.eq	ADDR  // b.none
ldr	x3, [sp, #336]
sub	x27, x27, x24
mov	x1, x24
mov	x0, x22
cmp	x3, #0x1
csetm	x2, ne	// ne = any
and	x2, x2, x27
add	x2, x2, x3
str	x2, [sp, #608]
bl	ADDR
ldr	x0, [x22, #8]
sub	x0, x0, #0x1
str	x0, [x22, #8]
ldr	x1, [sp, #32]
mov	x0, x24
bl	ADDR
mov	x1, x24
mov	x0, x22
bl	ADDR
mov	x0, x24
bl	ADDR
b	ADDR
b.ne	ADDR  // b.any
ldrb	w0, [x20]
strb	w0, [x3, w1, uxtw]
b	ADDR
ldr	w0, [x22, #40]
b	ADDR
ldr	x2, [sp, #616]
sub	x20, x20, x21
mov	x1, x26
cmp	x2, #0x1
csetm	x0, ne	// ne = any
and	x0, x0, x20
add	x0, x0, x2
str	x0, [sp, #432]
cmp	x0, #0x1
add	x0, x21, x0
csetm	x2, ne	// ne = any
and	x0, x2, x0
bl	ADDR
ldr	x2, [sp, #616]
mov	x1, x22
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x7                   	// #7
str	w1, [x0]
b	ADDR
ldr	x1, [sp, #16]
ldrb	w0, [x21, #40]
cmp	x1, #0x1
b.eq	ADDR  // b.none
ldr	x1, [sp, #16]
cmp	x1, #0x2
b.eq	ADDR  // b.none
tbz	w0, #0, ADDR
ldr	x1, [x25, #40]
ldr	w0, [x25, #8]
cmp	x1, x0
b.cs	ADDR  // b.hs, b.nlast
ldr	w2, [x25, #12]
add	x0, x21, #0x30
ldr	w1, [x25, #4]
strb	wzr, [x21, #40]
add	w1, w2, w1
bl	ADDR
ldr	x0, [x25, #40]
add	x0, x0, #0x1
str	x0, [x25, #40]
mov	x2, x20
mov	x1, x21
mov	x0, x25
mov	x19, #0x0                   	// #0
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x2                   	// #2
str	w1, [x0]
b	ADDR
sub	x2, x2, #0x1d
adrp	x1, ADDR
add	x1, x1, #0x3c8
ldr	x1, [x1, x2, lsl #3]
blr	x1
mov	x1, x0
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
mov	w1, #0xfffffff3            	// #-13
movk	x1, #0x17f, lsl #32
b	ADDR
mov	x1, x21
mov	x0, x24
bl	ADDR
ldr	x5, [sp, #608]
sub	x1, x24, x26
ldr	x4, [sp, #616]
cmp	x5, #0x1
csetm	x3, ne	// ne = any
add	x0, sp, #0x268
cmp	x4, #0x1
sub	x0, x0, x28
and	x3, x3, x1
csetm	x1, ne	// ne = any
and	x1, x1, x0
add	x3, x3, x5
add	x1, x1, x4
str	x3, [sp, #368]
cmp	x1, #0x1
add	x5, x28, x1
csetm	x4, ne	// ne = any
cmp	x3, #0x1
and	x5, x4, x5
cset	x0, eq	// eq = none
str	x1, [sp, #384]
add	x2, x26, x3
sub	x0, x0, #0x1
and	x0, x0, x2
ldp	x2, x7, [x5]
cmp	x2, #0x1
add	x2, x5, x2
csetm	x3, ne	// ne = any
and	x2, x3, x2
sub	x0, x0, x2
asr	x0, x0, #3
lsl	x0, x19, x0
bic	x0, x7, x0
str	x0, [x5, #8]
cbnz	x0, ADDR
ldr	x0, [sp, #16]
sub	x6, x28, x0
and	x4, x4, x6
add	x1, x4, x1
str	x1, [sp, #352]
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x11                  	// #17
str	w1, [x0]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x11                  	// #17
str	w1, [x0]
b	ADDR
ldr	x2, [sp, #608]
mov	x19, x0
ldr	x0, [sp, #48]
mov	x20, x1
cmp	x2, x0
b.ne	ADDR  // b.any
mov	x0, x19
cmp	x20, #0x1
b.eq	ADDR  // b.none
bl	ADDR
adrp	x2, ADDR
ldr	x2, [x2, #1160]
adrp	x1, ADDR
mov	x0, x24
add	x1, x1, #0x490
str	x2, [sp]
bl	ADDR
mov	x1, x24
mov	x0, x21
bl	ADDR
mov	x0, x24
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
mov	x2, x0
ldr	x0, [sp, #904]
ldr	x1, [x2]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.eq	ADDR  // b.none
bl	ADDR
bl	ADDR
mov	x21, x0
add	x1, x0, #0x40
adrp	x22, ADDR
mov	w2, #0x10f                 	// #271
add	x3, x22, #0xf00
adrp	x0, ADDR
add	x0, x0, #0xf70
str	x3, [sp, #176]
str	w2, [sp, #184]
str	x0, [sp, #192]
ldr	w23, [x1]
add	x0, x21, #0x98
ldrb	w0, [x0]
ands	w25, w0, #0xff
ccmp	w23, #0x4, #0x4, eq	// eq = none
b.gt	ADDR
adrp	x0, ADDR
add	x1, sp, #0x280
mov	x7, x1
str	x7, [sp, #48]
ldr	q0, [x0, #304]
str	x20, [sp, #480]
mov	x20, x7
add	x7, sp, #0x270
adrp	x6, ADDR
add	x6, x6, #0x8c0
str	x20, [sp, #608]
add	x24, sp, #0x260
add	x26, sp, #0x1d0
adrp	x1, ADDR
stur	q0, [x7, #-8]
mov	x0, x24
mov	x4, x26
add	x1, x1, #0x230
mov	x5, #0x0                   	// #0
mov	x3, #0xee                  	// #238
mov	x2, #0x4a                  	// #74
str	x19, [sp, #464]
str	x6, [sp, #632]
bl	ADDR
ldp	x2, x3, [x21, #8]
add	x1, sp, #0x70
ldp	x8, x9, [sp, #176]
stp	x8, x9, [sp, #112]
add	x19, sp, #0xc8
ldr	x7, [sp, #192]
mov	x0, x19
ldr	x5, [sp, #608]
mov	w4, #0x4                   	// #4
ldr	x6, [sp, #616]
str	x7, [x1, #16]
bl	ADDR
cmp	w25, #0x0
mov	x1, x19
cset	w3, ne	// ne = any
cmp	w23, #0x4
cset	w2, le
mov	x0, x21
bl	ADDR
ldr	x0, [sp, #608]
cmp	x0, x20
b.eq	ADDR  // b.none
bl	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x2                   	// #2
str	w1, [x0]
b	ADDR
bl	ADDR
mov	w3, #0x123                 	// #291
adrp	x2, ADDR
add	x2, x2, #0xf00
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x6, ADDR
add	x6, x6, #0xf70
ldp	x8, x9, [sp, #200]
stp	x8, x9, [sp, #112]
adrp	x2, ADDR
str	x6, [sp, #216]
add	x5, x25, #0x10
str	x6, [x1, #16]
mov	x4, x19
add	x2, x2, #0x2b0
mov	x3, #0x40                  	// #64
bl	ADDR
b	ADDR
bl	ADDR
mov	w3, #0x119                 	// #281
adrp	x2, ADDR
add	x2, x2, #0xf00
str	x2, [sp, #152]
str	w3, [sp, #160]
add	x1, sp, #0x70
adrp	x6, ADDR
add	x6, x6, #0xf70
ldp	x8, x9, [sp, #152]
stp	x8, x9, [sp, #112]
adrp	x3, ADDR
str	x6, [sp, #168]
add	x5, sp, #0x90
str	x6, [x1, #16]
add	x3, x3, #0x280
mov	x4, #0x2f                  	// #47
mov	w2, #0x4                   	// #4
bl	ADDR
b	ADDR
bl	ADDR
mov	w3, #0x133                 	// #307
adrp	x2, ADDR
add	x2, x2, #0xe48
str	x2, [sp, #152]
str	w3, [sp, #160]
add	x1, sp, #0x70
adrp	x6, ADDR
add	x6, x6, #0xf70
ldp	x8, x9, [sp, #152]
stp	x8, x9, [sp, #112]
adrp	x3, ADDR
str	x6, [sp, #168]
add	x5, sp, #0x90
str	x6, [x1, #16]
add	x3, x3, #0x158
mov	x4, #0x46                  	// #70
mov	w2, #0x4                   	// #4
bl	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w1, #0x16                  	// #22
str	w1, [x0]
b	ADDR
bl	ADDR
mov	w3, #0x45                  	// #69
adrp	x2, ADDR
add	x2, x2, #0xe80
str	x2, [sp, #176]
str	w3, [sp, #184]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #176]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #192]
add	x3, x3, #0xa8
str	x5, [x1, #16]
mov	x4, #0x34                  	// #52
mov	w2, #0x4                   	// #4
bl	ADDR
b	ADDR
bl	ADDR
mov	w3, #0x4a                  	// #74
adrp	x2, ADDR
add	x2, x2, #0xdd8
str	x2, [sp, #176]
str	w3, [sp, #184]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #176]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #192]
add	x3, x3, #0x30
str	x5, [x1, #16]
mov	x4, #0x34                  	// #52
mov	w2, #0x4                   	// #4
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w3, #0x1ec                 	// #492
adrp	x2, ADDR
add	x2, x2, #0x1e8
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
ldr	w7, [x22, #72]
adrp	x6, ADDR
ldp	x8, x9, [sp, #200]
add	x6, x6, #0x2f8
stp	x8, x9, [sp, #112]
add	x5, sp, #0xb0
mov	x4, #0x2a                  	// #42
str	w7, [sp, #176]
adrp	x3, ADDR
str	x6, [sp, #216]
add	x3, x3, #0x308
str	x6, [x1, #16]
mov	w2, #0x4                   	// #4
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, #0xffffffffffffffff    	// #-1
mov	w3, #0xa6                  	// #166
adrp	x2, ADDR
add	x2, x2, #0x158
str	x2, [sp, #200]
str	w3, [sp, #208]
add	x1, sp, #0x70
adrp	x7, ADDR
add	x7, x7, #0xf70
ldp	x8, x9, [sp, #200]
stp	x8, x9, [sp, #112]
add	x6, sp, #0xb0
str	x7, [sp, #216]
add	x5, sp, #0x98
str	x7, [x1, #16]
mov	x4, #0x3b                  	// #59
adrp	x3, ADDR
mov	w2, #0x4                   	// #4
add	x3, x3, #0x1f0
bl	ADDR
b	ADDR
bl	ADDR
mov	w3, #0x12c                 	// #300
adrp	x2, ADDR
add	x2, x2, #0xe48
str	x2, [sp, #176]
str	w3, [sp, #184]
add	x1, sp, #0x70
adrp	x5, ADDR
add	x5, x5, #0xf70
ldp	x6, x7, [sp, #176]
stp	x6, x7, [sp, #112]
adrp	x3, ADDR
str	x5, [sp, #192]
add	x3, x3, #0x120
str	x5, [x1, #16]
mov	x4, #0x34                  	// #52
mov	w2, #0x4                   	// #4
bl	ADDR
b	ADDR
mov	x19, x0
mov	x0, x24
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
mov	x0, x23
bl	ADDR
ldr	x2, [sp]
ldr	x0, [sp, #904]
ldr	x1, [x2]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
mov	x0, x19
bl	ADDR
mov	x19, x0
mov	x0, x22
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
ldr	x2, [sp, #904]
ldr	x1, [x0]
subs	x2, x2, x1
mov	x1, #0x0                   	// #0
b.eq	ADDR  // b.none
b	ADDR
b	ADDR
b	ADDR
adrp	x1, ADDR
ldr	x1, [x1, #1160]
mov	x19, x0
str	x1, [sp]
b	ADDR
mov	x19, x0
mov	x0, x26
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
ldr	x0, [sp, #40]
bl	ADDR
ldr	x0, [sp, #32]
bl	ADDR
ldr	x2, [sp]
ldr	x0, [sp, #904]
ldr	x1, [x2]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.eq	ADDR  // b.none
b	ADDR
b	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
mov	x2, x0
ldr	x0, [sp, #904]
ldr	x1, [x2]
subs	x0, x0, x1
mov	x1, #0x0                   	// #0
b.ne	ADDR  // b.any
bl	ADDR
ldr	x1, [sp, #440]
mov	x19, x0
cmp	x1, #0x1
b.ne	ADDR  // b.any
mov	x0, x24
bl	ADDR
ldr	x0, [sp, #56]
bl	ADDR
mov	x0, x26
bl	ADDR
ldr	x0, [sp, #40]
bl	ADDR
b	ADDR
add	x0, sp, #0x1b8
sub	x3, x28, x21
add	x0, x0, x1
sub	x0, x0, x28
str	x0, [sp, #368]
cmp	x0, #0x1
ldr	x2, [sp, #432]
csetm	x1, ne	// ne = any
and	x1, x1, x3
add	x0, x1, x0
str	x0, [sp, #384]
add	x21, x21, x0
ldr	x1, [x2]
cmp	x1, #0x1
add	x2, x2, x1
cset	x20, eq	// eq = none
cmp	x0, #0x1
sub	x20, x20, #0x1
csetm	x0, ne	// ne = any
and	x20, x20, x2
ands	x21, x0, x21
b.eq	ADDR  // b.none
mov	x0, x20
bl	ADDR
mov	x1, x21
mov	x0, x20
bl	ADDR
mov	x0, x20
bl	ADDR
b	ADDR
b	ADDR
adrp	x1, ADDR
ldr	x1, [x1, #1160]
mov	x19, x0
str	x1, [sp]
b	ADDR
mov	x19, x0
mov	x0, x28
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
b	ADDR
adrp	x1, ADDR
ldr	x1, [x1, #1160]
mov	x19, x0
str	x1, [sp]
b	ADDR
mov	x0, x2
bl	ADDR
b	ADDR
bl	ADDR
add	x22, x22, #0xf00
ldr	x1, [x0]
ldr	x1, [x1, #16]
blr	x1
mov	x5, x0
mov	w4, #0x10f                 	// #271
adrp	x0, ADDR
mov	x3, x24
mov	x8, x26
add	x0, x0, #0x480
mov	x2, #0x1cc                 	// #460
mov	x1, #0xb                   	// #11
str	x5, [sp, #608]
str	x22, [sp, #624]
str	w4, [sp, #640]
bl	ADDR
mov	x1, x26
mov	x0, x21
bl	ADDR
mov	x0, x26
bl	ADDR
bl	ADDR
b	ADDR
bl	ADDR
mov	x19, x0
mov	x0, x26
bl	ADDR
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
b	ADDR
mov	x19, x0
b	ADDR
mov	x19, x0
mov	x0, x24
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
bl	ADDR
b	ADDR
mov	x19, x0
bl	ADDR
b	ADDR
adrp	x1, ADDR
ldr	x1, [x1, #1160]
mov	x19, x0
str	x1, [sp]
b	ADDR
adrp	x1, ADDR
ldr	x1, [x1, #1160]
mov	x19, x0
str	x1, [sp]
b	ADDR
mov	x19, x0
mov	x0, x24
bl	ADDR
adrp	x0, ADDR
ldr	x0, [x0, #1160]
str	x0, [sp]
b	ADDR
b	ADDR
