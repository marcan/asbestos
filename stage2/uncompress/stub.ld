OUTPUT_FORMAT("elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)

/* stub runs in realmode */
__base = 0x100000;
__stage2_base = 0x10000;

ENTRY(_start)

PHDRS {
	stub		PT_LOAD FLAGS(7);
}

SECTIONS {
	. = __base;
	__self_start = .;

	.start : {
		*(.start)
	} :stub = 0

	.text : {
		*(.text)
		*(.text.*)
	}

	. = ALIGN(32);
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

	. = ALIGN(32);
	.data : {
		*(.data)
		*(.data.*)
		*(.sdata)
		*(.sdata.*)
	}
	. = (( . +67)&0xFFFFFFC0) - 4;
	.pad : {
		LONG(0);
	}
	__lzohdr = .;
	. += 8;
	__lzodata = .;

	/* make space for the lzo data */
	. += 0x100000;
	
	__bss_start = .;
	.bss : {
		*(.sbss)
		*(.sbss.*)
		*(COMMON)
		*(.bss)
		*(.bss.*)
	}
	__bss_end = .;

	. = ALIGN(32);

	.stack : {
		_stack_top = .;
		. += 32768;
		_stack_bot = .;
	} :NONE

}
