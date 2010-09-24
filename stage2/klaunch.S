/*  klaunch.h - kernel relocation and launch code

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"

_GLOBAL(kload)
	mfmsr r5
	# disable interrupts
	rldicl r5, r5, 48, 1
	rotldi r5, r5, 16
	mtmsrd r5, 0

	# relocate to our real address
	
	lis r4, __zimage_load_base@h
	ori r4, r4, __zimage_load_base@l
	li r5, 0

_move_loop:
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
	cmpld r5, r3
	blt _move_loop

	blr

_GLOBAL(klaunch)
	ba 0x100
