/*  tftp.c - simple TFTP client

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "debug.h"
#include "malloc.h"
#include "tftp.h"

enum tftp_state_t
{
	STATE_IDLE = 0,
	STATE_REQUESTED,
	STATE_RECEIVING,
	STATE_DONE,
	STATE_ERROR
};

struct tftp_client
{
	struct udp_pcb *pcb;
	struct ip_addr remote;
	enum tftp_state_t state;
	u16_t dport;
	u16_t rport;
	struct pbuf *ackbuf;
	tftp_cb_t cb;
	void *cbarg;
	u16 blocknum;
	u8 *rxbuf;
	u8 *rxp;
	size_t rxbytes;
	size_t bufsize;
};

struct tftp_dhdr {
	u16 opcode;
	u16 block;
} __attribute__((packed));

enum {
	TFTP_RRQ = 1,
	TFTP_WRQ,
	TFTP_DATA,
	TFTP_ACK,
	TFTP_ERROR
};

static err_t tftp_ack(struct tftp_client *clt, u16 blocknum)
{
	struct tftp_dhdr *hdr;
	hdr = clt->ackbuf->payload;
	hdr->opcode = TFTP_ACK;
	hdr->block = blocknum;

	err_t err = udp_send(clt->pcb, clt->ackbuf);
	// reclaim the header space used by udp_send
	pbuf_header(clt->ackbuf, -(clt->ackbuf->len - 4));
	return err;
}

static void tftp_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p, struct ip_addr *ipaddr, u16_t port)
{
	err_t err;
	struct tftp_client *clt = arg;

	if (clt->state != STATE_REQUESTED && clt->state != STATE_RECEIVING) {
		printf("TFTP: ignoring UDP packet while TFTP in state %d\n", clt->state);
		pbuf_free(p);
		return;
	}
	if (!ip_addr_cmp(ipaddr, &clt->remote)) {
		printf("TFTP: ignoring UDP packet from wrong remote\n");
		pbuf_free(p);
		return;
	}

	if (clt->state == STATE_REQUESTED) {
		printf("TFTP: received first packet, remote port is %d\n", port);
		clt->rport = port;
		udp_connect(clt->pcb, ipaddr, port);
		clt->state = STATE_RECEIVING;
	} else if (port != clt->rport) {
		printf("TFTP: ignoring UDP packet from wrong port %d\n", port);
		pbuf_free(p);
		return;
	}

	if (p->tot_len<4) {
		printf("TFTP: ignoring short packet (%d)\n", p->tot_len);
		pbuf_free(p);
		return;
	}

	struct tftp_dhdr thdr;
	pbuf_copy_partial(p, &thdr, 4, 0);
	if (thdr.opcode == TFTP_ERROR) {
		printf("TFTP: received error packet (code %d)\n", thdr.block);
		clt->cb(clt->cbarg, clt, TFTP_STATUS_ERROR, clt->rxbytes);
		clt->state = STATE_ERROR;
		udp_disconnect(clt->pcb);
		pbuf_free(p);
		return;
	} else if (thdr.opcode != TFTP_DATA) {
		printf("TFTP: unexpected opcode %d\n", thdr.opcode);
		pbuf_free(p);
		return;
	}
	//printf("TFTP: received data packet op %d block %d len %d\n", thdr.opcode, thdr.block, p->tot_len);

	if (clt->blocknum != thdr.block) {

		u16 last = clt->blocknum-1;
		if (last == thdr.block) {
			printf("TFTP: received duplicate block, ACKing and discarding\n");
			err = tftp_ack(clt, last);
			if(err != ERR_OK) {
				printf("ACK failed (%d)\n", err);
				clt->cb(clt->cbarg, clt, TFTP_STATUS_ERROR, clt->rxbytes);
				clt->state = STATE_ERROR;
				udp_disconnect(clt->pcb);
				pbuf_free(p);
				return;
			}
		} else {
			printf("TFTP: received unexpected block %d\n", thdr.block);
		}
		pbuf_free(p);
		return;
	}

	size_t len = p->tot_len-4;

	if (len > (clt->bufsize - clt->rxbytes))
		len = (clt->bufsize - clt->rxbytes);

	pbuf_copy_partial(p, clt->rxp, len, 4);

	clt->rxbytes += len;
	clt->rxp += len;

	err = tftp_ack(clt, clt->blocknum);
	if(err != ERR_OK) {
		printf("ACK failed (%d)\n", err);
		clt->cb(clt->cbarg, clt, TFTP_STATUS_ERROR, clt->rxbytes);
		clt->state = STATE_ERROR;
		udp_disconnect(clt->pcb);
		pbuf_free(p);
		return;
	}
	clt->blocknum++;

	if (clt->blocknum%1024 == 1) {
		printf("TFTP: downloaded %ldkB so far...\n", clt->rxbytes/1024);
	}

	if (len != (p->tot_len-4)) {
		printf("TFTP: buffer overflow\n");
		clt->cb(clt->cbarg, clt, TFTP_STATUS_OVERFLOW, clt->rxbytes);
		clt->state = STATE_ERROR;
		udp_disconnect(clt->pcb);
	} else if (len < 512) {
		printf("TFTP: download complete (%ld bytes)\n", clt->rxbytes);
		clt->cb(clt->cbarg, clt, TFTP_STATUS_OK, clt->rxbytes);
		clt->state = STATE_IDLE;
		udp_disconnect(clt->pcb);
	}

	pbuf_free(p);
}

struct tftp_client *tftp_new(void)
{
	struct tftp_client *clt;
	clt = malloc(sizeof(struct tftp_client));
	memset(clt, 0, sizeof(struct tftp_client));

	clt->pcb = udp_new();
	if (!clt->pcb) {
		free(clt);
		return NULL;
	}
	udp_recv(clt->pcb, tftp_recv, clt);

	clt->ackbuf = pbuf_alloc(PBUF_TRANSPORT, 4, PBUF_RAM);
	if (!clt->ackbuf) {
		udp_remove(clt->pcb);
		free(clt);
		return NULL;
	}
	return clt;
}

err_t tftp_connect(struct tftp_client *clt, struct ip_addr *ipaddr, u16_t port)
{
	ip_addr_set(&clt->remote, ipaddr);
	clt->dport = port;

	return ERR_OK;
}

err_t tftp_get(struct tftp_client *clt, const char *name, void *buffer, size_t max_size, tftp_cb_t cb, void *arg)
{
	err_t err;
	int len;
	printf("TFTP: starting download for file '%s'\n", name);

	len = 4 + strlen(name) + strlen("octet");

	struct pbuf *pb;
	pb = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
	if (!pb)
		return ERR_MEM;

	u8 *p = pb->payload;
	// RRQ (Read Request)
	p[0] = 0;
	p[1] = TFTP_RRQ;
	p += 2;
	len = strlcpy((char*)p, name, 100);
	p += len+1;
	len = strlcpy((char*)p, "octet", 100);

	err = udp_sendto(clt->pcb, pb, &clt->remote, clt->dport);
	if (err)
		return err;

	printf("TFTP: sent TFTP RRQ (%d bytes)\n", pb->len);

	clt->rport = 0;
	clt->cb = cb;
	clt->cbarg = arg;
	clt->blocknum = 1;
	clt->rxbuf = buffer;
	clt->rxp = buffer;
	clt->bufsize = max_size;
	clt->rxbytes = 0;
	clt->state = STATE_REQUESTED;

	return ERR_OK;
}

void tftp_remove(struct tftp_client *clt)
{
	if (clt->ackbuf)
		pbuf_free(clt->ackbuf);
	clt->ackbuf = NULL;
	if (clt->pcb)
		udp_remove(clt->pcb);
	clt->pcb = NULL;
	free(clt);
}
