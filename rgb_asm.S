 @*****************************************************************************
 @ 2015 olegk0 <olegvedi@gmail.com>
 @
 @ This program is free software; you can redistribute it and/or modify
 @ it under the terms of the GNU Lesser General Public License as published by
 @ the Free Software Foundation; either version 2.1 of the License, or
 @ (at your option) any later version.
 @
 @ This program is distributed in the hope that it will be useful,
 @ but WITHOUT ANY WARRANTY; without even the implied warranty of
 @ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 @ GNU Lesser General Public License for more details.
 @
 @ You should have received a copy of the GNU Lesser General Public License
 @ along with this program; if not, write to the Free Software Foundation,
 @ Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 @****************************************************************************/

	.syntax unified
	.arch armv7-a
	.fpu neon
	.text

#define AlphaThreshold	100


#define ARGB	r0
#define I8	r1
#define WIDTH	r2
#define HEIGHT	r3
#define A8	r4
#define DPITCH	r5
#define DPAD	r5
	@ Color Table
#define CT	r6
#define SPITCH	r8
#define SPAD	r8
#define CNT	lr

    .global iargb_argb_asm
    .type	iargb_argb_asm, %function
iargb_argb_asm:
    push	{r4-r9,lr}
    ldmia	r0, {ARGB, DPITCH}
    ldmia	r1, {I8, A8, CT, SPITCH}

    cmp		HEIGHT, #0
    sub		DPAD, DPITCH, WIDTH, lsl #2
    sub		SPAD, SPITCH, WIDTH, lsl #1
1:
    movsgt	CNT, WIDTH
    pople	{r4-r9,pc}
