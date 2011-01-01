/*  network.c - Toplevel networking functions

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "debug.h"
#include "string.h"
#include "time.h"

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"

#include "gelic_netif.h"
#include "netif/etharp.h"

struct netif eth;
static struct ip_addr ipaddr, netmask, gw;

#if LWIP_TCP
static u64 last_tcp_time;
#endif
static u64 last_arp_time;
static u64 last_dhcp_coarse_time;
static u64 last_dhcp_fine_time;

void net_init(void) {
	memset(&ipaddr, 0, sizeof(ipaddr));
	memset(&netmask, 0, sizeof(ipaddr));
	memset(&gw, 0, sizeof(ipaddr));

	printf("Initializing network...\n");
	lwip_init();
	printf("lwIP Initialized\n");
	netif_add(&eth, &ipaddr, &netmask, &gw, NULL, gelicif_init, ethernet_input);
	netif_set_default(&eth);
	netif_set_up(&eth);
	printf("Ethernet interface initialized\n");
	printf("Starting DHCP\n");
	dhcp_start(&eth);

	u64 now = gettb();
#if LWIP_TCP
	last_tcp_time = now;
#endif
	last_arp_time = last_dhcp_coarse_time = last_dhcp_fine_time = now;
}

void net_poll(void) {

	u64 now = gettb();

	gelicif_input(&eth);
	if ((now - last_arp_time) >= (ARP_TMR_INTERVAL*TICKS_PER_MS)) {
		etharp_tmr();
		last_arp_time = now;
	}
#if LWIP_TCP
	if ((now - last_tcp_time) >= (TCP_TMR_INTERVAL*TICKS_PER_MS)) {
		tcp_tmr();
		last_tcp_time = now;
	}
#endif
	if ((now - last_dhcp_coarse_time) >= (DHCP_COARSE_TIMER_SECS*TICKS_PER_SEC)) {
		dhcp_coarse_tmr();
		last_dhcp_coarse_time = now;
	}
	if ((now - last_dhcp_fine_time) >= (DHCP_FINE_TIMER_MSECS*TICKS_PER_MS)) {
		dhcp_fine_tmr();
		last_dhcp_fine_time = now;
	}
}

void net_shutdown(void) {
	printf("Releasing DHCP lease...\n");
	dhcp_release(&eth);
	dhcp_stop(&eth);
	printf("Shutting down network...\n");
	netif_remove(&eth);
	gelicif_shutdown(&eth);
}
