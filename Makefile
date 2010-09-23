#  Master makefile
#
# Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

all:
	make -C stage1
	make -C stage2
	make -C tools

clean:
	make -C stage1 clean
	make -C stage2 clean
	make -C tools clean

