/*  device.c - lv1 device functions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "lv1call.h"

int map_dma_mem(int bus_id, int dev_id, void *start, size_t len, u64 *r_bus_addr)
{
	s64 result;
	u64 real_addr = (u64)start;
	u64 real_end = real_addr + len;
	u64 map_start = real_addr & ~0xfff;
	u64 map_end = (real_end + 0xfff) & ~0xfff;
	u64 bus_addr;

	u64 flags = 0xf800000000000000UL;

	result = lv1_allocate_device_dma_region(bus_id, dev_id, map_end - map_start, 12, 0, &bus_addr);
	if (result)
		return result;

	result = lv1_map_device_dma_region(bus_id, dev_id, map_start, bus_addr, map_end - map_start, flags);
	if (result) {
		lv1_free_device_dma_region(bus_id, dev_id, bus_addr);
		return result;
	}

	*r_bus_addr = bus_addr + real_addr - map_start;
}

int unmap_dma_mem(int bus_id, int dev_id, u64 bus_addr, size_t len)
{
	s64 result;
	
	bus_addr &= ~0xfff;
	
	result = lv1_unmap_device_dma_region(bus_id, dev_id, bus_addr, len);
	if (result)
		return result;

	return lv1_free_device_dma_region(bus_id, dev_id, bus_addr);
}
