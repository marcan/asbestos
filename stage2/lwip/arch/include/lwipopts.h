#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define MEM_LIBC_MALLOC 1

#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define LWIP_CHKSUM_ALGORITHM 2

#define LWIP_DHCP 1
#define LWIP_ARP 1
#define LWIP_TCP 0
#define LWIP_NETIF_HOSTNAME 1

#define ETH_PAD_SIZE 2

#endif
