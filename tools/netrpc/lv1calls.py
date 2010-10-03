#!/usr/bin/python

import os

class LV1Call(object):
	def __init__(self, num, name, iargs, oargs):
		self.num = num
		self.name = name
		self.iargs = iargs
		self.oargs = oargs
	def __str__(self):
		args = self.iargs + ["&%s"%s for s in self.oargs]
		astr = ", ".join(args)
		return "<%d>%s(%s)"%(self.num, self.name, astr)

def getcalls():
	calls = []
	callmap = {}

	infopath = os.path.join(os.path.dirname(__file__),"lv1.txt")
	for line in open(infopath):
		line = line.replace("\n","")
		if len(line) == 0:
			continue
		if line[0] == "#":
			continue

		d = line.split(None,4)
		if not 4 <= len(d) <= 5:
			raise Exception("Bad entry %s\n"%d)

		num, name, numin, numout = d[:4]
		num = int(num)
		numin = int(numin)
		numout = int(numout)
		if len(d) == 4:
			iargs = ["in_%d"%i for i in range(1,numin+1)]
			oargs = ["out_%d"%i for i in range(1,numout+1)]
		else:
			args = [s.strip() for s in d[4].split(",")]
			if len(args) != (numin+numout):
				raise Exception("Argument count mismatch for %s (%d)\n"%(name,num))
			iargs = args[:numin]
			oargs = args[numin:]

		c = LV1Call(num, name, iargs, oargs)
		calls.append(c)
		callmap[num] = c

	return calls, callmap

calls, callmap = getcalls()

if __name__ == "__main__":
	print "LV1 call list:"
	for call in calls:
		print call
