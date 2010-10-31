/*  netrpc.h - server to perform lv1 and device ops via network requests

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef NETRPC_H
#define NETRPC_H

#include "config.h"
#ifdef NETRPC_ENABLE

#include "types.h"
#include "lwip/udp.h"

void netrpc_init(void);
void netrpc_shutdown(void);

#endif
#endif