/*  lzocomp.c - simple LZO1X-999 compressor

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <lzo/lzo1x.h>

void put32(FILE *f, uint32_t v)
{
	uint8_t buf[4] = {v>>24, v>>16, v>>8, v};
	if (fwrite(buf, 4, 1, f) != 1) {
		perror("fwrite");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
		return 1;
	}

	FILE *fin, *fout;
	void *ibuf, *obuf, *wmem;
	lzo_uint clen, dlen;

	fin = fopen(argv[1],"rb");
	if (!fin) {
		perror("fopen");
		return 1;
	}
	fout = fopen(argv[2],"wb");
	if (!fout) {
		perror("fopen");
		return 1;
	}

	if(lzo_init() < 0) {
		fprintf(stderr, "Could not initialize LZO\n");
		return 1;
	}

	fseek(fin, 0, SEEK_END);
	dlen = ftell(fin);
	fseek(fin, 0, SEEK_SET);

	printf("Compressing %ld bytes...\n", dlen);

	ibuf = malloc(dlen);
	obuf = malloc(2*dlen);
	wmem = malloc(LZO1X_999_MEM_COMPRESS);

	if (fread(ibuf, 1, dlen, fin) != dlen) {
		perror("fread");
		return 1;
	}
	fclose(fin);
	
	if (lzo1x_999_compress(ibuf, dlen, obuf, &clen, wmem) < 0) {
		fprintf(stderr, "Failed to compress\n");
		return 1;
	}

	printf("Compressed size: %ld bytes (%ld%%)\n", clen, 100*clen/dlen);

	put32(fout, dlen);
	put32(fout, clen);

	if (fwrite(obuf, 1, clen, fout) != clen) {
		perror("fwrite");
		return 1;
	}
	fclose(fout);

	return 0;
}