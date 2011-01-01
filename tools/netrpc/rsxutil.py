#!/usr/bin/python
#
#  netrpc - use your PS3 as a slave for Python code
#
# Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

import struct
from PIL import Image
from rsxconst import *
from netrpc import chexdump

CTRL_PUT = 0x40
CTRL_GET = 0x44
CTRL_TOP = 0x54

DMA_NOTIFIER_HANDLE_BASE = 0x66604200
DMA_NOTIFIER_OFFSET_BASE = 0x1000
DMA_NOTIFIER_SIZE        = 0x40
NOTIFIER = 7

UPLOAD_SUBCH = 1
DOWNLOAD_SUBCH = 2

RENDER_SUBCH = 7

OBJ_NULL = 0x00000000
OBJ_SCALED_IMAGE_FROM_MEMORY = 0x3137af00
OBJ_SWIZZLED_SURFACE = 0x31337a73
OBJ_IMAGE_FROM_CPU = 0x31337808
OBJ_DDR_TO_XDR = 0x3137c0de
OBJ_CTX_SURFACE_2D = 0x313371c3
OBJ_XDR_TO_DDR = 0x3137730
OBJ_3D = 0x31337000

DMA_DDR = 0xfeed0000
DMA_XDR = 0xfeed0001

class RSXFIFO(object):
	def __init__(self, p, ctx, regs, lpar_addr, ioif_addr, size):
		self.p = p
		self.ctx_handle = ctx
		self.regs = regs
		self.lpar_addr = lpar_addr
		self.ioif_addr = ioif_addr
		self.size = size
		self.buf = ""
		self.init()

	def _rget(self):
		return self.p.read32(self.regs + CTRL_GET)
	def _wget(self, data):
		self.p.write32(self.regs + CTRL_GET, data)
	_get = property(_rget, _wget)
	def _rput(self):
		return self.p.read32(self.regs + CTRL_PUT)
	def _wput(self, data):
		self.p.write32(self.regs + CTRL_PUT, data)
	_put = property(_rput, _wput)
	def _rtop(self):
		return self.p.read32(self.regs + CTRL_TOP)
	def _wtop(self, data):
		self.p.write32(self.regs + CTRL_TOP, data)
	_top = property(_rtop, _wtop)

	def init(self):
		self.offset = 0
		self.p.lv1_gpu_fifo_init(self.ctx_handle, self.ioif_addr, self.ioif_addr, self.ioif_addr)

	def wait(self, timeout=100):
		for i in range(timeout):
			if self._get == self._put:
				break
		else:
			raise Exception("FIFO Timeout %08x %08x"%(self._put,self._get))

	def rewind(self):
		self.p.write32(self.lpar_addr + self.offset, 0x20000000 | self.ioif_addr)
		self._put = self.ioif_addr
		self.offset = 0

	def out(self, data):
		if isinstance(data, float):
			self.buf += struct.pack(">f", float(data))
		else:
			self.buf += struct.pack(">I", data)

	def begin(self, chan, tag, size):
		self.out((size<<18)|(chan<<13)|tag)

	def fire(self):
		#chexdump(self.buf)
		fifo_left = self.size - self.offset
		assert 0 <= fifo_left <= self.size

		need = len(self.buf) + 4 # space for a rewind, if needed

		print "Pushing %d words (0x%x bytes) to FIFO (current offset: %08x)"%(len(self.buf)/4, len(self.buf), self.offset)

		if need > self.size:
			raise Exception("Buffer too full for FIFO")
		if need > fifo_left:
			print "Rewinding FIFO (at %08x, have %08x, need %08x)...\n"%(put,fifo_left,need)
			self.wait()
			self.rewind()

		self.p.writemem(self.lpar_addr + self.offset, self.buf)
		self.offset += len(self.buf)
		assert self.offset < self.size
		self._put = self.ioif_addr + self.offset
		self.buf = ""

	def resync(self):
		get = self._get
		assert self.ioif_addr <= get <= (self.ioif_addr+self.size)
		self.offset = get - self.ioif_addr

	def show(self):
		print "FIFO PUT: %08x"%self._put
		print "FIFO GET: %08x"%self._get
		print "FIFO TOP: %08x"%self._top

	def setsubchannel(self, subc, obj):
		self.begin(subc, 0, 1)
		self.out(obj)

	def nop(self):
		self.out(0)

