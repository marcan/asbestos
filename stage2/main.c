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
#include "klaunch.h"

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"

#include "tftp.h"

#include "gelic_netif.h"
#include "netif/etharp.h"

static struct netif eth;
static struct ip_addr ipaddr, netmask, gw;

int network_up = 0;

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
	printf("Waiting for DHCP lease...\n");
}

void start_net_ops(void);

void net_loop(void) {
	u64 now = gettb();
#if LWIP_TCP
	u64 last_tcp_time = now;
#endif
	u64 last_arp_time = now;
	u64 last_dhcp_coarse_time = now;
	u64 last_dhcp_fine_time = now;
	printf("Entering net mainloop\n");
	while(1) {
		gelicif_input(&eth);
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
		if (!network_up && eth.dhcp->state == DHCP_BOUND) {
			network_up = 1;
			start_net_ops();
		}
	}
	printf("Mainloop done\n");
}

#define P_IP(w) (u8)((w)>>24), (u8)((w)>>16), (u8)((w)>>8), (u8)(w)

extern u8 __zimage_load_base[];
u8 *recv_buf;

struct tftp_client *tftp;

void tftp_cb(void *arg, struct tftp_client *clt, enum tftp_status status, size_t recvd)
{
	printf("Transfer complete, status %d. Image size: %ld bytes\n", status, recvd);

	if (status == TFTP_STATUS_OK) {
		printf("Relocating kernel...\n");
		kload(recvd);
		printf("Taking the plunge...\n");
		klaunch();
		fatal("klaunch returned\n");
	} else {
		printf("Transfer did not complete successfully\n");
		printf("Rebooting...\n");
		lv1_panic(1);
	}
}

void start_net_ops(void)
{
	printf("Network is up:\n");
	printf(" IP address:  %d.%d.%d.%d\n", P_IP(eth.ip_addr.addr));
	printf(" Netmask:     %d.%d.%d.%d\n", P_IP(eth.netmask.addr));
	printf(" Gateway:     %d.%d.%d.%d\n", P_IP(eth.gw.addr));
	printf(" TFTP server: %d.%d.%d.%d\n", P_IP(eth.dhcp->offered_si_addr.addr));
	if (eth.dhcp->boot_file_name)
		printf(" Bootfile:    %s\n", eth.dhcp->boot_file_name);
	else
		printf(" Bootfile:    NONE\n");

	if (eth.dhcp->offered_si_addr.addr == 0 || !eth.dhcp->boot_file_name) {
		printf("Missing boot settings, cannot continue. Shutting down...\n");
		lv1_panic(0);
	}

	tftp = tftp_new();
	if (!tftp)
		fatal("tftp alloc failed");

	tftp_connect(tftp, &eth.dhcp->offered_si_addr, 69);

	// memmove it later
	recv_buf = __zimage_load_base;

	tftp_get(tftp, (char*)eth.dhcp->boot_file_name, recv_buf, 10*1024*1024, tftp_cb, NULL);
}

int main(void) {
	debug_init();
	printf("\n\nAsbestOS Stage 2 starting...\n");
	net_init();
	net_loop();
	lv1_panic(1);
	return 0;
}
