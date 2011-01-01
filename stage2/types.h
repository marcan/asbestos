/*  types.h - standard type definitions

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef TYPES_H
#define TYPES_H

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

/* FATFS stuff */

typedef u8 BYTE;
typedef u8 UCHAR;

typedef u16 WORD;
typedef u16 USHORT;

typedef u32 DWORD;
typedef u32 UINT;
typedef u32 ULONG;

typedef s8 CHAR;
typedef s16 SHORT;
typedef u16 WCHAR;
typedef s32 INT;
typedef s32 LONG;

typedef enum { FALSE = 0, TRUE } BOOL;

#endif
