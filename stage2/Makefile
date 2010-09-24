#  Stage2 Makefile
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

include ../common/ppu.mak
LDFLAGS += -Wl,-T,stage2.ld
CFLAGS += -O2 -Wall -Ilwip/src/include -Ilwip/arch/include

DEPDIR = .deps

OBJS = start.o main.o debug.o hvcall.o string.o printf.o device.o malloc.o \
	time.o

all: stage2.bin

%.bin: %.elf
	@echo "  OBJCOPY   $<"
	@$(PREFIX)objcopy -O binary $< $@

stage2.elf: $(OBJS) stage2.ld
	@echo "  LINK      $<"
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
	rm -f *.o *.elf *.bin

-include $(DEPDIR)/*

.PHONY: clean
