#  Stage1 Makefile
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

include ../common/ppu.mak
LDFLAGS += -Wl,-T,stage1.ld
CFLAGS += -O2 -Wall

OBJS = start.o thunks.o usbdev.o

all: stage1.bin

%.bin: %.elf
	$(PREFIX)objcopy -O binary $< $@

stage1.elf: $(OBJS) stage1.ld
	$(PREFIX)gcc $(LDFLAGS) -o $@ $(OBJS)

%.o: %.S
	$(PREFIX)gcc $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(PREFIX)gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.elf *.bin
