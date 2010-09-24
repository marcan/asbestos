/*  main.c - AsbestOS Stage2 main C entrypoint

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "malloc.h"
#include "time.h"

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"

#include "gelic_netif.h"
#include "netif/etharp.h"

static struct netif eth;
static struct ip_addr ipaddr, netmask, gw;

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
	printf("Network initialized\n");
}

void net_loop(void) {
	u64 now = gettb();
#if LWIP_TCP
	u64 last_tcp_time = now;
#endif
	u64 last_arp_time = now;
	u64 last_dhcp_coarse_time = now;
	u64 last_dhcp_fine_time = now;
	printf("now = %lx\n", now);
	printf("Entering net mainloop\n");
	int i = 0x100;
	while(i) {
		if (gelicif_input(&eth)) {
			i--;
		}
		now = gettb();
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
	printf("Mainloop done\n");
}


int main(void) {
	debug_init();
	printf("\n\nAsbestOS Stage 2 starting...\n");
	net_init();
	net_loop();
	lv1_panic(1);
	return 0;
}
