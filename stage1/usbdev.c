/*  usbdev.c - USB device driver implementing stage2 loading

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "lv2.h"

int ep_pipe;

enum {
	REQ_PRINT = 1,
	REQ_GET_STAGE2_SIZE,
	REQ_READ_STAGE2_BLOCK
};

#define TYPE_HOST2DEV 0x40
#define TYPE_DEV2HOST 0xc0

volatile int cv;
volatile int u_result, u_count;

CBTHUNK(void, usbcb, (int result, int count, void *arg))
{
	u_result = result;
	u_count = count;
	cv=1;
}

// printf hook
CBTHUNK(int, write, (char *data, int len, void *blah))
{
	control_transfer_t req = { TYPE_HOST2DEV, REQ_PRINT, 0, 0, len };
	cv = 0;
	usbControlTransfer(ep_pipe, &req, data, usbcb, NULL);
	while (!cv);
	return len;
}

CBTHUNK(int, device_probe, (int device_id))
{
	u8 *desc;
	desc = usbGetDescriptor(device_id, NULL, 1);
	if (!desc)
		return -1;
	if ((*(u32*)&desc[8] == 0xAAAA1337))
		return 0;
	else
		return -1;
}

void *write_save;

extern u8 __stage2[];

static void flushblock(void *addr)
{
	asm("dcbst 0, %0; sync; icbi 0, %0" :: "r"(addr));
}

static void ipatch(void *addr, u32 instr)
{
	*(u32*)addr = instr;
	flushblock(addr);
}

extern u8 __thread_catch[];
extern u8 __thread_catch_end[];

static void *memcpy(void *dst, const void *src, int len)
{
	int i;

	for (i = 0; i < len; i++)
		((unsigned char *)dst)[i] = ((unsigned char *)src)[i];
	return dst;
}

void disirq(void);

CBTHUNK(int, device_attach, (int device_id))
{
	ep_pipe = usbOpenEndpoint(device_id, NULL);
	if (ep_pipe < 0)
		return -1;

	// save old printf handler
	write_save = *(void**)0x800000000033ffb0;

    // get rid of the mutex stuff in printf that breaks the USB stack
	ipatch((void*)0x800000000028a694, 0x60000000);
	ipatch((void*)0x800000000028a6c8, 0x60000000);

	// hook printf
	*(void**)0x800000000033ffb0 = write;

	printf("Hello, world from Lv-2 (stage1)\n");

	u32 tid;
	asm("mfspr %0, 0x88" : "=r"(tid));
	printf("Current thread ID: %d\n", 2-(tid>>30));

	control_transfer_t req = { TYPE_DEV2HOST, REQ_GET_STAGE2_SIZE, 0, 0, 4 };
	volatile u32 stage2_size = 0;
	u32 offset = 0;
	u8 *p = __stage2;

	cv = 0;
	usbControlTransfer(ep_pipe, &req, (u32*)&stage2_size, usbcb, NULL);
	while(!cv);
	if (u_result != 0 || u_count != 4) {
		printf("USB request failed: result %d, count: 0x%x\n", u_result, u_count);
		panic();
	}

	printf("Loading stage2 (0x%x bytes)...\n", stage2_size);

	req.bRequest = REQ_READ_STAGE2_BLOCK;

	while (offset < stage2_size) {
		int blocksize = 0x1000;
		if (blocksize > (stage2_size - offset))
			blocksize = stage2_size - offset;

		req.wIndex = offset >> 12;
		req.wLength = blocksize;

		cv = 0;
		usbControlTransfer(ep_pipe, &req, p, usbcb, NULL);
		while(!cv);
		if (u_result != 0 || u_count != blocksize) {
			printf("USB request failed: result %d, count: 0x%x\n", u_result, u_count);
			panic();
		}

		p += blocksize;
		offset += blocksize;
	}

	p = __stage2;
	offset = 0;
	while (offset < stage2_size) {
		flushblock(p);
		offset += 32;
		p += 32;
	}

	printf("Stage2 loaded\n");

	asm("mfspr %0, 0x88" : "=r"(tid));
	printf("Current thread ID: %d\n", 2-(tid>>30));

	printf("Catching the other thread and taking the plunge...\n");
	disirq();

	u8 *decr_vector = (void*)0x8000000000000900;
	memcpy(decr_vector, __thread_catch, __thread_catch_end - __thread_catch);
	flushblock(decr_vector);

	int i;
	for (i=0x100; i<0x1800; i+=0x80) {
		if (i == 0x900)
			continue;
		u32 *vector = (void*)(0x8000000000000000+i);
		*vector = 0x48000902; // ba 0x900
		flushblock(vector);
	}

	asm ("bl __stage2; b panic");
	return 0;
}

CBTHUNK(int, device_remove, (int device_id))
{
	*(void**)0x800000000033ffb0 = write_save;
	// this should never be called anyway, since we kill lv2
	panic();
	return 0;
}

const usb_driver_t usb_driver = {
	"AsbestOS",
	device_probe,
	device_attach,
	device_remove
};

