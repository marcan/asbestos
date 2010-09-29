/*  start.S - AsbestOS stage1 startup code

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"

	.text
	.section .start,"ax",@progbits
	.align 3

	.global _start

_start:
	mflr r0
	bl 1f
1:
	mflr r4
	/* calculate address of _start */
	addi r4, r4, (_start - 1b)

	/* relocate to our real address */
	li r3, 1
	sldi r3, r3, 63
	oris r5, r3, __self_start@h
	ori r5, r5, __self_start@l
	oris r6, r3, __self_end@h
	ori r6, r6, __self_end@l

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
	oris r4, r3, reloced_start@h
	ori r4, r4, reloced_start@l
	mtctr r4
	bctr

reloced_start:
	std r0, 0x10(sp)
	stdu sp, -0x80(sp)

	/* register the USB device driver */
	oris r3, r3, usb_driver@h
	ori r3, r3, usb_driver@l
	bl _usbRegisterDriver

	/* return from exploit */
return:
	addi sp, sp, 0x80
	ld r0, 0x10(sp)
	mtlr r0
	li r3, 0
	blr

.global __thread_catch
.global __thread_catch_end

/* code to overwrite the decrementer interrupt */
__thread_catch:
	ld r3, 0x918(0)
	cmpldi r3, 0
	beq __thread_catch
	mtctr r3
	bctr
	nop
	.quad 0
__thread_catch_end:
