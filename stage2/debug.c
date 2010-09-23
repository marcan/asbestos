/*  debug.c - printf message logging via Lv-1 Ethernet

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "debug.h"
#include "lv1call.h"
#include "gelic.h"
#include "string.h"
#include "printf.h"

volatile struct gelic_descr *dbg_descr = (void*)0x40000;
void panic(void);

static u64 bus_id = 1;
static u64 dev_id = 0;

static u64 bus_addr;

#define DEBUG_PORT 18194

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
	char data[];
} __attribute__((packed));

#define MAX_MESSAGE_SIZE 1000

struct debug_packet *dbg_pkt = (void*)0x40100;

void debug_init(void)
{
	s64 result;
	u64 v2;

	u64 rgn_addr = (u64)dbg_descr;
	u64 flags = 0xf800000000000000UL;

	result = lv1_allocate_device_dma_region(bus_id, dev_id, 0x10000, 12, 0, &bus_addr);
	if (result)
		lv1_panic(0);

	result = lv1_map_device_dma_region(bus_id, dev_id, rgn_addr, bus_addr, 0x10000, flags);
	if (result)
		lv1_panic(0);

	memset((void*)dbg_descr, 0, sizeof(*dbg_descr));
	dbg_descr->buf_addr = bus_addr + 0x100;

	u64 mac;
	result = lv1_net_control(bus_id, dev_id, 1, 0, 0, 0, &mac, &v2);
	if (result)
		lv1_panic(0);

	//printf("MAC: %012lx\n", mac);
	mac <<= 16;

	u64 vlan_id;
	result = lv1_net_control(bus_id, dev_id, 4, 2, 0, 0, &vlan_id, &v2);
	if (result)
		lv1_panic(0);

	memset(dbg_pkt, 0, sizeof(*dbg_pkt));

	memset(&dbg_pkt->eth.dest, 0xff, 6);
	memcpy(&dbg_pkt->eth.src, &mac, 6);
	dbg_pkt->eth.type = 0x8100;
	dbg_pkt->vlan.vlan = vlan_id;
	dbg_pkt->vlan.subtype = 0x0800;
	dbg_pkt->ip.ver_len = 0x45;
	dbg_pkt->ip.ttl = 10;
	dbg_pkt->ip.proto = 0x11;
	dbg_pkt->ip.src = 0x00000000;
	dbg_pkt->ip.dest = 0xffffffff;
	dbg_pkt->udp.src = DEBUG_PORT;
	dbg_pkt->udp.dest = DEBUG_PORT;
}

int printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	size_t msgsize = vsnprintf(dbg_pkt->data, MAX_MESSAGE_SIZE, fmt, ap);
	va_end(ap);

	dbg_descr->buf_size = sizeof(*dbg_pkt) + msgsize;
	dbg_pkt->ip.total_length = msgsize + sizeof(struct udphdr) + sizeof(struct iphdr);
	dbg_pkt->udp.len = msgsize + sizeof(struct udphdr);

	dbg_pkt->ip.checksum = 0;
	u32 sum = 0;
	u16 *p = (u16*)&dbg_pkt->ip;
	int i;
	for (i=0; i<5; i++)
		sum += *p++;
	dbg_pkt->ip.checksum = ~(sum + (sum>>16));

	dbg_descr->dmac_cmd_status = GELIC_DESCR_DMA_CMD_NO_CHKSUM | GELIC_DESCR_TX_DMA_FRAME_TAIL;
	dbg_descr->result_size = 0;
	dbg_descr->data_status = 0;

	lv1_net_start_tx_dma(bus_id, dev_id, bus_addr, 0);

	while ((dbg_descr->dmac_cmd_status & GELIC_DESCR_DMA_STAT_MASK) == GELIC_DESCR_DMA_CARDOWNED);
	return 0;
}
