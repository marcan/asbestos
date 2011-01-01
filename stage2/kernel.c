/*  device.c - lv1 device functions

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "kernel.h"
#include "mm.h"
#include "libfdt.h"
#include "string.h"
#include "elf.h"

#define VECSIZE 65536
static u8 vec_buf[VECSIZE];

#define DT_BUFSIZE 65536

extern char __base[];
extern char __devtree[];
extern char dt_blob_start[];

#define ADDR_LIMIT ((u64)__base)

static char bootargs[MAX_CMDLINE_SIZE];

static u64 *entry[3] = {NULL,NULL,NULL}; // function descriptor for the kernel entrypoint

typedef void (*kernel_entry)(void *devtree, void *self, void *null);

extern volatile u64 _thread1_release;
extern volatile u64 _thread1_vector;

static u8 *initrd_start = NULL;
static size_t initrd_size = 0;

static void devtree_prepare(void)
{
	int res, node;
	u64 memreg1[] = {0, mm_bootmem_size};
	u64 memreg2[] = {mm_highmem_addr, mm_highmem_size};

	res = fdt_open_into(dt_blob_start, __devtree, DT_BUFSIZE);
	if (res < 0)
		fatal("fdt_open_into() failed");

	node = fdt_path_offset(__devtree, "/chosen");
	if (node < 0)
		fatal("/chosen node not found in devtree");

	res = fdt_setprop(__devtree, node, "bootargs", bootargs, strlen(bootargs)+1);
	if (res < 0)
		fatal("couldn't set chosen.bootargs property");

	if (initrd_start && initrd_size)
	{
		u64 start, end;
		start = mm_addr_to_kernel(initrd_start);
		res = fdt_setprop(__devtree, node, "linux,initrd-start", &start, sizeof(start));
		if (res < 0)
			fatal("couldn't set chosen.linux,initrd-start property");

		end = mm_addr_to_kernel(initrd_start + initrd_size);
		res = fdt_setprop(__devtree, node, "linux,initrd-end", &end, sizeof(end));
		if (res < 0)
			fatal("couldn't set chosen.linux,initrd-end property");

		res = fdt_add_mem_rsv(__devtree, start, initrd_size);
		if (res < 0)
			fatal("couldn't add reservation for the initrd");
	}
	
	node = fdt_path_offset(__devtree, "/memory");
	if (node < 0)
		fatal("/memory node not found in devtree");

	res = fdt_setprop(__devtree, node, "reg", memreg1, sizeof(memreg1));
	if (res < 0)
		fatal("couldn't set memory.reg property");

	res = fdt_setprop(__devtree, node, "sony,lv1-highmem", memreg2, sizeof(memreg2));
	if (res < 0)
		fatal("couldn't set memory.sony,lv1-highmem property");

	res = fdt_add_mem_rsv(__devtree, (u64)__devtree, DT_BUFSIZE);
	if (res < 0)
		fatal("couldn't add reservation for the devtree");

	res = fdt_pack(__devtree);
	if (res < 0)
		fatal("fdt_pack() failed");

	printf("Device tree prepared\n");
}

static void vecmemclr(u64 dest, u64 size)
{
	u64 end = dest+size;
	if (size && dest < VECSIZE) {
		if (end <= VECSIZE)
			return;
		dest = VECSIZE;
		size = end - dest;
	}
	memset((void*)dest, 0, size);
	sync_before_exec((void*)dest, size);
}

static void vecmemcpy(u64 dest, const void *src, u64 size)
{
	const u8 *p = src;
	u64 end = dest+size;
	if (size && dest < VECSIZE) {
		if (end <= VECSIZE) {
			memcpy(vec_buf+dest, p, size);
			return;
		} else {
			memcpy(vec_buf+dest, p, VECSIZE-dest);
			p += VECSIZE-dest;
			dest = VECSIZE;
			size = end - dest;
		}
	}
	memcpy((void*)dest, p, size);
	sync_before_exec((void*)dest, size);
}

int kernel_load(const u8 *addr, u32 len)
{
	memset(vec_buf, 0, sizeof(vec_buf));

	if (len < sizeof(Elf64_Ehdr))
		return -1;

	Elf64_Ehdr *ehdr = (Elf64_Ehdr *) addr;

	if (memcmp("\x7F" "ELF\x02\x02\x01", ehdr->e_ident, 7)) {
		printf("load_elf_kernel: invalid ELF header 0x%02x 0x%02x 0x%02x 0x%02x\n",
						ehdr->e_ident[0], ehdr->e_ident[1],
						ehdr->e_ident[2], ehdr->e_ident[3]);
		return -1;
	}

	if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		printf("load_elf_kernel: ELF has no program headers\n");
		return -1;
	}

	int count = ehdr->e_phnum;
	if (len < ehdr->e_phoff + count * sizeof(Elf64_Phdr)) {
		printf("load_elf_kernel: image too short for phdrs\n");
		return -1;
	}

	Elf64_Phdr *phdr = (Elf64_Phdr *) &addr[ehdr->e_phoff];

	while (count--) {
		if (phdr->p_type != PT_LOAD) {
			printf("load_elf_kernel: skipping PHDR of type %d\n", phdr->p_type);
		} else {
			if ((phdr->p_paddr+phdr->p_memsz) > ADDR_LIMIT) {
				printf("PHDR out of bounds [0x%lx...0x%lx]\n",
					   phdr->p_paddr, phdr->p_paddr + phdr->p_memsz);
				return -1;
			}

			printf("load_elf_kernel: LOAD 0x%lx @0x%lx [0x%lx/0x%lx]\n", phdr->p_offset,
				   phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);

			vecmemclr(phdr->p_paddr, phdr->p_memsz);
			vecmemcpy(phdr->p_paddr, &addr[phdr->p_offset],
					phdr->p_filesz);
		}
		phdr++;
	}

	ehdr->e_entry &= 0x3ffffffffffffffful;

	entry[0] = (void*)ehdr->e_entry;

	printf("load_elf_kernel: kernel loaded, entry at 0x%lx\n", ehdr->e_entry);

	return 0;
}

void kernel_build_cmdline(const char *parameters, const char *root)
{
	bootargs[0] = 0;

	if (root) {
		strlcat(bootargs, "root=", MAX_CMDLINE_SIZE);
		strlcat(bootargs, root, MAX_CMDLINE_SIZE);
		strlcat(bootargs, " ", MAX_CMDLINE_SIZE);
	}

	if (parameters)
		strlcat(bootargs, parameters, MAX_CMDLINE_SIZE);

	printf("Kernel command line: '%s'\n", bootargs);
}

void kernel_set_initrd(void *start, size_t size)
{
	printf("Initrd at %p/0x%lx: %ld bytes (%ldKiB)\n", start, \
	mm_addr_to_kernel(start), size, size/1024);

	initrd_start = start;
	initrd_size = size;
}

void kernel_launch(void)
{
	devtree_prepare();
	printf("Relocating vectors...\n");
	memcpy((void*)0, vec_buf, VECSIZE);
	sync_before_exec((void*)0, VECSIZE);
	printf("Letting thread1 run loose...\n");
	_thread1_vector = 0x60; /* this is __secondary_hold in Linux */
	_thread1_release = 1;
	printf("Taking the plunge...\n");
	debug_shutdown();
	((kernel_entry)entry)(__devtree, entry[0], NULL);
	lv1_panic(0);
}
