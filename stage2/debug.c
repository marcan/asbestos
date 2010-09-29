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

static int bus_id;
static int dev_id;

static u64 bus_addr;

struct ethhdr {
	u8 dest[6];
	u8 src[6];
	u16 type;
} __attribute__((packed));

struct vlantag {
	u16 vlan;
	u16 subtype;
} __attribute__((packed));

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
} __attribute__((packed));

struct udphdr {
	u16 src;
	u16 dest;
	u16 len;
	u16 checksum;
} __attribute__((packed));

static struct ethhdr *h_eth;
static struct vlantag *h_vlan;
static struct iphdr *h_ip;
static struct udphdr *h_udp;

static char *pmsg;

#define MAX_MESSAGE_SIZE 1000

struct debug_block {
	volatile struct gelic_descr descr;
	u8 pkt[1520];
} __attribute__((packed));

static struct debug_block dbg ALIGNED(32);

static int debug_initialized = 0;

static int header_size;

void debug_init(void)
{
	s64 result;
	u64 v2;

	result = find_device_by_type(DEV_TYPE_ETH, 0, &bus_id, &dev_id, NULL);
	if (result)
		lv1_panic(0);

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

	h_eth = (struct ethhdr*)dbg.pkt;
	
	memset(&h_eth->dest, 0xff, 6);
	memcpy(&h_eth->src, &mac, 6);

	header_size = sizeof(struct ethhdr);

	u64 vlan_id;
	result = lv1_net_control(bus_id, dev_id, GELIC_LV1_GET_VLAN_ID, \
							 GELIC_LV1_VLAN_TX_ETHERNET_0, 0, 0, &vlan_id, &v2);
	if (result == 0) {
		h_eth->type = 0x8100;

		header_size += sizeof(struct vlantag);
		h_vlan = (struct vlantag*)(h_eth+1);
		h_vlan->vlan = vlan_id;
		h_vlan->subtype = 0x0800;
		h_ip = (struct iphdr*)(h_vlan+1);
	} else {
		h_eth->type = 0x0800;
		h_ip = (struct iphdr*)(h_eth+1);
	}

	header_size += sizeof(struct iphdr);
	h_ip->ver_len = 0x45;
	h_ip->ttl = 10;
	h_ip->proto = 0x11;
	h_ip->src = 0x00000000;
	h_ip->dest = 0xffffffff;

	header_size += sizeof(struct udphdr);
	h_udp = (struct udphdr*)(h_ip+1);
	h_udp->src = DEBUG_PORT;
	h_udp->dest = DEBUG_PORT;

	pmsg = (char*)(h_udp+1);

	debug_initialized = 1;
}

int printf(const char *fmt, ...)
{
	va_list ap;

	if (!debug_initialized)
		return 0;

	va_start(ap, fmt);
	size_t msgsize = vsnprintf(pmsg, MAX_MESSAGE_SIZE, fmt, ap);
	va_end(ap);

	dbg.descr.buf_size = header_size + msgsize;
	h_ip->total_length = msgsize + sizeof(struct udphdr) + sizeof(struct iphdr);
	h_udp->len = msgsize + sizeof(struct udphdr);

	h_ip->checksum = 0;
	u32 sum = 0;
	u16 *p = (u16*)h_ip;
	int i;
	for (i=0; i<5; i++)
		sum += *p++;
	h_ip->checksum = ~(sum + (sum>>16));

	dbg.descr.dmac_cmd_status = GELIC_DESCR_DMA_CMD_NO_CHKSUM | GELIC_DESCR_TX_DMA_FRAME_TAIL;
	dbg.descr.result_size = 0;
	dbg.descr.data_status = 0;

	lv1_net_start_tx_dma(bus_id, dev_id, bus_addr, 0);

	while ((dbg.descr.dmac_cmd_status & GELIC_DESCR_DMA_STAT_MASK) == GELIC_DESCR_DMA_CARDOWNED);
	return msgsize;
}

void abort(void)
{
	printf("abort() called! Panicking.\n");
	lv1_panic(0);
}

void fatal(const char *msg)
{
	printf("FATAL: %s\n", msg);
	printf("Panicking.\n");
	lv1_panic(0);
}
