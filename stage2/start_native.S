/*  start.S - AsbestOS stage2 startup code

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"
#define LV1_PANIC 255

	.text
	.section .vectors,"ax",@progbits
	.align 3

	.global _entry

_zero:
	.space 0x60

_thread1_entry:
	b _start
	.space 0x100 - (.-_zero)

_entry:
	/* both threads end up running here */

	/* calculate our base */
	bl 1f
1:
	mflr r30
	addi r30, r30, (_zero - 1b)
	clrldi r30, r30, 1

	/* if this is thread 1, go to _thread1_start */
	mfspr r3, 0x88
	cntlzw. r3, r3
	bne _thread1_start

	b _start

	.space 0x1000 - (.-_zero)

	.text
	.section .start,"ax",@progbits
	.align 3

_start:

	/* relocate to our real address */
	mr r4, r30
	lis r5, __self_start@h
	ori r5, r5, __self_start@l
	lis r6, __self_end@h
	ori r6, r6, __self_end@l

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

	/* continue running at the relocated location */
	lis r4, reloced_start@h
	ori r4, r4, reloced_start@l
	mtctr r4
	bctr

reloced_start:

	/* make sure _thread1_active is cleared on the new copy */
	lis r3, _thread1_active@h
	ori r3, r3, _thread1_active@l
	li r0, 0
	std r0, 0(r3)

	/* now jump thread1 over to the new vector */
	lis r4, __base@h
	ori r4, r4, __base@l
	sub r5, r3, r4
	add r5, r5, r30
	lis r3, _thread1_start@h
	ori r3, r3, _thread1_start@l
	std r3, 16(r5)
	li r3, 1
	std r3, 8(r5)

_no_reloc:
	/* set up the TOC register */
	lis r2, (__toc_start+0x8000)@h
	ori r2, r2, (__toc_start+0x8000)@l
	/* set up the stack */
	lis r1, _stack_bot@h
	ori r1, r1, _stack_bot@l
	/* make space for the first stack frame */
	addi r1, r1, 64
	li r0, 0
	std r0, 0(r1)

	/* clear BSS */
	lis r3, __bss_start@h
	ori r3, r3, __bss_start@l
	lis r4, __bss_end@h
	ori r4, r4, __bss_end@l
	li r5, 0
_bss_loop:
	std r5, 0(r3)
	addi r3, r3, 8
	cmpld r3, r4
	blt _bss_loop

	/* jump to main (panic if it returns) */
	bl main
	b panic

	/* thread 1 runs this code - spin until we have a vector */
_thread1_start:
	lis r3, _thread1_active@h
	ori r3, r3, _thread1_active@l
	lis r4, __base@h
	ori r4, r4, __base@l
	sub r5, r3, r4
	add r5, r5, r30

	/* next time make sure we use the real base (if applicable) */
	lis r30, __base@h
	ori r30, r30, __base@l

	li r4, 1
	std r4, 0(r5)
1:
	ld r4, 8(r5)
	cmpldi r4, 0
	beq 1b
	ld r4, 16(r5)
	mtctr r4
	li r3, 1 /* CPU number for Linux */
	bctr

	.align 3
	.global _thread1_active, _thread1_release, _thread1_vector
_thread1_active:	.quad 0
_thread1_release:	.quad 0
_thread1_vector:	.quad 0

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
