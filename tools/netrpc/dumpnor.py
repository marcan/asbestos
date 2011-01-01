#!/usr/bin/python
#
#  netrpc - use your PS3 as a slave for Python code
#
# Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

import struct
import sys
from netrpc import *

ps3host = os.environ.get("PS3HOST", "ps3")
p = LV1Client(ps3host)

p.clr_mmio()

bus, dev = 4,1

try:
	p.lv1_close_device(bus, dev)
	print "closed"
except:
	pass

p.lv1_open_device(bus, dev, 0)

def wait(etag):
	while True:
		try:
			tag, status = p.lv1_storage_get_async_status(dev)
			#print "status=%x,tag=%d"%(status,tag)
			if tag == etag:
				return status
		except:
			continue

lpar = 0x100000
lpar_size = 0x100000

busaddr = p.lv1_allocate_device_dma_region(bus, dev, lpar_size, 12, 0)
p.lv1_map_device_dma_region(bus, dev, lpar, busaddr, lpar_size, 0xf800000000000000)

cbuf = lpar
dbuf = lpar + 0x10000

fd = open("nor.bin","wb")
print "Dumping NOR"
for sector in range(0,0x8000,16):
	try:
		tag = p.lv1_storage_read(dev, 0, sector, 16, 0x02, dbuf)
		if wait(tag) == 0:
			sys.stdout.flush()
			data = p.readmem(dbuf, 0x2000)
			print "\rSector 0x%x (%dKiB)"%(sector,(sector+1)/2),
			fd.write(data)
			#chexdump(p.readmem(dbuf, 0x200), 0x200*sector)
	except KeyboardInterrupt:
		raise
	except Exception,e:
		print "Failed (%s)"%e
		break

print
print "nor.bin saved"

p.lv1_close_device(bus, dev)
