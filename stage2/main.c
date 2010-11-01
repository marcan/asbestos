/*  main.c - AsbestOS Stage2 main C entrypoint

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "config.h"
#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "malloc.h"
#include "time.h"
#include "device.h"
#include "exceptions.h"
#include "mm.h"
#include "network.h"
#include "netrpc.h"
#include "cleanup.h"
#include "kernel.h"

#include "tftp.h"

int network_up = 0;

static u8 *recv_buf;
static size_t recv_buf_size;

void start_net_ops(void);
static struct tftp_client *tftp = NULL;

extern u64 _thread1_active;

void shutdown_and_launch(void);

void tftp_cb(void *arg, struct tftp_client *clt, enum tftp_status status, size_t recvd)
{
	printf("Transfer complete, status %d. Image size: %ld bytes\n", status, recvd);

	if (status == TFTP_STATUS_OK) {
		mm_highmem_reserve(recvd);
		if (kernel_load(recv_buf, recvd) != 0) {
			printf("Failed to load kernel\n");
			printf("Rebooting...\n");
			lv1_panic(1);
		}
		shutdown_and_launch();
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

#ifdef AUTO_TFTP
	if (eth.dhcp->offered_si_addr.addr == 0 || !eth.dhcp->boot_file_name) {
		printf("Missing boot settings, cannot continue. Shutting down...\n");
		lv1_panic(0);
	}

	tftp = tftp_new();
	if (!tftp)
		fatal("tftp alloc failed");

	tftp_connect(tftp, &eth.dhcp->offered_si_addr, 69);

	recv_buf = mm_highmem_freestart();
	recv_buf_size = mm_highmem_freesize();

	tftp_get(tftp, (char*)eth.dhcp->boot_file_name, recv_buf, recv_buf_size, tftp_cb, NULL);
#endif
#ifdef NETRPC_ENABLE
	netrpc_init();
#endif
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
	while(1) {
		net_poll();
		if (!network_up && eth.dhcp->state == DHCP_BOUND) {
			network_up = 1;
			start_net_ops();
		}
	}
}

void shutdown_and_launch(void)
{
#ifdef NETRPC_ENABLE
	netrpc_shutdown();
#endif
	if (tftp)
		tftp_remove(tftp);

	net_shutdown();
	mm_shutdown();
	kernel_launch();
}
