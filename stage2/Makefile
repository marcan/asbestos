#  Stage2 Makefile
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

include ../common/ppu.mak

STUB_LDFLAGS := $(LDFLAGS) -Wl,--gc-sections -Wl,-T,uncompress/stub.ld
LDFLAGS += -Wl,--gc-sections -Wl,-T,stage2.ld
STUB_CFLAGS := $(CFLAGS) -Os -Wall -ffunction-sections -I. -Iuncompress/
CFLAGS += -Os -Wall -I. -ffunction-sections \
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
	time.o tftp.o gelic_netif.o $(LWIP_OBJS)

STUB_OBJS = uncompress/stub_start.o uncompress/stub_main.o uncompress/minilzo.o

all: stage2.bin

.PRECIOUS: stage2_raw.bin

stage2.bin : uncompress/stub.bin stage2_raw.lzo
	cat uncompress/stub.bin stage2_raw.lzo > $@

%.lzo: %.bin uncompress/lzocomp
	@echo "  LZOCOMP   $@"
	uncompress/lzocomp $< $@

%.bin: %.elf
	@echo "  OBJCOPY   $@"
	@$(PREFIX)objcopy -O binary $< $@

uncompress/stub.elf: $(STUB_OBJS) uncompress/stub.ld
	@echo "  LINK      $@"
	@$(PREFIX)gcc $(STUB_LDFLAGS) -o $@ $(STUB_OBJS)

stage2_raw.elf: $(OBJS) stage2.ld
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

uncompress/lzocomp : uncompress/lzocomp.c
	@echo "  HOSTCC    $<"
	gcc -o $@ $< -llzo2

clean:
	rm -rf $(DEPDIR)
	rm -f $(OBJS)*.elf *.bin *.lzo
	rm -f $(STUB_OBJS) stub/lzocomp

-include $(DEPDIR)/*

.PHONY: clean