2:
    ldrb	r7, [I8], #2 @ index RGB
    ldrb	r9, [A8], #2 @ alpha channel
    ldr		r7, [CT, r7, lsl #2] @ get RGB from table
    orr		r7, r7, r9, lsl #24 @ mix RGB and Alpha
    str		r7, [ARGB], #4

    subs	CNT, #1
    bne		2b

    subs	HEIGHT, #1
    add		ARGB, DPAD
    add		I8, SPAD
    add		A8, SPAD
    b		1b

@*******************************************************************
#define ARGB	r0
#define I8	r1
#define WIDTH	r2
#define HEIGHT	r3
#define ALPHA	r4
#define DPITCH	r5
#define DPAD	r5
#define A8	r6
	@ Color Table
#define CT	r7
#define SPITCH	r8
#define SPAD	r8
#define CNT	lr

    .global iargb_argb_arp_asm
    .type	iargb_argb_arp_asm, %function
iargb_argb_arp_asm:
    push	{r4-r10,lr}
    ldmia	r0, {ARGB, ALPHA, DPITCH}
    ldmia	r1, {I8, A8, CT, SPITCH}

    cmp		HEIGHT, #0
    sub		DPAD, DPITCH, WIDTH, lsl #2
    sub		SPAD, SPITCH, WIDTH, lsl #1
1:
    movsgt	CNT, WIDTH
    pople	{r4-r10,pc}
2:
    ldrb	r9, [A8], #2 @ alpha channel

    cmp		r9, #AlphaThreshold
    movlt	r10, ALPHA
    addlt	I8, #2

    ldrbge	r10, [I8], #2 @ index RGB
    ldrge	r10, [CT, r10, lsl #2] @ get RGB from table
    orrge	r10, r10, r9, lsl #24 @ mix RGB and Alpha
    str		r10, [ARGB], #4

    subs	CNT, #1
    bne		2b

    subs	HEIGHT, #1
    add		ARGB, DPAD
    add		I8, SPAD
    add		A8, SPAD
    b		1b

@*******************************************************************
#define ARGB	r0
#define A8	r1
#define WIDTH	r2
#define HEIGHT	r3
#define COLOR	r4
#define DPITCH	r5
#define DPAD	r5
#define SPITCH	r6
#define SPAD	r6
#define CNT	lr

    .global a8_argb_asm
    .type	a8_argb_asm, %function
a8_argb_asm:
    push	{r4-r8,lr}
    vpush	{d0-d2}

    ldmia	r0, {ARGB, DPITCH}
    ldmia	r1, {A8, COLOR, SPITCH}

    cmp		HEIGHT, #1
    blt		3f
    cmp		WIDTH, #2
    blt		3f

    bic		WIDTH, #1

    sub		DPAD, DPITCH, WIDTH, lsl #2
    sub		SPAD, SPITCH, WIDTH

    vdup.32	d2, COLOR
1:
    mov		CNT, WIDTH
2:
    ldrb	r7, [A8], #1 @ alpha channel
    ldrb	r8, [A8], #1 @ alpha channel

    vdup.8	d0, r7
    vdup.8	d1, r8
@	d0 = s0,s1
@	d1 = s2,s3
    vmov	s1, s2 @ => d0

    vmull.u8    q0, d2, d0
    vshrn.u16   d0, q0, #8 @ / 256

    vst1.8	{d0}, [ARGB]! @ 2px - 8 bytes

    subs	CNT, #2
    bne		2b

    subs	HEIGHT, #1
    add		ARGB, DPAD
    add		A8, SPAD
    bne		1b
3:
    vpop	{d0-d2}
    pop		{r4-r8,pc}

@*******************************************************************
#define ARGB	r0
#define A8	r1
#define WIDTH	r2
#define HEIGHT	r3
#define ALPHA	r4
#define DPITCH	r5
#define DPAD	r5
#define COLOR	r6
#define SPITCH	r7
#define SPAD	r7
#define CNT	lr

    .global a8_argb_arp_asm
    .type	a8_argb_arp_asm, %function
a8_argb_arp_asm:
    push	{r4-r10,lr}
    vpush	{d0-d2}

    ldmia	r0, {ARGB, ALPHA, DPITCH}
    ldmia	r1, {A8, COLOR, SPITCH}

    cmp		HEIGHT, #1
    blt		3f
    cmp		WIDTH, #2
    blt		3f

    bic		WIDTH, #1

    sub		DPAD, DPITCH, WIDTH, lsl #2
    sub		SPAD, SPITCH, WIDTH

    vdup.32	d2, COLOR
mov	r10, #AlphaThreshold
lsl	r10, #1
1:
    mov		CNT, WIDTH
2:
    ldrh	r8, [A8], #2 @ 2px - 2bytes alpha channel

and	r9, r8, #255
add	r9, r9, r8, lsr #8
cmp	r9, r10
vmovlt	s0, ALPHA
vmovlt	s1, ALPHA
blt	4f

    vmov.16	d0[0], r8
    vdup.8	d1, d0[1]
    vdup.8	d0, d0[0]
@	d0 = s0,s1
@	d1 = s2,s3
    vmov	s1, s2 @ => d0

    vmull.u8    q0, d2, d0
    vshrn.u16   d0, q0, #8 @ / 256
4:

    vst1.8	{d0}, [ARGB]! @ 2px - 8 bytes

    subs	CNT, #2
    bne		2b

    subs	HEIGHT, #1
    add		ARGB, DPAD
    add		A8, SPAD
    bne		1b
3:
    vpop	{d0-d2}
    pop		{r4-r10,pc}

@*******************************************************************


#define DST	r0
#define SRC	r1
#define WIDTH	r2
#define HEIGHT	r3
#define SPITCH0	r4
#define SPAD0	r4
#define DPITCH	r5
#define DPAD	r5
#define CNT	lr

    .global copy_bytes_asm
    .type copy_bytes_asm, %function
copy_bytes_asm:
    push	{r4-r6,lr}
    ldmia	r0,	{DST, DPITCH}
    ldmia	r1,	{SRC, SPITCH0}

    cmp		HEIGHT,	#1
    blt		3f
    cmp		WIDTH, #1
    blt		3f

    sub		DPAD, DPITCH, WIDTH
    sub		SPAD0, SPITCH0, WIDTH
1:
    mov		CNT, WIDTH

copyloopb:

    ldrb	r6, [SRC], #1
    strb	r6, [DST], #1
    subs	CNT, #1
    bne		copyloopb

    add		SRC, SPAD0
    add		DST, DPAD
    subs	HEIGHT, #1
    bgt		1b
3:
    pop		{r4-r6,pc}

@*******************************************************************

#define DST	r0
#define SRC	r1
#define WIDTH	r2
#define HEIGHT	r3
#define SPITCH0	r4
#define SPAD0	r4
#define DPITCH	r5
#define DPAD	r5
#define CNT	lr

    .global copy_words_asm
    .type copy_words_asm, %function
copy_words_asm:
    push	{r4-r6,lr}
    ldmia	r0,	{DST, DPITCH}
    ldmia	r1,	{SRC, SPITCH0}

    cmp		HEIGHT,	#1
    blt		3f
    cmp		WIDTH, #1
    blt		3f

    sub		DPAD, DPITCH, WIDTH, lsl #2
    sub		SPAD0, SPITCH0, WIDTH, lsl #2
1:
    mov		CNT, WIDTH

copyloopw:

    ldr		r6, [SRC], #4
    str		r6, [DST], #4
    subs	CNT, #1
    bne		copyloopw

    add		SRC, SPAD0
    add		DST, DPAD
    subs	HEIGHT, #1
    bgt		1b
3:
    pop		{r4-r6,pc}
