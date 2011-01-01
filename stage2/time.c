/*  time.c - timing and timebase functions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "time.h"

u64 gettb(void)
{
	u64 tb;
	do {
		asm volatile ("mftb %0" : "=r"(tb));
	} while ((u32)tb == 0);
	return tb;
}

void udelay(u64 usec)
{
	u64 now = gettb();
	u64 later;
	u64 ticks = usec * TICKS_PER_MS / 1000;

	do {
		later = gettb();
	} while ((later - now) < ticks);
}
