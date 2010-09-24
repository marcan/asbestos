/*  main.c - AsbestOS Stage2 main C entrypoint

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "lv1call.h"
#include "debug.h"
#include "malloc.h"

int main(void) {
	debug_init();
	printf("\n\nAsbestOS Stage 2 starting...\n");
	lv1_panic(1);
	return 0;
}
