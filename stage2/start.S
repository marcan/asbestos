/*  start.S - AsbestOS stage2 startup code

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

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
	/* at this point, one thread is (hopefully) spinning in the decrementer vector */
	/* release it here (it will already be in realmode and with IRQs disabled) */
	lis r3, _threads_start@h
	ori r3, r3, _threads_start@l
	std r3, 0x918(0)

_threads_start:
	/* both threads end up running here */

	/* if this is thread 1, go to _thread1_start */
	mfspr r3, 0x88
	cntlzw. r3, r3
	bne _thread1_start

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
	li r4, 1
	std r4, 0(r3)
1:
	ld r4, 8(r3)
	cmpldi r4, 0
	beq 1b
	ld r4, 16(r3)
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
