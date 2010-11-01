/*  kernel.h - device tree manipulation and kernel loading

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

#define MAX_CMDLINE_SIZE 255

int kernel_load(const u8 *addr, u32 len);
void kernel_build_cmdline(const char *parameters, const char *root);
void kernel_set_initrd(void *start, size_t size);
void kernel_launch(void);

#endif
