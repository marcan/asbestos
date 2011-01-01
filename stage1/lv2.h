/*  lv2.h - Lv-2 defines, thunks, and functions

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef LV2_H
#define LV2_H

#define NULL 0

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long s64;

#define CBTHUNK(ret, name, args) asm("" \
"	.section \".text\"\n" \
"	.align 2\n" \
"	.p2align 3,,7\n" \
"	.globl "#name"\n" \
"	.section \".opd\",\"aw\"\n" \
"	.align 3\n" \
#name": \n" \
"	.quad .L."#name",.TOC.@tocbase\n" \
"	.previous\n" \
"	.type   "#name", @function\n" \
".L."#name":\n" \
"	mflr 0\n" \
"	std 0, 32(1)\n" \
"	std 2, 40(1)\n" \
"	clrrdi 2, 2, 32\n" \
"	oris 2, 2, __toc@h\n" \
"	ori 2, 2, __toc@l\n" \
"	bl .L._"#name"\n" \
"	ld 2, 40(1)\n" \
"	ld 0, 32(1)\n" \
"	mtlr 0\n" \
"	blr\n" \
"	.size "#name",.-.L."#name"\n"); \
ret name args; \
ret _##name args

typedef struct {
	const char *name;
	int (*probe_func)(int device_id);
	int (*attach_func)(int device_id);
	int (*detach_func)(int device_id);
	
} usb_driver_t;

typedef struct {
	u8 bmRequestType;
	u8 bRequest;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
} control_transfer_t;

// lv1 but meh
void panic(void);
void reboot(void);

// lv2 functions
void *usbGetDescriptor(int device_id, void *start, u8 type);
int usbOpenEndpoint(int device_id, void *epdesc);
int usbControlTransfer(int pipe_id, control_transfer_t *request, void *data, void *cb, void *cbarg);
int printf(const char *fmt, ...);

#endif