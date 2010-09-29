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
_bss_loop:
	std r5, 0(r3)
	addi r3, r3, 8
	cmpld r3, r4
	blt _bss_loop

	/* jump to main (panic if it returns) */
	bl main
	b panic

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