class RSXBuffer(object):
	def __init__(self, p, ddr_offset, lpar_base, size):
		self.p = p
		self.obj = DMA_DDR
		self.offset = ddr_offset
		self.lpar = lpar_base + ddr_offset
		self.size = size
	def read(self):
		return self.p.readmem(self.lpar, self.size)
	def write(self, d):
		if len(d) > self.size:
			raise Exception("Buffer overflow")
		self.p.writemem(self.lpar, d)

class RSXFB(RSXBuffer):
	def __init__(self, p, ddr_offset, lpar_addr, width, height):
		self.bpp = 4
		self.w = width
		self.h = height
		self.pitch = self.w * self.bpp
		RSXBuffer.__init__(self, p, ddr_offset, lpar_addr, self.pitch * self.h)

	def getimg(self):
		return Image.frombuffer("RGBX", (self.w,self.h), self.read(), 'raw', "RGBX", 0, 1)

NV40_VP = (
	0x00000309, 0x0000c001,
	[
		# MOV result.position, vertex.position
		0x40041c6c, 0x0040000d, 0x8106c083, 0x6041ff80,
		# MOV result.texcoord[0], vertex.texcoord[0]
		#0x401f9c6c, 0x0040080d, 0x8106c083, 0x6041ff9c,
		# MOV result.texcoord[1], vertex.texcoord[1]
		#0x401f9c6c, 0x0040090d, 0x8106c083, 0x6041ffa1,
	]
)

NV30_FP = (
	2,
	[
		# MOV R0, ( 0.5f, 0.5f, 0.5f, 0.5f )
		0x01403e81, 0x1c9dc802, 0x0001c800, 0x3fe1c800,
		0x3f000000, 0x3f000000, 0x3f000000, 0x3f000000,
	]
)


