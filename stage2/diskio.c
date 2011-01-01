/* diskio .c - LV1 storage functions

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "debug.h"
#include "device.h"
#include "lv1call.h"
#include "diskio.h"
#include "string.h"

#define REGION 0
#define FLAGS 0x22
#define BUFFER_SECTORS 32

static int bus_id;
static int dev_id;

static u64 bus_addr;

static int initialized = 0;

static u8 buffer[512*BUFFER_SECTORS];

DSTATUS disk_initialize(BYTE id)
{
	s64 result;

	if (id != 0)
		return STA_NODISK;

	if (initialized)
		return STA_PROTECT;

	printf("Initializing HDD...\n");

	result = find_device_by_type(BUS_TYPE_STOR, DEV_TYPE_STOR_DISK, 0, &bus_id, &dev_id, NULL);
	if (result)
		fatal("failed to find STOR_DISK device");

	result = lv1_open_device(bus_id, dev_id, 0);
	if (result)
		fatal("failed to open disk device");

	result = map_dma_mem(bus_id, dev_id, buffer, sizeof(buffer), &bus_addr);
	if (result)
		fatal("failed to map DMA for disk device");

	printf("HDD inited\n");
	initialized = 1;

	return STA_PROTECT;
}

void disk_shutdown(BYTE id)
{
	if (id != 0)
		return;

	if (!initialized)
		return;

	initialized = 0;
	printf("Closing HDD...\n");
	if (unmap_dma_mem(bus_id, dev_id, bus_addr, sizeof(buffer)))
		fatal("failed to unmap DMA for disk");
	if (lv1_close_device(bus_id, dev_id))
		fatal("failed to close disk device");

	printf("HDD closed\n");
}

DSTATUS disk_status (BYTE id)
{
	if (id != 0)
		return STA_NODISK;
	if (!initialized)
		return STA_NOINIT;
	return STA_PROTECT;
}

DRESULT disk_ioctl(BYTE id, BYTE cmd, void *data)
{
	if (id != 0)
		return RES_PARERR;

	u32 *p = data;
	
	switch(cmd) {
		case CTRL_SYNC:
			return RES_OK;
		case GET_SECTOR_COUNT:
			return RES_WRPRT;
		case GET_SECTOR_SIZE:
			*p = 512;
			return RES_OK;
		case GET_BLOCK_SIZE:
			*p = 512;
			return RES_OK;
		default:
			return RES_PARERR;
	}
}

DRESULT disk_read (BYTE id, BYTE *buf, DWORD sector, u32 count)
{
	s64 result;
	u64 tag, etag;
	u64 status;

	while (count > BUFFER_SECTORS) {
		DRESULT r = disk_read(id, buf, sector, BUFFER_SECTORS);
		if (r != RES_OK)
			return r;
		count -= BUFFER_SECTORS;
		sector += BUFFER_SECTORS;
		buf += BUFFER_SECTORS * 512;
	}

	//printf("lv1_storage_read(sector=%d, count=%d)\n", sector, count);
	result = lv1_storage_read(dev_id, REGION, sector, count, FLAGS, (u64)buffer, &tag);
	if (result) {
		printf("lv1_storage_read(sector=%d, count=%d) failed: %ld\n", sector, count, result);
		return RES_ERROR;
	}

	while(1) {
		result = lv1_storage_get_async_status(dev_id, &etag, &status);
		if (result == -6)
			continue;
		if (tag == etag)
			break;
	}

	if (status) {
		printf("lv1_storage_read(sector=%d, count=%d) async returned error %ld\n", sector, count, status);
		return RES_ERROR;
	}

	memcpy(buf, buffer, count * 512);
	return RES_OK;
}

WCHAR ff_convert (WCHAR in, UINT dir)
{
	return in;
}