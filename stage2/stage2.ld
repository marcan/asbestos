/*  stage2.ld - AsbestOS stage2 linker script

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)

/* stage2 runs in realmode */
__base = 0xc00000;

/* address of the devicetree buffer */
__devtree = 0xff0000;

ENTRY(_start)

PHDRS {
	text		PT_LOAD FLAGS(5);
	rodata		PT_LOAD FLAGS(4);
	data		PT_LOAD FLAGS(6);
	bss			PT_LOAD FLAGS(6);
}

SECTIONS {
	. = __base;
	__self_start = .;

	.start : {
		*(.start)
	} :text = 0

	.text : {
		*(.text)
		*(.text.*)
	}

	. = ALIGN(64);
	.opd : {
		*(.opd)
	} :rodata = 0
	.toc : {
		__toc_start = .;
		*(.got)
		*(.toc)
	}

	.rodata : {
		*(.rodata)
		*(.rodata.*)
	}

	. = ALIGN(64);
	.data : {
		*(.data)
		*(.data.*)
		*(.sdata)
		*(.sdata.*)
	} :data = 0
	__self_end = .;

	__bss_start = .;
	.bss : {
		*(.sbss)
		*(.sbss.*)
		*(COMMON)
		*(.bss)
		*(.bss.*)
	} :bss = 0
	__bss_end = .;

	. = ALIGN(64);

	.stack : {
		_stack_top = .;
		. += 32768;
		_stack_bot = .;
	} :NONE

	__freemem = .;
}
