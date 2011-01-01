/*  stage2.ld - AsbestOS stage2 linker script

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("binary")
OUTPUT_ARCH(powerpc:common64)

ENTRY(main)

PHDRS {
	all		PT_LOAD FLAGS(7);
}

SECTIONS {
	.hdr : {
		QUAD(main)
	} :all

	.text : {
		*(.text)
		*(.text.*)
	}

	. = ALIGN(64);
	.opd : {
		*(.opd)
	}
	.toc : {
		__toc_start = .;
		*(.got)
		*(.toc)
	}

	.rodata : {
		*(.rodata)
		*(.rodata.*)
	}

	.data : {
		*(.data)
		*(.data.*)
		*(.sdata)
		*(.sdata.*)
	}
	. = ALIGN(64);
	__self_end = .;

	__bss_start = .;
	.bss : {
		*(.sbss)
		*(.sbss.*)
		*(COMMON)
		*(.bss)
		*(.bss.*)
	}
	__bss_end = .;

	. = ALIGN(64);

	.ftr : {
		QUAD(0)
	}
}
