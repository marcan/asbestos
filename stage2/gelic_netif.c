/*  gelic_netif.c - lwIP device driver for the Gelic PS3 Ethernet device

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

Based on ethernetif.c from lwIP:
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
*/

#include <stddef.h>
#include "string.h"
#include "debug.h"
#include "device.h"
#include "malloc.h"

#include "gelic.h"
#include "lv1call.h"

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define NUM_RX_BUFS 16

struct gelicif_buf {
	volatile struct gelic_descr *descr;
	u64 bus_addr;
	u8 *pkt;
};

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 */
struct gelicif {
	/* Add whatever per-interface state that is needed here. */
	int bus_id;
	int dev_id;
	u64 bus_addr;
	void *dma_buf;
	u64 buf_size;
	int need_vlan;
	u16 vlan_id;
	int last_rx;
	int next_rx;
	struct gelicif_buf txd;
	struct gelicif_buf rxd[NUM_RX_BUFS];

};

static void chain_rx(struct gelicif *gelicif, int i)
{
	//printf("gelicif: chain_rx(%d)\n", i);
	gelicif->rxd[i].descr->next_descr_addr = 0;
	gelicif->rxd[i].descr->result_size = 0;
	gelicif->rxd[i].descr->result_size = 0;
	gelicif->rxd[i].descr->valid_size = 0;
	gelicif->rxd[i].descr->data_error = 0;
	gelicif->rxd[i].descr->dmac_cmd_status = GELIC_DESCR_DMA_CARDOWNED;
	if (gelicif->last_rx == -1) {
		gelicif->last_rx = i;
	} else {
		if (gelicif->rxd[gelicif->last_rx].descr->next_descr_addr)
			fatal("gelicif: last RX packet was already chained");
		gelicif->rxd[gelicif->last_rx].descr->next_descr_addr = gelicif->rxd[i].bus_addr;
		gelicif->last_rx++;
		gelicif->last_rx %= NUM_RX_BUFS;
	}
}

/**
 * In this function, the hardware should be initialized.
 * Called from gelicif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this gelicif
 */
static void
low_level_init(struct netif *netif)
{
	struct gelicif *gelicif = netif->state;
	s64 result;
	u64 v2;
	int i;

	printf("gelicif: low_level_init()\n");

	/* maximum transfer unit */
	netif->mtu = 1500;

	u64 pktbuf_size = netif->mtu + 18;
	pktbuf_size = (pktbuf_size+127)&(~127);

	result = find_device_by_type(DEV_TYPE_ETH, 0, &gelicif->bus_id, &gelicif->dev_id, NULL);
	if (result)
		fatal("gelicif: failed to find device");

	printf("gelicif: device is on bus %d, device %d\n", gelicif->bus_id, gelicif->dev_id);

	gelicif->bus_id = 1;
	gelicif->dev_id = 0;
	gelicif->buf_size = (NUM_RX_BUFS + 1) * (sizeof(struct gelic_descr) + pktbuf_size);
	gelicif->dma_buf = memalign(32, gelicif->buf_size);
	if (!gelicif->dma_buf)
		fatal("gelicif: DMA buffer alloc failed");

	printf("gelicif: allocated 0x%lx bytes for DMA buffer\n", gelicif->buf_size);

	memset(gelicif->dma_buf, 0, gelicif->buf_size);

	/* set MAC hardware address length */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* set MAC hardware address */
	u64 mac;
	result = lv1_net_control(gelicif->bus_id, gelicif->dev_id, \
							 GELIC_LV1_GET_MAC_ADDRESS, 0, 0, 0, &mac, &v2);
	if (result)
		fatal("gelicif: lv1_net_control(GELIC_LV1_GET_MAC_ADDRESS) failed");

	printf("gelicif: ethernet MAC is %012lx\n", mac);
	mac <<= 16;
	memcpy(netif->hwaddr, &mac, 6);

	u64 vlan_id;
	result = lv1_net_control(gelicif->bus_id, gelicif->dev_id, GELIC_LV1_GET_VLAN_ID, \
							 GELIC_LV1_VLAN_TX_ETHERNET_0, 0, 0, &vlan_id, &v2);
	if (result == 0) {
		gelicif->need_vlan = 1;
		gelicif->vlan_id = vlan_id;
		printf("gelicif: VLAN ID is 0x%04x\n", gelicif->vlan_id);
	} else {
		gelicif->need_vlan = 0;
		printf("gelicif: no VLAN in use\n");
	}

	result = map_dma_mem(gelicif->bus_id, gelicif->dev_id, gelicif->dma_buf, \
						 gelicif->buf_size, &gelicif->bus_addr);
	if (result)
		fatal("gelicif: map_dma_mem failed");

	printf("gelicif: base bus address is 0x%lx\n", gelicif->bus_addr);

	u64 bus_addr = gelicif->bus_addr;
	u8 *mem_addr = gelicif->dma_buf;

	gelicif->txd.descr = (void*)mem_addr;
	gelicif->txd.bus_addr = bus_addr;
	mem_addr += sizeof(struct gelic_descr);
	bus_addr += sizeof(struct gelic_descr);
	for (i=0; i<NUM_RX_BUFS; i++) {
		gelicif->rxd[i].descr = (void*)mem_addr;
		gelicif->rxd[i].bus_addr = bus_addr;
		mem_addr += sizeof(struct gelic_descr);
		bus_addr += sizeof(struct gelic_descr);
	}
	gelicif->txd.descr->buf_addr = bus_addr;
	gelicif->txd.pkt = mem_addr;
	bus_addr += pktbuf_size;
	mem_addr += pktbuf_size;

	gelicif->last_rx = -1;
	gelicif->next_rx = 0;
	for (i=0; i<NUM_RX_BUFS; i++) {
		gelicif->rxd[i].descr->buf_addr = bus_addr;
		gelicif->rxd[i].descr->buf_size = pktbuf_size;
		gelicif->rxd[i].pkt = mem_addr;
		bus_addr += pktbuf_size;
		mem_addr += pktbuf_size;
		chain_rx(gelicif, i);
	}

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

	/* clear any existing RX DMA */
	result = lv1_net_stop_rx_dma(gelicif->bus_id, gelicif->dev_id);
	if (result == 0)
		printf("gelicif: cleared old RX DMA job\n");

	/* start the RX DMA */
	result = lv1_net_start_rx_dma(gelicif->bus_id, gelicif->dev_id, gelicif->rxd[0].bus_addr, 0);
	if (result)
		fatal("gelicif: lv1_net_start_rx_dma failed");
	printf("gelicif: started RX DMA\n");
}

