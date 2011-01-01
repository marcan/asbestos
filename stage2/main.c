/*  main.c - AsbestOS Stage2 main C entrypoint

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

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
#include "cleanup.h"
#include "kernel.h"
#include "kbootconf.h"

#include "tftp.h"

#ifdef USE_NETWORK
# include "network.h"
#endif
#ifdef NETRPC_ENABLE
# include "netrpc.h"
#endif

#ifdef AUTO_HDD
# include "ff.h"
# include "diskio.h"
#endif

extern u64 _thread1_active;

static int boot_entry = 0;
static void *kernel_buf;
static void *initrd_buf;

#ifdef USE_NETWORK
enum state_t {
	STATE_START,
	STATE_WAIT_NET,
	STATE_GOT_NET,
	STATE_GOT_CONF,
	STATE_GOT_KERNEL,
	STATE_GOT_INITRD,
	STATE_BOOT,
	STATE_WAIT_TFTP,
	STATE_IDLE,
};

static enum state_t gstate;

static struct tftp_client *tftp = NULL;
static enum state_t t_next;
static enum tftp_status t_status;
static size_t t_recvd;

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

		if (kernel_load(kernel_buf, t_recvd) != 0) {
			printf("Failed to load kernel. Rebooting...\n");
			lv1_panic(1);
		}

		if (conf.kernels[boot_entry].initrd && conf.kernels[boot_entry].initrd[0]) {
			printf("Downloading initrd...\n");
			initrd_buf = mm_highmem_freestart();
			seq_tftp_get(conf.kernels[boot_entry].initrd, initrd_buf, mm_highmem_freesize(), STATE_GOT_INITRD);
		} else {
			gstate = STATE_BOOT;
		}
		break;

	case STATE_GOT_INITRD:
		if (t_status != TFTP_STATUS_OK) {
			printf("Transfer did not complete successfully. Rebooting...\n");
			lv1_panic(1);
		}

		mm_highmem_reserve(t_recvd);
		kernel_set_initrd(initrd_buf, t_recvd);

		gstate = STATE_BOOT;
		break;

	case STATE_BOOT:
		kernel_build_cmdline(conf.kernels[boot_entry].parameters, conf.kernels[boot_entry].root);
		shutdown_and_launch();
		break;

	case STATE_WAIT_TFTP:
		break;

	case STATE_IDLE:
		break;
	}
}
#endif

#ifdef AUTO_HDD
int readfile(char *name, void *buf, u32 maxlen)
{
	static FIL fil;
	FRESULT res;
	u32 bytes_read;

	res = f_open(&fil, name, FA_READ);
	if (res != FR_OK)
		return -res;
	res = f_read(&fil, buf, maxlen, &bytes_read);
	if (res != FR_OK)
		return -res;
	f_close(&fil);
	return bytes_read;
}
#endif

int main(void)
{
	udelay(2000000);
	debug_init();
	printf("\n\nAsbestOS Stage 2 starting.\n");
	printf("Waiting for thread 1...\n");
	while(!_thread1_active);
	printf("Thread 1 is alive, all systems go.\n");

	exceptions_init();
	lv2_cleanup();
	mm_init();

#ifdef USE_NETWORK
	net_init();

	gstate = STATE_START;
	while(1) {
		net_poll();
		sequence();
	}
#endif
#ifdef AUTO_HDD
	static FATFS fatfs;
	int res;
	DSTATUS stat;

	stat = disk_initialize(0);
	if (stat & ~STA_PROTECT)
		fatal("disk_initialize() failed");

	printf("Mounting filesystem...\n");
	res = f_mount(0, &fatfs);
	if (res != FR_OK)
		fatal("f_mount() failed");

	printf("Reading kboot.conf...\n");
	res = readfile("/kboot.conf", conf_buf, MAX_KBOOTCONF_SIZE-1);
	if (res <= 0) {
		printf("Could not read kboot.conf (%d), panicking\n", res);
		lv1_panic(0);
	}
	conf_buf[res] = 0;
	kbootconf_parse();

	if (conf.num_kernels == 0) {
		printf("No kernels found in configuration file. Panicking...\n");
		lv1_panic(0);
	}

	boot_entry = conf.default_idx;

	printf("Starting to boot '%s'\n", conf.kernels[boot_entry].label);
	printf("Loading kernel (%s)...\n", conf.kernels[boot_entry].kernel);

	kernel_buf = mm_highmem_freestart();
	res = readfile(conf.kernels[boot_entry].kernel, kernel_buf, mm_highmem_freesize());
	if (res <= 0) {
		printf("Could not read kernel (%d), panicking\n", res);
		lv1_panic(0);
	}
	printf("Kernel size: %d\n", res);

	if (kernel_load(kernel_buf, res) != 0) {
		printf("Failed to load kernel. Rebooting...\n");
		lv1_panic(1);
	}

	if (conf.kernels[boot_entry].initrd && conf.kernels[boot_entry].initrd[0]) {
		initrd_buf = mm_highmem_freestart();
		res = readfile(conf.kernels[boot_entry].initrd, initrd_buf, mm_highmem_freesize());
		if (res <= 0) {
			printf("Could not read initrd (%d), panicking\n", res);
			lv1_panic(0);
		}
		printf("Initrd size: %d\n", res);
		mm_highmem_reserve(res);
		kernel_set_initrd(initrd_buf, res);
	}

	kernel_build_cmdline(conf.kernels[boot_entry].parameters, conf.kernels[boot_entry].root);

	f_mount(0, NULL);
	disk_shutdown(0);
	mm_shutdown();
	kernel_launch();

#endif
	printf("End of main() reached! Rebooting...\n");
	lv1_panic(1);
	return 0;
}

#ifdef USE_NETWORK
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
#endif
