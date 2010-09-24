/*  stub_main.c - LZO decompressor stub main

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "minilzo.h"

extern u8 __stage2_base[];
extern u8 __lzodata[];

struct lzo_hdr {
	u32 dlen;
	u32 clen;
};

extern struct lzo_hdr __lzohdr;

void panic(void);

int main(void)
{
	lzo_uint dlen = __lzohdr.dlen;
	lzo_uint clen = __lzohdr.clen;

	lzo1x_decompress(__lzodata, clen, __stage2_base, &dlen, 0);

	u8 *dst = __stage2_base;

	s32 len = dlen;
	while (len > 0) {
		asm ("dcbst 0, %0; sync; icbi 0, %0" :: "r"(dst));
		dst += 32;
		len -= 32;
	}

	asm ("bl __stage2_base; b panic");
	return 0;
}