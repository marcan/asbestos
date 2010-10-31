/*  mm.c - memory management

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "debug.h"
#include "mm.h"
#include "lv1call.h"
#include "string.h"
#include "repo.h"

#define MSR_IR (1UL<<5)
#define MSR_DR (1UL<<4)

#define NUM_SLB 64

int seg_idx = 1;
u64 vas_id;

#define NUM_MMIO 128

int mm_inited = 0;

// highmem size is aligned to 16MB blocks
#define HIGHMEM_PBITS 24
#define HIGHMEM_PALIGN (1<<HIGHMEM_PBITS)

// highmem pointer is aligned to 64kB
#define HIGHMEM_ALIGN 65536

u64 mm_bootmem_size;
u64 mm_highmem_addr;
u64 mm_highmem_size;

static u64 highmem_ptr;

struct mmio_region {
	u64 start;
	u32 end;
};

struct mmio_region mmios[NUM_MMIO];

int mm_addmmio(u64 start, u32 size)
{
	int i;
	u64 end = start+size;

	if (!size)
		return -1;

	if ((start & 0xfff) || (end & 0xfff)) {
		printf("WARNING: MMIO region is not aligned (0x%016lx 0x%x)", start, size);
		start &= ~0xfff;
		start = (start+0xfff)&~0xfff;
	}

	if (!start)
		return -1;

	for (i=0; i<NUM_MMIO; i++) {
		if (mmios[i].start == 0) {
			mmios[i].start = start;
			mmios[i].end = end;
			return 0;
		}
	}
	printf("WARNING: Maximum number of MMIO regions exceeded\n");
	return -1;
}

int mm_delmmio(u64 start)
{
	int i;
	start &= ~0xfff;
	for (i=0; i<NUM_MMIO; i++) {
		if (mmios[i].start == start) {
			mmios[i].start = 0;
			mmios[i].end = 0;
			return 0;
		}
	}
	printf("WARNING: Tried to delete nonexistent MMIO region 0x%016lx\n", start);
	return -1;
}

int mm_clrmmio(void)
{
	memset(mmios, 0, sizeof(mmios));
	return 0;
}

int mm_ismmio(u64 addr)
{
	int i;
	for (i=0; i<NUM_MMIO; i++) {
		if (mmios[i].start <= addr && mmios[i].end > addr)
			return 1;
	}
	return 0;
}

int mm_loadseg(u64 addr)
{
	if (!mm_inited)
		return 0;
	u64 esid = addr >> 28;

	int index = 0;

	if (esid) {
		index = seg_idx;
		seg_idx++;
		if (seg_idx >= NUM_SLB)
			seg_idx = 1;
	}

	u64 rs, rb;

	rs = (esid<<12) | 0x400;
	rb = (esid<<28) | (1<<27) | index;

	asm("slbmte %0,%1 ; isync" :: "r"(rs), "r"(rb));
	printf("SLB[%d] = %016lx %016lx (caused by %016lx)\n", index, rs, rb, addr);
	return 1;
}

int mm_loadhtab(u64 addr)
{
	if (!mm_inited)
		return 0;
	s64 result;

	//printf("Load HTAB for %016lx\n", addr);

	addr &= ~0xfff;

	u32 wimg = 0x2;
	if (mm_ismmio(addr)) {
		wimg = 0x5;
	}

	u64 hpte_v;
	u64 hpte_r;
	u64 inserted_index, evicted_v, evicted_r;

	hpte_v = (addr>>23)<<7;
	hpte_v |= 0x1;
	hpte_r = addr|(wimg<<3);

	u64 htab_hash_mask = ((256*1024)>>7)-1;

	u64 hash = (addr >> 28) ^ ((addr & 0x0fffffffUL) >> 12);
	u64 hpte_group = ((hash & htab_hash_mask) * 8);

	result = lv1_insert_htab_entry(0, hpte_group,
				       hpte_v, hpte_r,
				       0x10, 0,
				       &inserted_index,
				       &evicted_v, &evicted_r);
	if (result) {
		printf("Failed to insert HTAB entry:\n");
		printf("hpte_group=0x%lx hpte_v=0x%016lx hpte_r=0x%016lx\n", hpte_group, hpte_v, hpte_r);
		printf("result=%ld inserted_index=%lx evicted_v=%lx evicted_r=%lx\n", result, inserted_index, evicted_v, evicted_r);
		return 0;
	} else {
		return 1;
	}
}

void mm_init(void)
{
	s32 result;

	printf("Initializing memory management...\n");

	memset(mmios, 0, sizeof(mmios));
	u64 act_htab_size;
	result = lv1_construct_virtual_address_space(18, 0, 0, &vas_id, &act_htab_size);
	if (result)
		fatal("Could not construct VAS\n");
	printf("New VAS ID: %ld\n", vas_id);

	result = lv1_select_virtual_address_space(vas_id);
	if (result)
		fatal("Could not switch VAS\n");

	asm("slbia");
	mm_loadseg(0);

	mm_inited = 1;

	u64 msr;
	asm("mfmsr %0" : "=r"(msr));
	msr |= MSR_IR | MSR_DR;
	asm("isync ; mtmsrd %0,0 ; isync" :: "r"(msr));

	printf("MMU initialized, now running in virtual mode\n");

	u64 ppe_id, lpar_id;

	result = lv1_get_logical_ppe_id(&ppe_id);
	if (result)
		fatal("could not get PPE ID");

	result = lv1_get_logical_partition_id(&lpar_id);
	if (result)
		fatal("could not get LPAR ID");

	u64 rm_size, rgntotal, v2;
	
	result = lv1_get_repository_node_value(lpar_id, FIELD_FIRST("bi",0),
					FIELD("pu",0), ppe_id, FIELD("rm_size",0),
					&rm_size, &v2);
	if (result)
		fatal("could not get bootmem size");

	result = lv1_get_repository_node_value(lpar_id, FIELD_FIRST("bi",0),
					FIELD("rgntotal",0), 0, 0, &rgntotal, &v2);
	if (result)
		fatal("could not get total region size");

	printf("Region size = %ld bytes (%ldMB)\n", rgntotal, rgntotal>>20);
	printf("Bootmem = %ld bytes (%ldMB)\n", rm_size, rm_size>>20);

	mm_bootmem_size = rm_size;

	u64 exp_size = rgntotal - rm_size;
	exp_size &= ~(HIGHMEM_PALIGN-1);

	while (exp_size) {
		u64 muid, addr;
		result = lv1_allocate_memory(exp_size, HIGHMEM_PBITS, 0, 0, &addr, &muid);
		if (result == 0) {
			mm_highmem_addr = addr;
			mm_highmem_size = exp_size;
			break;
		} else {
			printf("Allocation of %ld bytes failed (%d), trying again...\n", exp_size, result);
			exp_size -= HIGHMEM_PALIGN;
		}
	}

	if (!exp_size)
		fatal("could not allocate highmem");

	highmem_ptr = mm_highmem_addr;

	printf("Highmem = %ld bytes (%ldMB) at 0x%lx\n", mm_highmem_size, mm_highmem_size>>20, mm_highmem_addr);
}

void mm_shutdown(void)
{
	s64 result;
	printf("Shutting down memory management...\n");

	u64 msr;
	asm("mfmsr %0" : "=r"(msr));
	msr &= ~(MSR_IR | MSR_DR);
	asm("mtmsrd %0,0 ; isync" :: "r"(msr));

	printf("Now running in real mode\n");

	result = lv1_select_virtual_address_space(0);
	if (result)
		fatal("Could not switch to VAS 0\n");

	result = lv1_destruct_virtual_address_space(vas_id);
	if (result)
		fatal("Could not destroy VAS\n");

	printf("Destroyed VAS %ld\n", vas_id);
	vas_id = 0;

	mm_inited = 0;
}

void mm_highmem_reserve(size_t size)
{
	if (size > mm_highmem_freesize())
		fatal("mm_highmem_reserve: too large");

	highmem_ptr += size + HIGHMEM_ALIGN - 1;
	highmem_ptr &= ~(HIGHMEM_ALIGN-1);
}

void *mm_highmem_freestart(void)
{
	return (void*)highmem_ptr;
}

size_t mm_highmem_freesize(void)
{
	return mm_highmem_addr + mm_highmem_size - highmem_ptr;
}

void sync_before_exec(void *addr, int len)
{
	u64 p = ((u64)addr) & ~0x1f;
	u64 end = ((u64)addr) + len;

	while (p < end) {
		asm("dcbst 0,%0 ; sync ; icbi 0,%0" :: "r"(p));
		p += 32;
	}

	asm("sync ; isync");
}
