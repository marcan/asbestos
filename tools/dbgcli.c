/*  dbgcli.c - printf over UDP debugging client

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#define BUFLEN 2048
#define DEBUG_PORT 18194

char buf[BUFLEN];

int main(void)
{
	struct sockaddr_in si_me, si_other;
	int fd;
	int len;
	socklen_t slen = sizeof(si_other);
	int one = 1;

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == -1) {
		perror("socket");
		return 1;
	}
	
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) == -1) {
		perror("setsockopt(SO_BROADCAST)");
		return 1;
	}

	memset((char *) &si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(DEBUG_PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(fd, (struct sockaddr *)&si_me, sizeof(si_me)) == -1) {
		perror("bind");
		return 1;
	}

	while (1) {
		len = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&si_other, &slen);
		if (len == -1) {
			perror("recvfrom");
			return 1;
		}
		buf[len] = 0;
		fputs(buf, stdout);
		fflush(stdout);
	}
}

