/*  mm.h - memory management

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef MM_H
#define MM_H

#include "types.h"

int mm_loadseg(u64 addr);
int mm_loadhtab(u64 addr);

void mm_init(void);
void mm_shutdown(void);

void sync_before_exec(void *addr, int len);

#endif
