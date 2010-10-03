/*  netrpc.c - server to perform lv1 and device ops via network requests

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stddef.h>

#include "config.h"
#include "netrpc.h"

#include "types.h"
#include "lwip/udp.h"
#include "debug.h"
#include "lv1call.h"
#include "mm.h"

#ifdef NETRPC_ENABLE

enum {
	RPC_PING = 0,

	RPC_READMEM,
	RPC_WRITEMEM,
	RPC_HVCALL,
	RPC_ADDMMIO,
	RPC_DELMMIO
};

struct rpc_header {
	u32 cmd;
	u32 tag;
	union {
		struct {
			void *addr;
			u32 size;
			u8 data[];
		} memop;
		struct {
			u32 numin, numout;
			u64 code;
			u64 regs[];
		} hvcall;
		struct {
			u64 start;
			u32 size;
		} addmmio;
		struct {
			u64 start;
		} delmmio;
		struct {
			s64 retcode;
			u8 retdata[];
		} reply;
	};
} __attribute__((packed));

static struct udp_pcb *pcb;

#define BUFSIZE 1500
#define XFERSIZE 1024
#define PORT 1337

static struct pbuf *outbuf;

#define REPLY_SIZE 16

static void sendbuf(int len, struct ip_addr *addr, u16_t port)
{
	outbuf->tot_len = len;
	outbuf->len = len;

	err_t err = udp_sendto(pcb, outbuf, addr, port);
	// reclaim the header space used by udp_send
	pbuf_header(outbuf, -(outbuf->len - len));
	outbuf->tot_len = BUFSIZE;
	outbuf->len = BUFSIZE;
	if (err)
		printf("netrpc: udp_sendto() returned %d\n", err);
}

void memcpy64(u64 *dst, u64 *src, s32 size)
{
	while (size > 0) {
		*dst++ = *src++;
		size -= 8;
	}
}

void memcpy32(u32 *dst, u32 *src, s32 size)
{
	while (size > 0) {
		*dst++ = *src++;
		size -= 4;
	}
}

void memcpy16(u16 *dst, u16 *src, s32 size)
{
	while (size > 0) {
		*dst++ = *src++;
		size -= 2;
	}
}

void memcpy_align(void *dst, void *src, u32 size)
{
	u64 t = ((u64)dst)|((u64)src)|size;

	if ((t&7) == 0) {
		memcpy64(dst, src, size);
	} else if ((t&3) == 0) {
		memcpy32(dst, src, size);
	} else if ((t&1) == 0) {
		memcpy16(dst, src, size);
	} else {
		memcpy(dst, src, size);
	}
}

static u8 tmpbuf[XFERSIZE] __attribute__((aligned(64)));

static void netrpc_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, struct ip_addr *addr, u16_t port)
{
	if (p->tot_len > BUFSIZE) {
		printf("netrpc: max buffer size exceeded (%d)\n", p->tot_len);
		return;
	}

	u8 *buf = outbuf->payload;
	pbuf_copy_partial(p, buf, BUFSIZE, 0);

	struct rpc_header *hdr = (void*)buf;

	u64 regs[9];
	u32 numout;
	u32 size;
	
	switch (hdr->cmd) {
		case RPC_PING:
			hdr->reply.retcode = 0;
			sendbuf(REPLY_SIZE, addr, port);
			break;
		case RPC_READMEM:
			if (hdr->memop.size > XFERSIZE) {
				printf("netrpc: READMEM size > %d (%d)\n", XFERSIZE, hdr->memop.size);
				hdr->reply.retcode = -1;
				sendbuf(REPLY_SIZE, addr, port);
				break;
			}
			size = hdr->memop.size;
			memcpy_align(tmpbuf, hdr->memop.addr, size);
			memcpy(hdr->reply.retdata, tmpbuf, size);
			hdr->reply.retcode = 0;
			sendbuf(REPLY_SIZE + size, addr, port);
			break;
		case RPC_WRITEMEM:
			if (hdr->memop.size > XFERSIZE) {
				printf("netrpc: WRITEMEM size > %d (%d)\n", XFERSIZE, hdr->memop.size);
				hdr->reply.retcode = -1;
				sendbuf(REPLY_SIZE, addr, port);
				break;
			}
			memcpy(tmpbuf, hdr->memop.data, hdr->memop.size);
			memcpy_align(hdr->memop.addr, tmpbuf, hdr->memop.size);
			hdr->reply.retcode = 0;
			sendbuf(REPLY_SIZE, addr, port);
			break;
		case RPC_HVCALL:
			if (hdr->hvcall.numout > 7) {
				printf("netrpc: HVCALL numout > 7 (%d)\n", hdr->hvcall.numout);
				hdr->reply.retcode = -100;
				sendbuf(REPLY_SIZE, addr, port);
				break;
			}
			if (hdr->hvcall.numin > 8) {
				printf("netrpc: HVCALL numin > 8 (%d)\n", hdr->hvcall.numin);
				hdr->reply.retcode = -100;
				sendbuf(REPLY_SIZE, addr, port);
				break;
			}
			numout = hdr->hvcall.numout;
			memcpy(regs, hdr->hvcall.regs, hdr->hvcall.numin*8);
			regs[8] = hdr->hvcall.code;
			hvcallv(regs);
			hdr->reply.retcode = regs[0];
			memcpy(hdr->reply.retdata, &regs[1], numout*8);
			sendbuf(REPLY_SIZE+numout*8, addr, port);
			break;
		case RPC_ADDMMIO:
			hdr->reply.retcode = mm_addmmio(hdr->addmmio.start, hdr->addmmio.size);
			sendbuf(REPLY_SIZE, addr, port);
			break;
		case RPC_DELMMIO:
			hdr->reply.retcode = mm_delmmio(hdr->addmmio.start);
			sendbuf(REPLY_SIZE, addr, port);
			break;
		default:
			printf("netrpc: Unknown RPC command 0x%x\n", hdr->cmd);
			hdr->reply.retcode = -1;
			sendbuf(REPLY_SIZE, addr, port);
			break;
	}
	pbuf_free(p);
}

void netrpc_init(void)
{
	pcb = udp_new();
	if (!pcb)
		fatal("netrpc: Could not allocate PCB for netrpc");
	outbuf = pbuf_alloc(PBUF_TRANSPORT, BUFSIZE, PBUF_RAM);

	udp_bind(pcb, IP_ADDR_ANY, PORT);
	udp_recv(pcb, netrpc_recv, NULL);
	printf("netrpc: initialized\n");
}
void netrpc_shutdown(void)
{
	udp_remove(pcb);
	outbuf = NULL;
	pbuf_free(outbuf);
	pcb = NULL;
	printf("netrpc: shut down\n");
}

#endif