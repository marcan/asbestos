/*  thunks.S - lv2 thunks for TOC fixup and lv1 calls

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "../common/assembly.h"
#define LV1_PANIC 255

	.text
	.align 3

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

.macro lv2thunk name
	.global \name
\name:
	mflr r0
	bl save
	bl _\name
	b restore
.endm

	lv2thunk usbGetDescriptor
	lv2thunk usbOpenEndpoint
	lv2thunk usbControlTransfer
	lv2thunk printf

save:
	std r0, 32(sp)
	std toc, 40(sp)
	clrrdi toc, toc, 32
	oris toc, toc, __lv2toc@h
	ori toc, toc, __lv2toc@l
	blr

restore:
	ld toc, 40(sp)
	ld r0, 32(sp)
	mtlr r0
	blr
