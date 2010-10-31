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
	asm ("mftb %0" : "=r"(tb));
	return tb;
}

void udelay(u64 usec)
{
	u64 now = gettb();
	u64 later = now + (usec * (TIMEBASE_FREQ/1000)) / 1000;
	
	while (later < now)
		now = gettb();
	while (now < later)
		now = gettb();
}