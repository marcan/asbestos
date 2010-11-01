/*  network.h - Toplevel networking functions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef NETWORK_H
#define NETWORK_H

#include "lwip/dhcp.h"

#define P_IP(w) (u8)((w)>>24), (u8)((w)>>16), (u8)((w)>>8), (u8)(w)

extern struct netif eth;

void net_init(void);
void net_poll(void);
void net_shutdown(void);

#endif