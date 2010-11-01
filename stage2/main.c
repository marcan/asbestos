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
#include "kbootconf.h"

#include "tftp.h"

enum state_t {
	STATE_START,
	STATE_WAIT_NET,
	STATE_GOT_NET,
	STATE_GOT_CONF,
	STATE_GOT_KERNEL,
	STATE_WAIT_TFTP,
	STATE_IDLE,
};

extern u64 _thread1_active;

static enum state_t gstate;

static struct tftp_client *tftp = NULL;
static enum state_t t_next;
static enum tftp_status t_status;
static size_t t_recvd;

static int boot_entry = 0;
static void *kernel_buf;

void shutdown_and_launch(void);

void tftp_cb(void *arg, struct tftp_client *clt, enum tftp_status status, size_t recvd)
{
	printf("Transfer complete, status %d, size: %ld bytes\n", status, recvd);

	t_recvd = recvd;
	t_status = status;
	gstate = t_next;
}

void seq_tftp_get(const char *name, void *buf, size_t bufsize, enum state_t next)
{
	t_next = next;
	gstate = STATE_WAIT_TFTP;
	tftp_get(tftp, name, buf, bufsize, tftp_cb, NULL);
}

void sequence(void)
{
	switch (gstate) {
	case STATE_START:
		printf("Waiting for DHCP lease...\n");
		gstate = STATE_WAIT_NET;
		break;
	case STATE_WAIT_NET:
		if (eth.dhcp->state == DHCP_BOUND) {
			gstate = STATE_GOT_NET;
		}
		break;
	case STATE_GOT_NET:
		printf("Network is up:\n");
		printf(" IP address:  %d.%d.%d.%d\n", P_IP(eth.ip_addr.addr));
		printf(" Netmask:     %d.%d.%d.%d\n", P_IP(eth.netmask.addr));
		printf(" Gateway:     %d.%d.%d.%d\n", P_IP(eth.gw.addr));
		printf(" TFTP server: %d.%d.%d.%d\n", P_IP(eth.dhcp->offered_si_addr.addr));
		if (eth.dhcp->boot_file_name)
			printf(" Bootfile:    %s\n", eth.dhcp->boot_file_name);
		else
			printf(" Bootfile:    NONE\n");

#ifdef NETRPC_ENABLE
		netrpc_init();
#endif
#ifdef AUTO_TFTP
		if (eth.dhcp->offered_si_addr.addr == 0 || !eth.dhcp->boot_file_name) {
			printf("Missing boot settings, cannot continue. Rebooting...\n");
			lv1_panic(1);
		}

		tftp = tftp_new();
		if (!tftp)
			fatal("tftp alloc failed");

		tftp_connect(tftp, &eth.dhcp->offered_si_addr, 69);

		printf("Downloading configuration file...\n");
		seq_tftp_get((char*)eth.dhcp->boot_file_name, conf_buf, MAX_KBOOTCONF_SIZE-1, STATE_GOT_CONF);
#else
		gstate = STATE_IDLE;
#endif
		break;

	case STATE_GOT_CONF:
		if (t_status != TFTP_STATUS_OK) {
			printf("Transfer did not complete successfully\n");
			printf("Rebooting...\n");
			lv1_panic(1);
		}

		printf("Configuration: %ld bytes\n", t_recvd);
		conf_buf[t_recvd] = 0;

		kbootconf_parse();

		if (conf.num_kernels == 0) {
			printf("No kernels found in configuration file. Rebooting...\n");
			lv1_panic(1);
		}

		boot_entry = conf.default_idx;

		printf("Starting to boot '%s'\n", conf.kernels[boot_entry].label);
		printf("Downloading kernel...\n");

		kernel_buf = mm_highmem_freestart();
		seq_tftp_get(conf.kernels[boot_entry].kernel, kernel_buf, mm_highmem_freesize(), STATE_GOT_KERNEL);
		break;

	case STATE_GOT_KERNEL:
		if (t_status != TFTP_STATUS_OK) {
			printf("Transfer did not complete successfully. Rebooting...\n");
			lv1_panic(1);
		}

		mm_highmem_reserve(t_recvd);
		if (kernel_load(kernel_buf, t_recvd) != 0) {
			printf("Failed to load kernel. Rebooting...\n");
			lv1_panic(1);
		}

		kernel_build_cmdline(conf.kernels[boot_entry].parameters, conf.kernels[boot_entry].root);
		shutdown_and_launch();
		break;

	case STATE_WAIT_TFTP:
		break;

	case STATE_IDLE:
		break;
	}
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

	gstate = STATE_START;
	while(1) {
		net_poll();
		sequence();
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
