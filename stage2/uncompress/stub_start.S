/*  stub_start.S - stage2 decompressor startup code

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
	mflr r0
	bl 1f
1:
	mflr r30

	# calculate our realmode base
	addi r30, r30, (_start - 1b)
	clrldi r30, r30, 1

	# calculate realmode address of realmode
	addi r3, r30, (realmode - _start)

	# switch to realmode
	isync
	mtsrr0 r3
	# set MSR[DR:IR] = 00 and jump to srr0
	mfmsr r3
	li r4, 0x30
	andc r3, r3, r4
	mtsrr1 r3
	rfid

realmode:

	mfmsr r3
	# disable interrupts
	rldicl r3, r3, 48, 1
	rotldi r3, r3, 16
	mtmsrd r3, 0

	mr r4, r30

	# relocate to our real address
	lis r5, __self_start@h
	ori r5, r5, __self_start@l
	lis r6, __lzodata@h
	ori r6, r6, __lzodata@l

	# add the size of the LZO data
	sub r7, r6, r5
	add r7, r7, r4
	lwz r7, -4(r7)
	add r6, r6, r7

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

# continue running at the relocated location
	lis r4, reloced_start@h
	ori r4, r4, reloced_start@l
	mtctr r4
	bctr

reloced_start:
# set up the TOC register
	lis r2, (__toc_start+0x8000)@h
	ori r2, r2, (__toc_start+0x8000)@l
# set up the stack
	lis r1, _stack_bot@h
	ori r1, r1, _stack_bot@l

# clear BSS
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

# jump to main (panic if it returns)
	bl .main
	b panic

# shutdown
	.global panic
panic:
	li r3, 0
	li r11, LV1_PANIC
	lv1call

# reboot
	.global reboot
reboot:
	li r3, 1
	li r11, LV1_PANIC
	lv1call
