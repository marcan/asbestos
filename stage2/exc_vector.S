/*  exc_vector.S - exception handling vector

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"

	.text
	.section .start,"ax",@progbits
	.align 3

	.global _exc_vec, _exc_vec_end, _exception_start

_exc_vec:
	std r0, 0x2000(0)
	mflr r0
	std r0, 0x2110(0)
	lis r0, _exception_start@h
	ori r0, r0, _exception_start@l
	mtlr r0
	li r0, 0 /* exception number */
	blr
_exc_vec_end:

_exception_start:
	std r1, 0x2008(0)
	std r2, 0x2010(0)
	std r3, 0x2018(0)
	std r4, 0x2020(0)
	std r5, 0x2028(0)
	std r6, 0x2030(0)
	std r7, 0x2038(0)
	std r8, 0x2040(0)
	std r9, 0x2048(0)
	std r10, 0x2050(0)
	std r11, 0x2058(0)
	std r12, 0x2060(0)
	std r13, 0x2068(0)
	std r14, 0x2070(0)
	std r15, 0x2078(0)
	std r16, 0x2080(0)
	std r17, 0x2088(0)
	std r18, 0x2090(0)
	std r19, 0x2098(0)
	std r20, 0x20a0(0)
	std r21, 0x20a8(0)
	std r22, 0x20b0(0)
	std r23, 0x20b8(0)
	std r24, 0x20c0(0)
	std r25, 0x20c8(0)
	std r26, 0x20d0(0)
	std r27, 0x20d8(0)
	std r28, 0x20e0(0)
	std r29, 0x20e8(0)
	std r30, 0x20f0(0)
	std r31, 0x20f8(0)

	mfcr r3		; std r3, 0x2100(0)
	mfxer r3	; std r3, 0x2108(0)
	mfctr r3	; std r3, 0x2118(0)
	mfsrr0 r3	; std r3, 0x2120(0)
	mfsrr1 r3	; std r3, 0x2128(0)
	mfdar r3	; std r3, 0x2130(0)
	mfdsisr r3	; std r3, 0x2138(0)

	/* make stack space (at least 288 bytes required per ABI */
	addi sp, sp, -512
	std r0, 0(sp)
	/* exception number = arg */
	mr r3, r0
	/* set up the TOC register */
	lis r2, (__toc_start+0x8000)@h
	ori r2, r2, (__toc_start+0x8000)@l
	bl exception_handler

	ld r3, 0x2100(0) ; mtcr r3
	ld r3, 0x2108(0) ; mtxer r3
	ld r3, 0x2110(0) ; mtlr r3
	ld r3, 0x2118(0) ; mtctr r3
	ld r3, 0x2120(0) ; mtsrr0 r3
	ld r3, 0x2128(0) ; mtsrr1 r3

	ld r0, 0x2000(0)
	ld r1, 0x2008(0)
	ld r2, 0x2010(0)
	ld r3, 0x2018(0)
	ld r4, 0x2020(0)
	ld r5, 0x2028(0)
	ld r6, 0x2030(0)
	ld r7, 0x2038(0)
	ld r8, 0x2040(0)
	ld r9, 0x2048(0)
	ld r10, 0x2050(0)
	ld r11, 0x2058(0)
	ld r12, 0x2060(0)
	ld r13, 0x2068(0)
	ld r14, 0x2070(0)
	ld r15, 0x2078(0)
	ld r16, 0x2080(0)
	ld r17, 0x2088(0)
	ld r18, 0x2090(0)
	ld r19, 0x2098(0)
	ld r20, 0x20a0(0)
	ld r21, 0x20a8(0)
	ld r22, 0x20b0(0)
	ld r23, 0x20b8(0)
	ld r24, 0x20c0(0)
	ld r25, 0x20c8(0)
	ld r26, 0x20d0(0)
	ld r27, 0x20d8(0)
	ld r28, 0x20e0(0)
	ld r29, 0x20e8(0)
	ld r30, 0x20f0(0)
	ld r31, 0x20f8(0)

	rfid
