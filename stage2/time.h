/*  time.h - timing and timebase functions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef TIME_H
#define TIME_H

#include "types.h"

#define TIMEBASE_FREQ ((long)79800000)
#define TICKS_PER_SEC TIMEBASE_FREQ
#define TICKS_PER_MS (TIMEBASE_FREQ/1000)

u64 gettb(void);

void udelay(u64 usec);

#endif