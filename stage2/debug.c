/*  debug.c - printf message logging via Lv-1 Ethernet

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stddef.h>
#include "types.h"
#include "debug.h"
#include "lv1call.h"
#include "gelic.h"
#include "string.h"
#include "printf.h"
#include "device.h"

#define DEBUG_PORT 18194

static u64 bus_id = 1;
static u64 dev_id = 0;

static u64 bus_addr;

struct ethhdr {
	u8 dest[6];
	u8 src[6];
	u16 type;
};

struct vlantag {
	u16 vlan;
	u16 subtype;
};

struct iphdr {
	u8 ver_len;
	u8 dscp_ecn;
	u16 total_length;
	u16 ident;
	u16 frag_off_flags;
	u8 ttl;
	u8 proto;
	u16 checksum;
	u32 src;
	u32 dest;
};

struct udphdr {
	u16 src;
	u16 dest;
	u16 len;
	u16 checksum;
};

struct debug_packet {
	struct ethhdr eth;
	struct vlantag vlan;
	struct iphdr ip;
	struct udphdr udp;
} __attribute__((packed));

#define MAX_MESSAGE_SIZE 1000

struct debug_block {
	volatile struct gelic_descr descr;
	struct debug_packet pkt;
	char message[MAX_MESSAGE_SIZE];
} __attribute__((packed));

static struct debug_block dbg ALIGNED(32);

void debug_init(void)
{
	s64 result;
	u64 v2;

	result = map_dma_mem(bus_id, dev_id, &dbg, sizeof(dbg), &bus_addr);
	if (result)
		lv1_panic(0);

	memset(&dbg, 0, sizeof(dbg));

	dbg.descr.buf_addr = bus_addr + offsetof(struct debug_block, pkt);

	u64 mac;
	result = lv1_net_control(bus_id, dev_id, GELIC_LV1_GET_MAC_ADDRESS, 0, 0, 0, &mac, &v2);
	if (result)
		lv1_panic(0);

	mac <<= 16;

	u64 vlan_id;
	result = lv1_net_control(bus_id, dev_id, GELIC_LV1_GET_VLAN_ID, \
							 GELIC_LV1_VLAN_TX_ETHERNET_0, 0, 0, &vlan_id, &v2);
	if (result)
		lv1_panic(0);

	memset(&dbg.pkt.eth.dest, 0xff, 6);
	memcpy(&dbg.pkt.eth.src, &mac, 6);
	dbg.pkt.eth.type = 0x8100;
	dbg.pkt.vlan.vlan = vlan_id;
	dbg.pkt.vlan.subtype = 0x0800;
	dbg.pkt.ip.ver_len = 0x45;
	dbg.pkt.ip.ttl = 10;
	dbg.pkt.ip.proto = 0x11;
	dbg.pkt.ip.src = 0x00000000;
	dbg.pkt.ip.dest = 0xffffffff;
	dbg.pkt.udp.src = DEBUG_PORT;
	dbg.pkt.udp.dest = DEBUG_PORT;
}

int printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	size_t msgsize = vsnprintf(dbg.message, MAX_MESSAGE_SIZE, fmt, ap);
	va_end(ap);

	dbg.descr.buf_size = sizeof(dbg.pkt) + msgsize;
	dbg.pkt.ip.total_length = msgsize + sizeof(struct udphdr) + sizeof(struct iphdr);
	dbg.pkt.udp.len = msgsize + sizeof(struct udphdr);

	dbg.pkt.ip.checksum = 0;
	u32 sum = 0;
	u16 *p = (u16*)&dbg.pkt.ip;
	int i;
	for (i=0; i<5; i++)
		sum += *p++;
	dbg.pkt.ip.checksum = ~(sum + (sum>>16));

	dbg.descr.dmac_cmd_status = GELIC_DESCR_DMA_CMD_NO_CHKSUM | GELIC_DESCR_TX_DMA_FRAME_TAIL;
	dbg.descr.result_size = 0;
	dbg.descr.data_status = 0;

	lv1_net_start_tx_dma(bus_id, dev_id, bus_addr, 0);

	while ((dbg.descr.dmac_cmd_status & GELIC_DESCR_DMA_STAT_MASK) == GELIC_DESCR_DMA_CARDOWNED);
	return 0;
}

void abort(void)
{
	printf("abort() called! Panicking.\n");
	lv1_panic(0);
}
