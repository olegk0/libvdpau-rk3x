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

@    .align 2
    .global iargb_argb_asm
    .type	iargb_argb_asm, %function
iargb_argb_asm:
    push		{r4-r9,lr}
    ldmia		r0,	{ARGB, DPITCH}
    ldmia		r1,	{I8, A8, CT, SPITCH}

    cmp		HEIGHT,	#0
    sub		DPAD,	DPITCH,	WIDTH, lsl #2
    sub		SPAD,	SPITCH,	WIDTH, lsl #1
1:
    movsgt	CNT,	WIDTH
    pople	{r4-r9,pc}
2:
    ldrb	r7, [I8], #2 @ index RGB
    ldrb	r9, [A8], #2 @ alpha channel
    ldr		r7, [CT, r7, lsl #2] @ get RGB from table
    orr		r7, r7, r9, lsl #24 @ mix RGB and Alpha
    str		r7, [ARGB], #4

    subs	CNT,	#1
    bgt		2b

    subs	HEIGHT,	#1
    add		ARGB,	DPAD
    add		I8,	SPAD
    add		A8,	SPAD
    b		1b

@********************************************************************
#define bY0	r0
#define UVYA0	r1
#define WIDTH	r2
#define HEIGHT	r3
#define bUV	r4
#define DPITCH	r5
#define DPAD	r5
#define bY1	r6
#define UVYA1	r7
#define SPITCH	r8
#define SPAD	r8
#define DPITCHuv r9
#define DPADuv	r9
#define CNT	lr

    .align 2
    .global mix_yuv_semiplanar_neon
    .type mix_yuv_semiplanar_neon, %function
mix_yuv_semiplanar_neon:
    push		{r4-r9,lr}

    ldmia		r0,	{bY0, bUV, DPITCH}
    ldmia		r1,	{UVYA0, SPITCH}
    cmp		HEIGHT,	#2
    poplt	{r4-r9,pc}
@	odd lines
    add		bY1, bY0, DPITCH
    add		UVYA1, UVYA0, SPITCH
@ two lines (odd even) on one step
    mov		HEIGHT, HEIGHT, lsr #1
@ round the WIDTH up to a multiple of 8
    add		WIDTH, #7
    bic		WIDTH, #7

    mov		DPITCHuv, DPITCH
    sub		DPADuv, DPITCHuv, WIDTH
@	jump through odd line for Y
    mov		DPITCH, DPITCH, lsl #1
    mov		SPITCH, SPITCH, lsl #1
    sub		DPAD, DPITCH, WIDTH
    sub		SPAD, SPITCH, WIDTH, lsl #2
    vpush {d0-d15} @???
1:
    mov		CNT,	WIDTH
2:
@	source image
    vld4.u8	{d0, d1, d2, d3}, [UVYA0]! @ 8px - 32 bytes
    vld4.u8	{d12, d13, d14, d15}, [UVYA1]! @ 8px - 32 bytes

@	background source and destination
    vld1.u8	{d4}, [bY0] @ 8px - 8 bytes
    vld1.u8	{d5}, [bY1] @ 8px - 8 bytes
@	Y block 2 lines, mix bg and new img with alpha
@	q5 = d10,d11
    vshll.u8	q5, d4, #8
    vmlal.u8	q5, d2, d3
    vmlsl.u8	q5, d4, d3
    vshrn.u16   d2, q5, #8
    vst1.u8	d2, [bY0]! @ 8px - 8 bytes

    vshll.u8	q5, d5, #8
    vmlal.u8	q5, d14, d15
    vmlsl.u8	q5, d5, d15
    vshrn.u16   d2, q5, #8
    vst1.u8	d2, [bY1]! @ 8px - 8 bytes

@	UV block one line for yuv420
    vld1.u8	{d6}, [bUV] @ 8px - 8 bytes
@	compress in 2 times by width
@	q0 = d0,d1 src even
    vpaddl.u8	q0, q0	@ summ series 2x8bit blocks into 16 bit block
@    vzip.u16	d0, d1 @ d0-d1: mix =>  U16V16U16V16...
@    vshrn.u16	d0, q0, #1 @ average val UVUV...
@	to improve color
@	q6 = d12,d13 src odd
    vpaddl.u8	q6, q6	@ summ series 2x8bit blocks into 16 bit block
    vadd.u16	q0, q0, q6	@ summ U16V16 odd and even lines
    vzip.u16	d0, d1 @ d0-d1: mix =>  U16V16U16V16...
    vshrn.u16	d0, q0, #2 @ average val from 4 src UVUV...

@	UV block, mix bg and new img with alpha
    vshll.u8	q5, d6, #8 @ bgUV * 256
    vmlal.u8	q5, d0, d3 @ + newUV * Alpha
    vmlsl.u8	q5, d6, d3 @ - bgUV * Alpha
    vshrn.u16	d1, q5, #8 @ / 256

    vst1.u8	d1, [bUV]! @ 8px - 8 bytes

    subs	CNT, #8
    bgt		2b

    add		bY0, DPAD
    add		bY1, DPAD
    add		UVYA0, SPAD
    add		UVYA1, SPAD

    add		bUV, DPADuv

    subs	HEIGHT, #1
    bgt		1b
    vpop	{d0-d15}
    pop		{r4-r9,pc}

@********************************************************************
#define bY0	r0
#define A0	r1
#define WIDTH	r2
#define HEIGHT	r3
#define bUV	r4
#define DPITCH	r5
#define DPAD	r5
#define bY1	r6
#define Color	r7
#define A1	r7
#define SPITCH	r8
#define SPAD	r8
#define DPITCHuv r9
#define DPADuv	r9
#define CNT	lr

@		mix background yuv-semiplanar and A8 bitmap
    .align 2
    .global mix_a2yuv_semiplanar_neon
    .type mix_a2yuv_semiplanar_neon, %function
mix_a2yuv_semiplanar_neon:
    push		{r4-r9,lr}

    ldmia		r0,	{bY0, bUV, DPITCH}
    ldmia		r1,	{A0, Color, SPITCH}
    cmp		HEIGHT,	#2
    blt		end

    vpush	{d0-d15}
@	Only one color
    vmov.u32	d0[0], Color @ Color Msb XYVU
    vdup.8	d2, d0[2] @ dup Y
    vdup.8	d1, d0[1] @ dup V
    vdup.8	d0, d0[0] @ dup U
    vzip.u8	d0, d1 @ d0-d1: mix =>  UVUV...
@	odd lines
    add		bY1, bY0, DPITCH
    add		A1, A0, SPITCH
@	two lines (odd even) on one step TODO
    mov		HEIGHT, HEIGHT, lsr #1
@	round the WIDTH up to a multiple of 8
    add		WIDTH, #7
    bic		WIDTH, #7

    mov		DPITCHuv, DPITCH
    sub		DPADuv, DPITCHuv, WIDTH
@	jump through odd line for Y
    mov		DPITCH, DPITCH, lsl #1
    mov		SPITCH, SPITCH, lsl #1
    sub		DPAD, DPITCH, WIDTH
    sub		SPAD, SPITCH, WIDTH

1:
    mov		CNT,	WIDTH

2:
@	source image only Alpha channel
    vld1.u8	{d3}, [A0]! @ Alpha 8px - 8 bytes, colorY in d2
    vld1.u8	{d15}, [A1]! @ Alpha 8px - 8 bytes, colorY in d2

@	background source and destination
    vld1.u8	{d4}, [bY0] @ 8px - 8 bytes
    vld1.u8	{d5}, [bY1] @ 8px - 8 bytes
@	Y block 2 lines, mix bg and new img with alpha
@	q5 = d10,d11
    vshll.u8	q5, d4, #8
    vmlal.u8	q5, d2, d3
    vmlsl.u8	q5, d4, d3
    vshrn.u16   d6, q5, #8
    vst1.u8	d6, [bY0]! @ 8px - 8 bytes

    vshll.u8	q5, d5, #8
    vmlal.u8	q5, d2, d15
    vmlsl.u8	q5, d5, d15
    vshrn.u16   d6, q5, #8
    vst1.u8	d6, [bY1]! @ 8px - 8 bytes

@	UV block one line for yuv420
    vld1.u8	{d6}, [bUV] @ 8px - 8 bytes
@	q0 = d0,d1 src UV mix
@	UV block, mix bg and new img with alpha
    vshll.u8	q5, d6, #8 @ bgUV * 256
    vmlal.u8	q5, d0, d3 @ + newUV * Alpha
    vmlsl.u8	q5, d6, d3 @ - bgUV * Alpha
    vshrn.u16	d1, q5, #8 @ / 256

    vst1.u8	d1, [bUV]! @ 8px - 8 bytes

    subs	CNT, #8
    bgt		2b

    add		bY0, DPAD
    add		bY1, DPAD
    add		A0, SPAD
    add		A1, SPAD

    add		bUV, DPADuv

    subs	HEIGHT, #1
    bgt		1b
    vpop	{d0-d15}
end:
    pop		{r4-r9,pc}

@********************************************************************
#define CToutYUV r0
#define CTinRGB	r1
#define SIZE	r2
#define CNT	lr

#define vRc d0
#define vGc d1
#define vBc d2

#define vURc d3
#define vUGc d4
#define vUBcVRc d5

#define vVGc d6
#define vVBc d7

@	XRGB -> XYVU
    .global ct_rgb2yuv_neon
    .type ct_rgb2yuv_neon, %function
ct_rgb2yuv_neon:
    push	{r3,lr}
    cmp		SIZE,	#8
    blt		2f;

    @ round the SIZE up to a multiple of 8
    add		SIZE, #7
    bic		SIZE, #7

    vpush {d0-d15} @???
@	Load and duplicate transform coefficients
    mov		r3, #77
    vdup.8	vRc, r3
    mov		r3, #150
    vdup.8	vGc, r3
    mov		r3, #29
    vdup.8	vBc, r3

    mov		r3, #43	@-
    vdup.8	vURc, r3
    mov		r3, #85	@-
    vdup.8	vUGc, r3
    mov		r3, #128
    vdup.8	vUBcVRc, r3

    mov		r3, #107@-
    vdup.8	vVGc, r3
    mov		r3, #21@-
    vdup.8	vVBc, r3

    vmov.i64	d15, #0

1:
@	Y block
    vld4.u8	{d9, d10, d11, d12}, [CTinRGB]! @ 8px - 32 bytes - 8 word

    vmull.u8	q8, d11, vRc
    vmlal.u8	q8, d10, vGc
    vmlal.u8	q8, d9, vBc
    vshrn.u16   d14, q8, #8
@	UV block
@	U block
    vshll.u8	q8, vUBcVRc, #8
    vmlsl.u8	q8, d11, vURc
    vmlsl.u8	q8, d10, vUGc
    vmlal.u8	q8, d9, vUBcVRc
    vshrn.u16   d12, q8, #8
@	V block
    vshll.u8	q8, vUBcVRc, #8
    vmlal.u8	q8, d11, vUBcVRc
    vmlsl.u8	q8, d10, vVGc
    vmlsl.u8	q8, d9, vVBc
    vshrn.u16   d13, q8, #8
@	store
    vst4.8      {d12-d15}, [CToutYUV]! @ 8px - 8 words

    subs	SIZE, #8 @ -8px
    bgt		1b

2:
    vpop {d0-d15}
    pop		{r3,pc}

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

copyloop:

    ldrb	r6, [SRC], #1
    strb	r6, [DST], #1
    subs	CNT, #1
    bne		copyloop

    add		SRC, SPAD0
    add		DST, DPAD
    subs	HEIGHT, #1
    bgt		1b
3:
    pop		{r4-r6,pc}
