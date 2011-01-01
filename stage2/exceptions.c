/*  exceptions.c - exception handling

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "exceptions.h"
#include "mm.h"
#include "string.h"
#include "debug.h"
#include "lv1call.h"

extern u8 _exc_vec[], _exc_vec_end[];

struct exc_data *exc = (void*)0x2000;

static void print_exception(int exception)
{
	int i;

	u32 tid;
	asm("mfspr %0, 0x88" : "=r"(tid));
	tid = 2-(tid>>30);
	printf("\nException %04x occurred on thread %d!\n", exception, tid);

	for (i=0; i<32; i+=4) {
		printf("r%d-%d: ", i, i+3);
		if (i<8) printf("  ");
		if (i==8) printf(" ");
		printf("%16lx %16lx %16lx %16lx\n", exc->regs[i], exc->regs[i+1], exc->regs[i+2], exc->regs[i+3]);
	}

	printf("LR:    %016lx\n", exc->lr);
	printf("CTR:   %016lx\n", exc->ctr);
	printf("CR:    %08lx\n", exc->cr);
	printf("XER:   %08lx\n", exc->xer);
	printf("DAR:   %016lx\n", exc->dar);
	printf("DSISR: %08lx\n", exc->dsisr);
	printf("SRR0:  %016lx\n", exc->srr0);
	printf("SRR1:  %016lx\n", exc->srr1);

}

void exception_handler(int exception)
{
	switch(exception) {
		case 0x300: // Data Storage
			if ((exc->dsisr & (1<<30)) && mm_loadhtab(exc->dar))
				return;
			break;
		case 0x380: // Data Segment
			if (mm_loadseg(exc->dar))
				return;
			break;
		case 0x400: // Instruction Storage
			if ((exc->srr1 & (1<<30)) && mm_loadhtab(exc->srr0))
				return;
			break;
		case 0x480: // Instruction Segment
			if (mm_loadseg(exc->srr0))
				return;
			break;
	}
	print_exception(exception);
	printf("\nUnhandled exception. Panicking...\n");
	lv1_panic(0);
	while(1);
}

void exceptions_init(void)
{
	u64 addr;

	printf("Installing exception handlers...\n");

	for (addr=0x100; addr<0x2000; addr+=0x20) {
		u32 *p = (u32*)addr;
		memcpy(p, _exc_vec, _exc_vec_end - _exc_vec);
		p[6] |= addr; // patch exception number
		sync_before_exec(p, 0x20);
	}
}
