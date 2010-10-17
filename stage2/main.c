/*  main.c - AsbestOS Stage2 main C entrypoint

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "config.h"
#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "malloc.h"
#include "time.h"
#include "klaunch.h"
#include "device.h"
#include "exceptions.h"
#include "mm.h"
#include "netrpc.h"

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/tcp.h"

#include "tftp.h"

#include "gelic_netif.h"
#include "netif/etharp.h"

extern volatile u64 _thread1_active;
extern volatile u64 _thread1_release;
extern volatile u64 _thread1_vector;

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

struct tftp_client *tftp = NULL;

void shutdown_and_launch(size_t recvd);

void tftp_cb(void *arg, struct tftp_client *clt, enum tftp_status status, size_t recvd)
{
	printf("Transfer complete, status %d. Image size: %ld bytes\n", status, recvd);

	if (status == TFTP_STATUS_OK) {
		shutdown_and_launch(recvd);
	} else {
		printf("Transfer did not complete successfully\n");
		printf("Rebooting...\n");
		lv1_panic(1);
	}
}

void shutdown_and_launch(size_t recvd)
{
#ifdef NETRPC_ENABLE
	netrpc_shutdown();
#endif
	if (tftp)
		tftp_remove(tftp);
	printf("Releasing DHCP lease...\n");
	dhcp_release(&eth);
	dhcp_stop(&eth);
	printf("Shutting down network...\n");
	netif_remove(&eth);
	gelicif_shutdown(&eth);

	mm_shutdown();
	printf("Relocating kernel...\n");
	kload(recvd);
	printf("Letting thread1 run loose...\n");
	_thread1_vector = 0x100;
	_thread1_release = 1;
	printf("Taking the plunge...\n");
	debug_shutdown();
	klaunch();
	lv1_panic(0);
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

#ifdef AUTO_TFTP
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
#endif
#ifdef NETRPC_ENABLE
	netrpc_init();
#endif
}

void lv2_cleanup(void)
{
	s64 result;
	int i,j;
	printf("Cleaning up after Lv-2...\n");

	if (lv1_gpu_close() == 0)
		printf("GPU closed\n");

	if (lv1_deconfigure_virtual_uart_irq() == 0)
		printf("Deconfigured VUART IRQ\n");

	u64 ppe_id;
	result = lv1_get_logical_ppe_id(&ppe_id);

	printf("PPE id: %ld\n", ppe_id);

	u64 vas_id;
	result = lv1_get_virtual_address_space_id_of_ppe(ppe_id , &vas_id );
	printf("VAS id: %ld\n", vas_id);
	result = lv1_select_virtual_address_space(0);
	if (result == 0)
		printf("Selected VAS 0\n");

	int count = 0;

	for (i=0; i<512; i++)
		for (j=0; j<2; j++)
			if (lv1_disconnect_irq_plug_ext(ppe_id, j, i) == 0)
				count++;

	printf("Disconnected %d IRQ plugs\n", count);

	for (i=0, count=0; i<256; i++)
		if (lv1_destruct_io_irq_outlet(i) == 0)
			count++;
	printf("Destructed %d IRQ outlets\n", count);

	count = 0;
	if (lv1_configure_irq_state_bitmap(ppe_id, 0, 0) == 0) count++;
	if (lv1_configure_irq_state_bitmap(ppe_id, 1, 0) == 0) count++;
	printf("Removed %d state bitmaps\n", count);

	printf("Closed %d devices\n", close_all_devs());

	for (i=0; i<256; i++) {
		if (lv1_disable_logical_spe(i, 0) == 0)
			printf("Disabled logical spe %d\n", i);
		if (lv1_destruct_logical_spe(i) == 0)
			printf("Destroyed logical spe %d\n", i);
	}

	u64 size = 256*1024*1024;
	int blocks = 0;
	u64 total = 0;

	printf("Allocating all possible lv1 memory...\n");

	u64 ctr_max = 0;

	while (size >= 4096) {
		u64 newaddr;
		u64 muid;
		u64 cur_ctr;
		result = lv1_allocate_memory(size, 12, 0, 0, &newaddr, &muid);
		if (result) {
			size >>= 1;
			continue;
		}
		cur_ctr = (newaddr & 0x03fffffff000)>>12;
		if (cur_ctr > ctr_max)
			ctr_max = cur_ctr;
		blocks++;
		total += size;
	}

	printf("Allocated %ld bytes in %d blocks\n", total, blocks);
	printf("Current allocation address counter is 0x%lx\n", ctr_max);

	printf("Brute forcing Lv1 allocation regions...\n");

	u64 counter;
	u64 sbits;
	total = 0;
	blocks = 0;
	u64 prev = -1;

	u64 ctr_limit = 0x40000;

	for (counter = 0; counter <= ctr_limit; counter++) {
		for (sbits = 12; sbits <= 30; sbits++) {
			u64 addr = (counter<<12) | (sbits<<42);
			if (counter > ctr_max && (addr & 0xfffff))
				continue;
			u64 start_address, size, access_rights, page_size, flags;
			result = lv1_query_logical_partition_address_region_info(addr,
				&start_address, &size, &access_rights, &page_size, &flags);
			if (result == 0 && start_address != prev) {
				printf("%012lx: size %7lx rights %lx pagesize %ld (%6x) flags 0x%016lx ", start_address, size, access_rights, page_size, 1<<page_size, flags);
				result = lv1_release_memory(start_address);
				if (result == 0) {
					blocks++;
					total += size;
					printf("CLEANED\n");
				} else if (flags != 0) {
					if ((flags>>60) == 0xc) {
						if (lv1_unmap_htab(start_address) == 0)
							printf("CLEANED ");
						printf("HTAB\n");
					} else {
						printf("IOMEM\n");
					}
				} else {
					printf("ERROR\n");
				}
				prev = start_address;
			}
		}
	}

	printf("Cleaned up %ld bytes in %d blocks\n", total, blocks);

	for (i=0; i<16; i++)
	if (lv1_destruct_virtual_address_space(i) == 0)
		printf("Destroyed VAS %d\n", i);
}


int main(void)
{
	debug_init();
	printf("\n\nAsbestOS Stage 2 starting.\n");
	printf("Waiting for thread 1...\n");
	while(!_thread1_active);
	printf("Thread 1 is alive, all systems go.\n");

	exceptions_init();
	lv2_cleanup();
	mm_init();

	net_init();
	net_loop();
	lv1_panic(1);
	return 0;
}