static void low_level_shutdown(struct netif *netif)
{
	struct gelicif *gelicif = netif->state;
	s32 result;

	printf("gelicif: low_level_shutdown()\n");

	result = lv1_net_stop_rx_dma(gelicif->bus_id, gelicif->dev_id);
	if (result) {
		printf("gelicif: WARNING: failed to stop RX DMA, will not free buffer\n");
	} else {
		printf("gelicif: stopped RX DMA\n");
		result = unmap_dma_mem(gelicif->bus_id, gelicif->dev_id, gelicif->bus_addr, gelicif->buf_size);
		if (result) {
			printf("gelicif: WARNING: failed to unmap DMA mem, will not free buffer\n");
		} else {
			printf("gelicif: unmapped DMA memory\n");
			free(gelicif->dma_buf);
		}
	}

	printf("gelicif: low_level_shutdown() complete\n");
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this gelicif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
	struct gelicif *gelicif = netif->state;
	s64 result;

	//printf("gelicif: transmitting packet\n");

	u8 *pkt = gelicif->txd.pkt;
	u64 total_len = 0;

#if ETH_PAD_SIZE
	pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

	if (gelicif->need_vlan) {
		total_len = pbuf_copy_partial(p, pkt, 12, 0);
		/* Insert the VLAN tag */
		pkt[12] = 0x81;
		pkt[13] = 0x00;
		pkt[14] = gelicif->vlan_id >> 8;
		pkt[15] = gelicif->vlan_id;
		total_len += 4;
		total_len += pbuf_copy_partial(p, &pkt[16], netif->mtu + 2, 12);
	} else {
		total_len = pbuf_copy_partial(p, pkt, netif->mtu + 14, 0);
	}

	/* build the descriptor */
	gelicif->txd.descr->buf_size = total_len;
	gelicif->txd.descr->dmac_cmd_status = GELIC_DESCR_DMA_CMD_NO_CHKSUM | GELIC_DESCR_TX_DMA_FRAME_TAIL;
	gelicif->txd.descr->result_size = 0;
	gelicif->txd.descr->data_status = 0;

	/* send it and block until done */
	result = lv1_net_start_tx_dma(gelicif->bus_id, gelicif->dev_id, gelicif->txd.bus_addr, 0);
	if (result)
		fatal("gelicif: lv1_net_start_tx_dma failed");
	while ((gelicif->txd.descr->dmac_cmd_status & GELIC_DESCR_DMA_STAT_MASK) == GELIC_DESCR_DMA_CARDOWNED);

#if ETH_PAD_SIZE
	pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

	LINK_STATS_INC(link.xmit);

	return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this gelicif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif)
{
	struct gelicif *gelicif = netif->state;
	struct pbuf *p = NULL;
	u16_t len, alloc_len;
	s64 result;
	int idx;

	idx = gelicif->next_rx;
	u32 status = gelicif->rxd[idx].descr->dmac_cmd_status;
	if ((status & GELIC_DESCR_DMA_STAT_MASK) == GELIC_DESCR_DMA_CARDOWNED)
		return NULL;

	//printf("gelicif: packet received (idx %d, status 0x%08x)\n", gelicif->next_rx, status);

	gelicif->next_rx++;
	gelicif->next_rx %= NUM_RX_BUFS;
	if (status & GELIC_DESCR_RX_DMA_CHAIN_END) {
		printf("gelicif: DMA chain ended, restarting RX DMA\n");
		result = lv1_net_start_rx_dma(gelicif->bus_id, gelicif->dev_id,
				gelicif->rxd[gelicif->next_rx].bus_addr, 0);
		if (result)
			fatal("gelicif: lv1_net_start_rx_dma failed");
	}

	if (!(status & GELIC_DESCR_DMA_FRAME_END))
		printf("gelicif: FRAME_END not set?\n");

	/* Obtain the size of the packet and put it into the "len"
	variable. */
	len = gelicif->rxd[idx].descr->valid_size;
	if (!len) {
		printf("gelicif: RX buffer overflow? 0x%x 0x%x\n", \
			   gelicif->rxd[idx].descr->result_size,
			   gelicif->rxd[idx].descr->buf_size);
		chain_rx(gelicif, idx);
		return NULL;
	}

	/* Gelic puts the vlan tag in front of the frame, so strip it */

	u8 *pkt = &gelicif->rxd[idx].pkt[2];
	len -= 2;

	alloc_len = len;

#if ETH_PAD_SIZE
	alloc_len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

	/* We allocate a pbuf chain of pbufs from the pool. */
	p = pbuf_alloc(PBUF_RAW, alloc_len, PBUF_POOL);

	if (p != NULL) {

#if ETH_PAD_SIZE
		pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
		pbuf_take(p, pkt, len);
		/*
		int i;
		for (i=0; i<64; i+=8) {
			printf("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				   ((u8*)p->payload)[i+0], ((u8*)p->payload)[i+1], ((u8*)p->payload)[i+2], ((u8*)p->payload)[i+3],
				   ((u8*)p->payload)[i+4], ((u8*)p->payload)[i+5], ((u8*)p->payload)[i+6], ((u8*)p->payload)[i+7]);
		}
		*/

#if ETH_PAD_SIZE
		pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

		LINK_STATS_INC(link.recv);
	} else {
		printf("gelicif: Could not alloc pbuf!\n");
		//drop packet();
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
	}
	chain_rx(gelicif, idx);

	return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this gelicif
 */
int
gelicif_input(struct netif *netif)
{
	struct gelicif *gelicif;
	struct eth_hdr *ethhdr;
	struct pbuf *p;

	gelicif = netif->state;

	/* move received packet into a new pbuf */
	p = low_level_input(netif);
	/* no packet could be read, silently ignore this */
	if (p == NULL) return 0;
	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

	switch (htons(ethhdr->type)) {
		/* IP or ARP packet? */
		case ETHTYPE_IP:
		case ETHTYPE_ARP:
#if PPPOE_SUPPORT
		/* PPPoE packet? */
		case ETHTYPE_PPPOEDISC:
		case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
			/* full packet send to tcpip_thread to process */
			if (netif->input(p, netif)!=ERR_OK)
			{
				LWIP_DEBUGF(NETIF_DEBUG, ("gelicif_input: IP input error\n"));
				pbuf_free(p);
				p = NULL;
			}
			break;

		default:
			printf("gelicif: unknown packet type 0x%04x\n", ethhdr->type);
			pbuf_free(p);
			p = NULL;
			break;
	}
	return 1;
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this gelicif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
gelicif_init(struct netif *netif)
{
	struct gelicif *gelicif;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	printf("gelicif: gelicif_init()\n");

	gelicif = mem_malloc(sizeof(struct gelicif));
	if (gelicif == NULL) {
		fatal("gelicif: out of memory");
		return ERR_MEM;
	}

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "ps3";
#endif /* LWIP_NETIF_HOSTNAME */

	netif->state = gelicif;
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	/* We directly use etharp_output() here to save a function call.
	* You can instead declare your own function an call etharp_output()
	* from it if you have to do some checks before sending (e.g. if link
	* is available...) */
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	/* initialize the hardware */
	low_level_init(netif);

	return ERR_OK;
}

err_t
gelicif_shutdown(struct netif *netif)
{
	/* deinitialize the hardware */
	low_level_shutdown(netif);

	if (!netif->state)
		fatal("gelicif: interface already freed");

	free(netif->state);
	netif->state = NULL;

	return ERR_OK;
}
