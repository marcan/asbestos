/*  netrpc.c - server to perform lv1 and device ops via network requests

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

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
#include "network.h"

#ifdef NETRPC_ENABLE

extern volatile u64 _thread1_release;
extern volatile u64 _thread1_vector;

enum {
	RPC_PING = 0,

	RPC_READMEM,
	RPC_WRITEMEM,
	RPC_HVCALL,
	RPC_ADDMMIO,
	RPC_DELMMIO,
	RPC_CLRMMIO,
	RPC_MEMSET,
	RPC_VECTOR,
	RPC_SYNC,
	RPC_CALL,
};

struct rpc_vec {
	void *vec0;
	void *vec1;
	void *copy_dst;
	void *copy_src;
	u32 copy_size;
};

typedef s64 (*rpc_callfn) (u64,u64,u64,u64,u64,u64,u64,u64);

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
		struct rpc_vec vector;
		struct {
			void *addr;
			u32 size;
		} sync;
		struct {
			rpc_callfn addr;
			u64 args[8];
		} call;
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

void memset64(u64 *dst, u64 val, s32 size)
{
	while (size > 0) {
		*dst++ = val;
		size -= 8;
	}
}

void memset32(u32 *dst, u32 val, s32 size)
{
	while (size > 0) {
		*dst++ = val;
		size -= 4;
	}
}

void memset16(u16 *dst, u16 val, s32 size)
{
	while (size > 0) {
		*dst++ = val;
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

void memset_align(void *dst, u64 val, u32 size)
{
	u64 t = ((u64)dst)|size;

	if ((t&7) == 0) {
		memset64(dst, val, size);
	} else if ((t&3) == 0) {
		memset32(dst, val, size);
	} else if ((t&1) == 0) {
		memset16(dst, val, size);
	} else {
		memset(dst, val, size);
	}
}

static u8 tmpbuf[XFERSIZE] __attribute__((aligned(64)));

static u64 *vector[3] = {NULL,NULL,NULL};

static void netrpc_vector(void *vec0, void *vec1, void *copy_dst, void *copy_src, u32 copy_size)
{
	netrpc_shutdown();
	net_shutdown();
	mm_shutdown_highmem();
	mm_shutdown();
	if (copy_size) {
		printf("netrpc: Relocating vectors...\n");
		memcpy(copy_dst, copy_src, copy_size);
		sync_before_exec(copy_dst, copy_size);
	}
	printf("netrpc: Letting thread1 run loose...\n");
	_thread1_vector = (u64)vec1;
	_thread1_release = 1;
	vector[0] = vec0;
	printf("netrpc: Taking the plunge...\n");
	debug_shutdown();
	((void (*)(void))vector)();
	lv1_panic(0);
}

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
	u64 val;

	struct rpc_vec vector;

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
		case RPC_MEMSET:
			memcpy(&val, hdr->memop.data, sizeof(u64));
			memset_align(hdr->memop.addr, val, hdr->memop.size);
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
		case RPC_CLRMMIO:
			hdr->reply.retcode = mm_clrmmio();
			sendbuf(REPLY_SIZE, addr, port);
			break;
		case RPC_VECTOR:
			vector = hdr->vector;
			printf("netrpc: vector called (%p,%p %p,%p,0x%x)...\n", vector.vec0, vector.vec1, vector.copy_dst, vector.copy_src, vector.copy_size);
			hdr->reply.retcode = 0;
			sendbuf(REPLY_SIZE, addr, port);
			netrpc_vector(vector.vec0, vector.vec1, vector.copy_dst, vector.copy_src, vector.copy_size);
			break;
		case RPC_SYNC:
			sync_before_exec(hdr->sync.addr, hdr->sync.size);
			hdr->reply.retcode = 0;
			sendbuf(REPLY_SIZE, addr, port);
			break;
		case RPC_CALL:
			hdr->reply.retcode = hdr->call.addr(hdr->call.args[0], hdr->call.args[1],
												hdr->call.args[2], hdr->call.args[3],
												hdr->call.args[4], hdr->call.args[5],
												hdr->call.args[6], hdr->call.args[7]);
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
	if (!outbuf)
		fatal("netrpc: Could not allocate buffer");

	udp_bind(pcb, IP_ADDR_ANY, PORT);
	udp_recv(pcb, netrpc_recv, NULL);
	printf("netrpc: initialized\n");
}
void netrpc_shutdown(void)
{
	printf("netrpc: shutting down\n");
	if (pcb)
		udp_remove(pcb);
	pcb = NULL;
	if (outbuf)
		pbuf_free(outbuf);
	outbuf = NULL;
	printf("netrpc: shutdown complete\n");
}

#endif