class RSX3D(object):
	def __init__(self, fifo, fb, fp_buf):
		self.fifo = fifo
		self.sch = RENDER_SUBCH
		self.fb = fb
		self.fp_buf = fp_buf

	def put(self, reg, *data):
		if len(data) == 1 and (isinstance(data[0], list) or isinstance(data[0], tuple)):
			data = data[0]
		self.fifo.begin(self.sch, reg, len(data))
		for d in data:
			self.fifo.out(d)

	def tcl_init(self):
		self.fifo.setsubchannel(self.sch, OBJ_3D)
		self.put(NV40TCL_DMA_NOTIFY, DMA_NOTIFIER_HANDLE_BASE)
		self.put(NV40TCL_DMA_TEXTURE0, self.fb.obj)
		self.put(NV40TCL_DMA_COLOR0, self.fb.obj, self.fb.obj)
		self.put(NV40TCL_DMA_ZETA, self.fb.obj)

		self.put(0x1ea4, 0x00000010, 0x01000100, 0xff800006)
		self.put(0x1fc4, 0x06144321)
		self.put(0x1fc8, 0xedcba987)
		self.put(0x1fd0, 0x00171615)
		self.put(0x1fd4, 0x001b1a19)

		self.put(0x1ef8, 0x0020ffff)
		self.put(0x1d64, 0x00d30000)
		self.put(0x1e94, 0x00000001)

		self.put(NV40TCL_VIEWPORT_TRANSLATE_X,
			0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0)

		self.put(NV40TCL_STENCIL_FRONT_ENABLE, 0)
		self.put(NV40TCL_STENCIL_BACK_ENABLE, 0)
		self.put(NV40TCL_ALPHA_TEST_ENABLE, 0)
		self.put(NV40TCL_DEPTH_WRITE_ENABLE, 0)
		self.put(NV40TCL_DEPTH_TEST_ENABLE, 0)
		self.put(NV40TCL_DEPTH_FUNC, NV40TCL_DEPTH_FUNC_LESS)
		self.put(NV40TCL_COLOR_MASK, 0x01010101)
		self.put(NV40TCL_CULL_FACE_ENABLE, 0)
		self.put(NV40TCL_BLEND_ENABLE, 0)
		self.put(NV40TCL_COLOR_LOGIC_OP_ENABLE, 0, NV40TCL_COLOR_LOGIC_OP_COPY)
		self.put(NV40TCL_DITHER_ENABLE, 0)
		self.put(NV40TCL_SHADE_MODEL, NV40TCL_SHADE_MODEL_SMOOTH)
		self.put(NV40TCL_POLYGON_OFFSET_FACTOR, 0.0, 0.0)
		self.put(NV40TCL_POLYGON_MODE_FRONT, NV40TCL_POLYGON_MODE_FRONT_FILL, NV40TCL_POLYGON_MODE_BACK_FILL)

		self.put(NV40TCL_POLYGON_STIPPLE_PATTERN(0), [0xffffffff]*0x20)

		for i in range(16):
			self.put(NV40TCL_TEX_ENABLE(i), 0)

		self.put(0x1d78, 0x110)

		self.put(NV40TCL_RT_ENABLE, NV40TCL_RT_ENABLE_COLOR0)
		self.put(NV40TCL_RT_HORIZ, (512 << 16), (512 << 16))
		self.put(NV40TCL_SCISSOR_HORIZ, (512 << 16), (512 << 16))
		self.put(NV40TCL_VIEWPORT_HORIZ, (512 << 16), (512 << 16))
		self.put(NV40TCL_VIEWPORT_CLIP_HORIZ(0), (512 << 16), (512 << 16))

		self.put(NV40TCL_ZETA_OFFSET, 512*4)
		self.put(NV40TCL_ZETA_PITCH, self.fb.pitch)

		self.put(NV40TCL_RT_FORMAT,
			NV40TCL_RT_FORMAT_TYPE_LINEAR | NV40TCL_RT_FORMAT_ZETA_Z16 | NV40TCL_RT_FORMAT_COLOR_A8R8G8B8,
			self.fb.pitch, 0)

		self.put(NV40TCL_CLEAR_VALUE_COLOR, 0xdeadbeef)
		self.put(NV40TCL_CLEAR_VALUE_DEPTH, 0xffff)

		self.put(NV40TCL_CLEAR_BUFFERS,
			NV40TCL_CLEAR_BUFFERS_COLOR_B | NV40TCL_CLEAR_BUFFERS_COLOR_G |
			NV40TCL_CLEAR_BUFFERS_COLOR_R | NV40TCL_CLEAR_BUFFERS_COLOR_A |
			NV40TCL_CLEAR_BUFFERS_STENCIL | NV40TCL_CLEAR_BUFFERS_DEPTH)

		self.load_vtx_prog(NV40_VP)
		self.load_frag_prog(NV30_FP)

		#ptr += NV40_LoadTex( ptr, (uint8_t *)fbmem );
		#ptr += NV40_LoadVtxProg( ptr,  &nv40_vp );
		#ptr += NV40_LoadFragProg( ptr, fbmem,  &nv30_fp );
		#ptr += NV40_EmitGeometry( ptr );

	def load_vtx_prog(self, prog):
		in_reg, out_reg, data = prog

		self.put(NV40TCL_VP_UPLOAD_FROM_ID, 0)
		inst = 0
		for i in range(0, len(data), 4):
			w = data[i:i+4]
			self.put(NV40TCL_VP_UPLOAD_INST(0), data[i:i+4])
			inst += 1

		print "Uploaded %d vertex program lines"%inst

		self.put(NV40TCL_VP_START_FROM_ID, 0)
		self.put(NV40TCL_VP_ATTRIB_EN, in_reg, out_reg)

		self.put(0x1478, 0)

	def load_frag_prog(self, prog):
		numregs, data = prog
		buf = ""
		for word in data:
			lo = word&0xFFFF
			hi = word>>16
			buf += struct.pack(">HH", lo, hi) # swab halfwords

		self.fp_buf.write(buf)
		print "Uploaded %d fragment program lines"%(len(buf)/16)

		self.put(NV40TCL_FP_ADDRESS, self.fp_buf.offset | NV40TCL_FP_ADDRESS_DMA0)
		self.put(NV40TCL_FP_CONTROL, numregs<<NV40TCL_FP_CONTROL_TEMP_COUNT_SHIFT)

	def begin_triangles(self):
		self.put(NV40TCL_BEGIN_END, NV40TCL_BEGIN_END_TRIANGLES)

	def vertex3(self, x, y, z):
		self.put(NV40TCL_VTX_ATTR_4F_X(0), float(x), float(y), float(z), 1.0)

	def end(self):
		self.put(NV40TCL_BEGIN_END, NV40TCL_BEGIN_END_STOP)
