/*  device.h - lv1 device functions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef DEVICE_H
#define DEVICE_H

int map_dma_mem(int bus_id, int dev_id, void *start, size_t len, u64 *bus_addr);
int unmap_dma_mem(int bus_id, int dev_id, u64 bus_addr, size_t len);

#endif
