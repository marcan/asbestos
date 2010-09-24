/*  tftp.h - simple TFTP client

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef TFTP_H
#define TFTP_H

#include "types.h"
#include "lwip/udp.h"

// opaque type representing a TFTP client
struct tftp_client;

enum tftp_status {
	TFTP_STATUS_OK = 0,
	TFTP_STATUS_OVERFLOW,
	TFTP_STATUS_ERROR,
};

typedef void (*tftp_cb_t)(void *arg, struct tftp_client *clt, enum tftp_status status, size_t recvd);

struct tftp_client *tftp_new(void);
err_t tftp_connect(struct tftp_client *clt, struct ip_addr *addr, u16_t port);
err_t tftp_get(struct tftp_client *clt, char *name, void *buffer, size_t max_size, tftp_cb_t cb, void *arg);
void tftp_remove(struct tftp_client *clt);

#endif