/*  c_head.h - C header for netrpc compiled code

Copyright (C) 2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <stddef.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long s64;

typedef unsigned long size_t;
typedef signed long ssize_t;

#define ALIGNED(x) __attribute__((aligned(x)))

static inline u64 read64(u64 addr)
{
	u64 val;
	asm volatile("ld %0,0(%1) ; sync" : "=r"(val) : "b"(addr));
	return val;
}

static inline void write64(u64 addr, u64 val)
{
	asm("std %0,0(%1) ; eieio" : : "r"(val), "b"(addr));
}

static inline u32 read32(u64 addr)
{
	u32 val;
	asm volatile("lwz %0,0(%1) ; sync" : "=r"(val) : "b"(addr));
	return val;
}

static inline void write32(u64 addr, u32 val)
{
	asm("stw %0,0(%1) ; eieio" : : "r"(val), "b"(addr));
}

static inline u16 read16(u64 addr)
{
	u16 val;
	asm volatile("lhz %0,0(%1) ; sync" : "=r"(val) : "b"(addr));
	return val;
}

static inline void write16(u64 addr, u16 val)
{
	asm("sth %0,0(%1) ; eieio" : : "r"(val), "b"(addr));
}

static inline u8 read8(u64 addr)
{
	u8 val;
	asm volatile("lbz %0,0(%1) ; sync" : "=r"(val) : "b"(addr));
	return val;
}

static inline void write8(u64 addr, u8 val)
{
	asm("stb %0,0(%1) ; eieio" : : "r"(val), "b"(addr));
}

