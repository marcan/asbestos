/*  kernel.h - device tree manipulation and kernel loading

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

int kernel_load(const u8 *addr, u32 len);
void kernel_launch(void);

#endif
