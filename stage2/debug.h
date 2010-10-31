/*  debug.h - printf message logging via Lv-1 Ethernet

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef DEBUG_H
#define DEBUG_H

void debug_init(void);
void debug_shutdown(void);
int printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

void fatal(const char *msg) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

#endif
