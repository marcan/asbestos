#!/usr/bin/python

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

