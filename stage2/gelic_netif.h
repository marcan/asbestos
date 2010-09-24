#ifndef GELIC_NETIF_H
#define GELIC_NETIF_H

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "netif/etharp.h"

err_t gelicif_init(struct netif *netif);
int gelicif_input(struct netif *netif);


#endif