#  Stage2 Makefile
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

include ../common/ppu.mak
LDFLAGS += -Wl,-T,stage2.ld
CFLAGS += -O2 -Wall -I. \
	-Ilwip/src/include -Ilwip/src/include/ipv4 -Ilwip/arch/include

DEPDIR = .deps

LWIP_OBJS = \
	$(addprefix lwip/src/core/,\
		dhcp.o init.o mem.o memp.o netif.o pbuf.o raw.o stats.o sys.o \
		tcp.o tcp_in.o tcp_out.o udp.o) \
	$(addprefix lwip/src/core/ipv4/,\
		autoip.o icmp.o ip.o ip_addr.o ip_frag.o inet_chksum.o) \
	$(addprefix lwip/src/api/,\
		tcpip.o err.o netifapi.o api_lib.o api_msg.o sockets.o) \
	lwip/src/netif/etharp.o

OBJS = start.o main.o debug.o hvcall.o string.o printf.o device.o malloc.o \
	time.o gelic_netif.o $(LWIP_OBJS)

all: stage2.bin

%.bin: %.elf
	@echo "  OBJCOPY   $@"
	@$(PREFIX)objcopy -O binary $< $@

stage2.elf: $(OBJS) stage2.ld
	@echo "  LINK      $@"
	@$(PREFIX)gcc $(LDFLAGS) -o $@ $(OBJS)

%.o: %.S
	@echo "  ASSEMBLE  $<"
	@mkdir -p $(DEPDIR)
	@$(PREFIX)gcc $(CFLAGS) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c -o $@ $<

%.o: %.c
	@echo "  COMPILE   $<"
	@mkdir -p $(DEPDIR)
	@$(PREFIX)gcc $(CFLAGS) -Wp,-MMD,$(DEPDIR)/$(*F).d,-MQ,"$@",-MP -c -o $@ $<

clean:
	rm -rf $(DEPDIR)
	rm -f $(OBJS) *.elf *.bin

-include $(DEPDIR)/*

.PHONY: clean
