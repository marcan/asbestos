/*  stub_start.S - stage2 decompressor startup code

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"
#define LV1_PANIC 255

	.text
	.section .start,"ax",@progbits
	.align 3

	.global _start

_start:
	/* disable interrupts */
	mfmsr r0
	rldicl r0, r0, 48, 1
	rotldi r0, r0, 16
	mtmsrd r0, 0

	/* calculate our realmode base */
	mflr r0
	bl 1f
1:
	mflr r30
	addi r30, r30, (_start - 1b)
	clrldi r30, r30, 1

	/* calculate realmode address of realmode */
	addi r3, r30, (realmode - _start)

	/* switch to realmode */
	isync
	mtsrr0 r3
	/* set MSR[DR:IR] = 00 and jump to srr0 */
	mfmsr r3
	li r4, 0x30
	andc r3, r3, r4
	mtsrr1 r3
	rfid

realmode:
	/* install thread catcher in the decrementer vector */
	li r5, 0
	std r5, 0x920(0) /* release */
	std r5, 0x928(0) /* ack */
	addi r3, r30, (__thread_catch - _start)
	li r4, 0x900
	ld r5, 0x00(r3) ; std r5, 0x00(r4)
	ld r5, 0x08(r3) ; std r5, 0x08(r4)
	ld r5, 0x10(r3) ; std r5, 0x10(r4)
	ld r5, 0x18(r3) ; std r5, 0x18(r4)
	dcbst 0, r4 ; sync ; icbi 0, r4

	/* now point all the other vectors to the decrementer */
	li r3, 0x100
	lis r4, 0x4800
	ori r4, r4, 0x0902 /* ba 0x900 */
_vector_loop:
	cmpldi r3, 0x900
	beq _vector_skip
	stw r4, 0(r3)
	dcbst 0, r3 ; sync ; icbi 0, r3
_vector_skip:
	addi r3, r3, 0x80
	cmpldi r3, 0x1800
	blt _vector_loop

	/* wait for the other thread to arrive */
_wait_loop:
	sync
	ld r3, 0x928(0)
	cmpld r3, 0
	beq _wait_loop

	/* relocate to our real address */
	mr r4, r30
	lis r5, __self_start@h
	ori r5, r5, __self_start@l
	lis r6, __lzma_end@h
	ori r6, r6, __lzma_end@l

	cmpld r4, r5
	beq _no_reloc

_reloc_loop:
	ld r8, 0(r4)
	std r8, 0(r5)
	ld r8, 8(r4)
	std r8, 8(r5)
	ld r8, 16(r4)
	std r8, 16(r5)
	ld r8, 24(r4)
	std r8, 24(r5)
	dcbst 0, r5
	sync
	icbi 0, r5
	addi r4, r4, 0x20
	addi r5, r5, 0x20
	cmpld r5, r6
	blt _reloc_loop
_no_reloc:

	/* continue running at the relocated location */
	lis r4, reloced_start@h
	ori r4, r4, reloced_start@l
	mtctr r4
	bctr

reloced_start:
	/* set up the TOC register */
	lis r2, (__toc_start+0x8000)@h
	ori r2, r2, (__toc_start+0x8000)@l
	/* set up the stack */
	lis r1, _stack_bot@h
	ori r1, r1, _stack_bot@l

	/* clear BSS */
	lis r3, __bss_start@h
	ori r3, r3, __bss_start@l
	lis r4, __bss_end@h
	ori r4, r4, __bss_end@l
	li r5, 0
	cmpld r3, r4
	beq _nobss
_bss_loop:
	std r5, 0(r3)
	addi r3, r3, 8
	cmpld r3, r4
	blt _bss_loop
_nobss:

	/* uncompress main stage2 */
	lis r3, probs@h
	ori r3, r3, probs@l
	lis r4, __lzma@h
	ori r4, r4, __lzma@l
	lis r5, 0x1000
	li r6, 0
	/* r6: unused, used to be &inSizeProcessed */
	lis r7, __stage2_base@h
	ori r7, r7, __stage2_base@l
	lis r8, 0x1000
	li r9, 0
	/* r9: unused, used to be &outSizeProcessed, see below */

	/* HACK: this is mostly 32bit code running in 64bit mode (with a few fixes)
	   It does ugly things like clear the top 32 bits of callee-save registers,
	   but we don't care */
	bl LzmaDecode

	/* if fail, panic */
	cmplwi r3, 0
	bne panic

	/* HACK: LzmaDecode returns the end of the output buffer in r4 */
	lis r3, __stage2_base@h
	ori r3, r3, __stage2_base@l
_flush_loop:
	dcbst 0, r3
	sync
	icbi 0, r3
	addi r3, r3, 32
	cmpld r3, r4
	blt _flush_loop

	b __stage2_base

/* code to overwrite the decrementer interrupt */
__thread_catch:
	li r3, 1
	std r3, 0x928(0)
__thread_loop:
	ld r3, 0x920(0)
	cmpldi r3, 0
	beq __thread_loop
	mtctr r3
	bctr
	nop
__thread_catch_end:

/* shutdown */
	.global panic
panic:
	li r3, 0
	li r11, LV1_PANIC
	lv1call

/* reboot */
	.global reboot
reboot:
	li r3, 1
	li r11, LV1_PANIC
	lv1call

	.section .bss
	.lcomm probs, 14134
