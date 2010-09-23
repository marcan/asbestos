/*  stage1.ld - AsbestOS stage1 linker script

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

OUTPUT_FORMAT("elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)

__base = 0x80000000007ff000;

__stage2 = 0x8000000000780000;

ENTRY(_start)

PHDRS {
	stage1		PT_LOAD FLAGS(7);
}

SECTIONS {
	. = __base;
	__self_start = .;

	.start : { *(.start) } :stage1 = 0

	. = ALIGN(32);
	.text : {
		*(.text)
		*(.text.*)
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
	.bss : {
		*(.sbss)
		*(.sbss.*)
		*(COMMON)
		*(.bss)
		*(.bss.*)
		LONG(0)
	}

	. = ALIGN(32);
	__toc = . + 0x8000;
	.toc : {
		*(.toc)
		*(.toc.*)
	}
	.opd : {
		*(.opd)
		*(.opd.*)
	}

	__self_end = .;
}

__lv2toc			= 0x800000000033e720;

_usbRegisterDriver	= 0x80000000000d22d8;
_usbGetDescriptor	= 0x80000000000d2998;
_usbOpenEndpoint	= 0x80000000000d29c4;
_usbControlTransfer	= 0x80000000000d292c;
_printf				= 0x800000000028a654;
