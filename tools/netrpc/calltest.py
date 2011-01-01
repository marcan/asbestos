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

c_mul = """
	u64 main(u64 a, u64 b) {
		return a * b;
	}
"""
multiply = CCode(p, c_mul)

print "5*3 = ", multiply(5, 3)
print "42*1337 = ", multiply(42, 1337)

