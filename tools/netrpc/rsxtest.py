#!/usr/bin/python

import sys, time
import random, math
from netrpc import *
from rsxutil import *

ps3host = os.environ.get("PS3HOST", "ps3")
p = LV1Client(ps3host)

try:
	p.lv1_gpu_close()
except LV1Error:
	pass

p.lv1_gpu_open(0)
p.clr_mmio()

mem_handle, ddr_lpar = p.lv1_gpu_memory_allocate(254<<20, 0, 0, 0, 0);
p.add_mmio(ddr_lpar, 254<<20)

ctrl_size = 65536
ctx_handle, ctrl_lpar, drv_lpar, rep_lpar, rep_size = p.lv1_gpu_context_allocate(mem_handle, 0)
print "CH1"
ctx_handle, ctrl_lpar, drv_lpar, rep_lpar, rep_size = p.lv1_gpu_context_allocate(mem_handle, 0)
print "CH2"
ctx_handle, ctrl_lpar, drv_lpar, rep_lpar, rep_size = p.lv1_gpu_context_allocate(mem_handle, 0)
print "CH3"
ctx_handle, ctrl_lpar, drv_lpar, rep_lpar, rep_size = p.lv1_gpu_context_allocate(mem_handle, 0)
print "CH4"

p.add_mmio(ctrl_lpar, ctrl_size)

print "Memory handle: 0x%x"%mem_handle
print "Context handle: 0x%x"%ctx_handle

xdr_lpar = None
try:
	xdr_ioif = 0x0c000000
	xdr_size = 1024*1024*16
	xdr_lpar, muid = p.lv1_allocate_memory(xdr_size, 21, 0, 0)

	fifo_size = 1024*1024*2
	fifo_ioif = xdr_ioif
	fifo_lpar = xdr_lpar

	xfb_size = xdr_size - fifo_size
	xfb_ioif = xdr_ioif + fifo_size
	xfb_lpar = xdr_lpar + fifo_size

	p.lv1_gpu_context_iomap(ctx_handle, xdr_ioif, xdr_lpar, xdr_size, \
		CBE_IOPTE_PP_W | CBE_IOPTE_PP_R | CBE_IOPTE_M)

	p.lv1_gpu_fb_setup(ctx_handle, fifo_lpar, fifo_size, fifo_ioif)

	fifo = RSXFIFO(p, ctx_handle, ctrl_lpar, fifo_lpar, fifo_ioif, fifo_size)
	fifo.show()

	off = 0
	fb = RSXFB(p, off, ddr_lpar, 800, 600)
	off += fb.size
	off = (off+0xff)&~0xff
	fragprog_buf = RSXBuffer(p, off, ddr_lpar, 0x1000)

	print "Clearing FB..."
	p.memset32(ddr_lpar, ~0xdeadbeef, fb.size)
	print "Testing FIFO..."

	for i in range(0x1000):
		fifo.nop()

	fifo.fire()
	fifo.wait()
	fifo.show()
	print "FIFO OK!"

	print "Initializing 3D..."

	render = RSX3D(fifo, fb, fragprog_buf)
	render.tcl_init()

	fifo.fire()
	fifo.wait()
	fifo.show()

	print "Rendering some random triangles..."

	render.begin_triangles()

	vertices = [
		(-0.5, -0.433),
		(0.5, -0.433),
		(0, 0.433),
	]

	for i in range(40):
		cx = random.randrange(64, 512-64)
		cy = random.randrange(64, 512-64)
		cr = random.uniform(0, 2*math.pi)
		size = 64

		for x,y in vertices:
			xp = x*math.cos(cr) - y*math.sin(cr)
			yp = x*math.sin(cr) + y*math.cos(cr)
			render.vertex3(xp*size + cx, yp*size + cy, 0)

	render.end()

	fifo.fire()
	fifo.wait()
	fifo.show()
	print "Reading FB..."

	#p.lv1_gpu_fb_blit(ctx_handle, 0, fifo_ioif, (fb.w<<16)|fb.h, fb.pitch)

	fb.getimg().show()

finally:
	p.lv1_gpu_close()
	if xdr_lpar is not None:
		p.lv1_release_memory(xdr_lpar)

