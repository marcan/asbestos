/*  exceptions.h - exception handling

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

struct exc_data {
	u64 regs[32];
	u64 cr, xer, lr, ctr, srr0, srr1, dar, dsisr;
};

struct exc_data *exc;

void exceptions_init(void);

#endif
