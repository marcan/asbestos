#!/usr/bin/python
#
#  netrpc - use your PS3 as a slave for Python code
#
# Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

import serial, os, struct, code, traceback, readline
from netrpc import *
import __main__
import __builtin__

saved_display = sys.displayhook

def display(val):
	global saved_display
	if isinstance(val, int) or isinstance(val, long):
		__builtin__._ = val
		print hex(val)
	elif isinstance(val, tuple) or isinstance(val, list):
		l = []
		for i in val:
			if isinstance(i, int) or isinstance(i, long):
				l.append(hex(i))
			else:
				l.append(repr(i))
		if isinstance(val, tuple):
			if len(val) == 1:
				print "(%s,)"%l[0]
			else:
				print "(%s)"%', '.join(l)
		else:
			print "[%s]"%', '.join(l)
		__builtin__._ = val
	else:
		saved_display(val)

sys.displayhook = display

# convenience
h = hex

ps3host = os.environ.get("PS3HOST", "ps3")

proxy = LV1Client(ps3host)

locals = __main__.__dict__

for attr in dir(proxy):
	locals[attr] = getattr(proxy,attr)
del attr

class ConsoleMod(code.InteractiveConsole):
	def showtraceback(self):
		type, value, tb = sys.exc_info()
		self.write(traceback.format_exception_only(type, value)[0])

ConsoleMod(locals).interact("Have fun!")
