/*  types.h - standard type definitions

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef TYPES_H
#define TYPES_H

#define NULL ((void *)0)

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

#endif
