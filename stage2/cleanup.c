/*  cleanup.c - clean up after lv2

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "config.h"
#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "cleanup.h"
#include "device.h"

void lv2_cleanup(void)
{
	s64 result;
	int i,j;
	printf("Cleaning up after Lv-2...\n");

	if (lv1_gpu_close() == 0)
		printf("GPU closed\n");

	u64 ppe_id;
	result = lv1_get_logical_ppe_id(&ppe_id);
	if (result)
		fatal("could not get PPE id");

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

	if (lv1_deconfigure_virtual_uart_irq() == 0)
		printf("Deconfigured VUART IRQ\n");

	for (i=0; i<64; i++) {
		result = lv1_set_virtual_uart_param(i, 2, 0);
		if (result == 0)
			printf("Cleared IRQ mask for VUART %d\n", i);
	}

}