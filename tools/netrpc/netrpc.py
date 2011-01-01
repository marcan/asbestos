#!/usr/bin/python
#
#  netrpc.py - use your PS3 as a slave for Python code
#
# Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>
#
# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

import os, os.path, sys, socket, struct, lv1calls, tempfile, subprocess

def hexdump(s,sep=" "):
	return sep.join(map(lambda x: "%02x"%ord(x),s))

def ascii(s):
	s2 = ""
	for c in s:
		if ord(c)<0x20 or ord(c)>0x7e:
			s2 += "."
		else:
			s2 += c
	return s2

def pad(s,c,l):
	if len(s)<l:
		s += c * (l-len(s))
	return s

def chexdump(s,st=0):
	for i in range(0,len(s),16):
		print "%08x  %s  %s  |%s|"%(i+st,pad(hexdump(s[i:i+8],' ')," ",23),pad(hexdump(s[i+8:i+16],' ')," ",23),pad(ascii(s[i:i+16])," ",16))

CBE_IOPTE_PP_W = 0x8000000000000000
CBE_IOPTE_PP_R = 0x4000000000000000
CBE_IOPTE_M = 0x2000000000000000
CBE_IOPTE_SO_R = 0x1000000000000000
CBE_IOPTE_SO_RW = 0x1800000000000000
CBE_IOPTE_RPN_Mask = 0x07fffffffffff000
CBE_IOPTE_H = 0x0000000000000800
CBE_IOPTE_IOID_Mask = 0x00000000000007ff

LV1_SUCCESS                     = 0,
LV1_RESOURCE_SHORTAGE           = -2
LV1_NO_PRIVILEGE                = -3
LV1_DENIED_BY_POLICY            = -4
LV1_ACCESS_VIOLATION            = -5
LV1_NO_ENTRY                    = -6
LV1_DUPLICATE_ENTRY             = -7
LV1_TYPE_MISMATCH               = -8
LV1_BUSY                        = -9
LV1_EMPTY                       = -10
LV1_WRONG_STATE                 = -11
LV1_NO_MATCH                    = -13
LV1_ALREADY_CONNECTED           = -14
LV1_UNSUPPORTED_PARAMETER_VALUE = -15
LV1_CONDITION_NOT_SATISFIED     = -16
LV1_ILLEGAL_PARAMETER_VALUE     = -17
LV1_BAD_OPTION                  = -18
LV1_IMPLEMENTATION_LIMITATION   = -19
LV1_NOT_IMPLEMENTED             = -20
LV1_INVALID_CLASS_ID            = -21
LV1_CONSTRAINT_NOT_SATISFIED    = -22
LV1_ALIGNMENT_ERROR             = -23
LV1_HARDWARE_ERROR              = -24
LV1_INVALID_DATA_FORMAT         = -25
LV1_INVALID_OPERATION           = -26
LV1_INTERNAL_ERROR              = -32768

class LV1Error(Exception):
	CODES = {
		0: "LV1_SUCCESS",
		-2: "LV1_RESOURCE_SHORTAGE",
		-3: "LV1_NO_PRIVILEGE",
		-4: "LV1_DENIED_BY_POLICY",
		-5: "LV1_ACCESS_VIOLATION",
		-6: "LV1_NO_ENTRY",
		-7: "LV1_DUPLICATE_ENTRY",
		-8: "LV1_TYPE_MISMATCH",
		-9: "LV1_BUSY",
		-10: "LV1_EMPTY",
		-11: "LV1_WRONG_STATE",
		-13: "LV1_NO_MATCH",
		-14: "LV1_ALREADY_CONNECTED",
		-15: "LV1_UNSUPPORTED_PARAMETER_VALUE",
		-16: "LV1_CONDITION_NOT_SATISFIED",
		-17: "LV1_ILLEGAL_PARAMETER_VALUE",
		-18: "LV1_BAD_OPTION",
		-19: "LV1_IMPLEMENTATION_LIMITATION",
		-20: "LV1_NOT_IMPLEMENTED",
		-21: "LV1_INVALID_CLASS_ID",
		-22: "LV1_CONSTRAINT_NOT_SATISFIED",
		-23: "LV1_ALIGNMENT_ERROR",
		-24: "LV1_HARDWARE_ERROR",
		-25: "LV1_INVALID_DATA_FORMAT",
		-26: "LV1_INVALID_OPERATION",
		-32768: "LV1_INTERNAL_ERROR",
	}
	def __init__(self, code):
		Exception.__init__(self)
		self.code = code
	def __str__(self):
		return "%s (%d)"%(self.CODES.get(self.code, "Unknown"),self.code)

class RPCError(Exception):
	pass

class RPCProtocolError(RPCError):
	pass

class RPCCommandError(RPCError):
	pass

class RPCClient(object):
	RPC_PING = 0
	RPC_READMEM = 1
	RPC_WRITEMEM = 2
	RPC_HVCALL = 3
	RPC_ADDMMIO = 4
	RPC_DELMMIO = 5
	RPC_CLRMMIO = 6
	RPC_MEMSET = 7
	RPC_VECTOR = 8
	RPC_SYNC = 9
	RPC_CALL = 10
	def __init__(self, host, port=1337):
		self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.s.connect((host, port))
		self.s.settimeout(1)
		self.tag = 1

	def rpc(self, cmd, data=""):
		tag = self.tag
		self.tag += 1
		hdr = struct.pack(">II", cmd, tag)
		self.s.send(hdr + data)
		while True:
			r = self.s.recv(1500)
			if len(r) < 16:
				raise RPCProtocolError("Received small packet (%d bytes)"%len(r))
			rcmd, rtag, retcode = struct.unpack(">IIq", r[:16])
			data = r[16:]
			if rcmd != cmd:
				raise RPCProtocolError("Received bad command (expected %d, got %d)"%(cmd,rcmd))
			if rtag != tag:
				print "RPC: Received bad tag (expected %d, got %d)"%(tag,rtag)
				continue
			if len(data) != 0:
				return retcode, data
			else:
				return retcode

	def chkret(self, ret):
		if ret != 0:
			raise RPCCommandError("RPC Error %d"%ret)

	def ping(self):
		self.chkret(self.rpc(self.RPC_PING))

	def readmemblk(self, addr, length):
		if length == 0:
			return ""
		args = struct.pack(">QI", addr, length)
		ret, data = self.rpc(self.RPC_READMEM, args)
		self.chkret(ret)
		if len(data) != length:
			raise RPCProtocolError("Data length error")
		return data

	def writememblk(self, addr, data):
		if len(data) == 0:
			return
		args = struct.pack(">QI", addr, len(data)) + data
		ret = self.rpc(self.RPC_WRITEMEM, args)
		self.chkret(ret)

	def readmem(self, addr, length):
		data = ""
		while length != 0:
			blk = length
			if blk > 1024:
				blk = 1024
			data += self.readmemblk(addr, blk)
			addr += blk
			length -= blk
		return data

	def writemem(self, addr, data):
		while len(data) != 0:
			blk = data[:1024]
			data = data[1024:]
			self.writememblk(addr, blk)
			addr += len(blk)

	def read8(self, addr):
		return struct.unpack(">B", self.readmem(addr, 1))[0]
	def read16(self, addr):
		return struct.unpack(">H", self.readmem(addr, 2))[0]
	def read32(self, addr):
		return struct.unpack(">I", self.readmem(addr, 4))[0]
	def read64(self, addr):
		return struct.unpack(">Q", self.readmem(addr, 8))[0]

	def write8(self, addr, data):
		self.writemem(addr, struct.pack(">B", data))
	def write16(self, addr, data):
		self.writemem(addr, struct.pack(">H", data))
	def write32(self, addr, data):
		self.writemem(addr, struct.pack(">I", data))
	def write64(self, addr, data):
		self.writemem(addr, struct.pack(">Q", data))

	def memset(self, addr, val, length):
		if length == 0:
			return
		args = struct.pack(">QIQ", addr, length, val)
		ret = self.rpc(self.RPC_MEMSET, args)
		self.chkret(ret)

	def memset8(self, addr, val, length):
		val &= 0xff
		self.memset(addr, val*0x0101010101010101, length)
	def memset16(self, addr, val, length):
		val &= 0xffff
		if (addr|length)&1:
			raise RPCError("Unaligned arguments")
		self.memset(addr, val*0x0001000100010001, length)
	def memset32(self, addr, val, length):
		val &= 0xffffffff
		if (addr|length)&3:
			raise RPCError("Unaligned arguments")
		self.memset(addr, val|(val<<32), length)
	def memset64(self, addr, val, length):
		val &= 0xffffffffffffffff
		if (addr|length)&7:
			raise RPCError("Unaligned arguments")
		self.memset(addr, val, length)

	def lv1ret(self, ret):
		if ret != 0:
			raise LV1Error(ret)

	def hvcall(self, call, numout, *args):
		numin = len(args)
		if numin > 8:
			raise ValueError("Too many HV in args")
		if numout > 7:
			raise ValueError("Too many HV out args")
		data = struct.pack(">IIQ%dQ"%numin, numin, numout, call, *args)
		r = self.rpc(self.RPC_HVCALL, data)
		if not isinstance(r, tuple):
			ret = r
			outvals = None
		else:
			ret, data = r
			outvals = struct.unpack(">%dQ"%numout, data)
			if len(outvals) == 1:
				outvals = outvals[0]
		self.lv1ret(ret)
		return outvals

	def add_mmio(self, start, size):
		args = struct.pack(">QI", start, size)
		ret = self.rpc(self.RPC_ADDMMIO, args)
		self.chkret(ret)

	def del_mmio(self, start):
		args = struct.pack(">Q", start)
		ret = self.rpc(self.RPC_DELMMIO, args)
		self.chkret(ret)

	def clr_mmio(self):
		ret = self.rpc(self.RPC_CLRMMIO)
		self.chkret(ret)

	def vector(self, vec0, vec1, copy_to=0, copy_from=0, copy_size=0):
		args = struct.pack(">QQQQI", vec0, vec1, copy_to, copy_from, copy_size)
		ret = self.rpc(self.RPC_VECTOR, args)
		self.chkret(ret)

	def sync_before_exec(self, addr, size):
		args = struct.pack(">QI", addr, size)
		ret = self.rpc(self.RPC_SYNC, args)
		self.chkret(ret)

	def call(self, descr_addr, *args):
		args = list(args) + [0] * (8-len(args))
		args = struct.pack(">Q8Q", descr_addr, *args)
		return self.rpc(self.RPC_CALL, args)

L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC = 0x101
L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP = 0x102

L1GPU_CONTEXT_ATTRIBUTE_FIFO_INIT = 0x001
L1GPU_CONTEXT_ATTRIBUTE_FB_SETUP = 0x600
L1GPU_CONTEXT_ATTRIBUTE_FB_BLIT = 0x601
L1GPU_CONTEXT_ATTRIBUTE_FB_BLIT_SYNC = 0x602
L1GPU_CONTEXT_ATTRIBUTE_FB_CLOSE = 0x603

L1GPU_FB_BLIT_WAIT_FOR_COMPLETION = (1 << 32)

L1GPU_DISPLAY_SYNC_HSYNC = 1
L1GPU_DISPLAY_SYNC_VSYNC = 2

class LV1Client(RPCClient):
	def __init__(self, host, port=1337):
		RPCClient.__init__(self, host, port)

		for c in lv1calls.calls:
			# voodoo ahead
			def makethunk(call):
				icnt = len(call.iargs)
				ocnt = len(call.oargs)
				def thunk(self, *args):
					if len(args) != icnt:
						raise TypeError("%s() takes exactly %d arguments (%d given)"%(call.name, icnt+1, len(args)+1))
					return self.hvcall(call.num, ocnt, *args)
				return thunk
			self.__dict__[c.name] = makethunk(c).__get__(self, LV1Client)

	def parse_repo_path(self, path):
		spath = path.split(".")
		if len(spath) < 2:
			raise ValueError("Repo path '%s' is too short", path)
		if spath[0] == "pme":
			lpar_id = 1
		elif spath[0] == "cur":
			lpar_id = self.lv1_get_logical_partition_id()
		else:
			try:
				lpar_id = int(spath[0])
			except ValueError:
				raise ValueError("Unknown LPAR ID '%s'"%spath[0])
		spath = spath[1:]
		vid = 0
		if spath[0] == "sony":
			spath = spath[1:]
			vid = 0x8000000000000000
		if len(spath) < 1:
			raise ValueError("Repo path '%s' is too short"%path)
		if len(spath) > 4:
			raise ValueError("Repo path '%s' is too long"%path)
		key = [lpar_id]
		for i, sk in enumerate(spath):
			num = 0
			if '#' in sk:
				sk, num = sk.split('#')
				num = int(num)
			if i == 0 and len(sk) > 4 or len(sk) > 8:
				raise ValueError("Repo path component '%s' is too long"%sk)
			if i == 0 and len(sk) == 0:
				raise ValueError("First path component is required not to be empty")
			sk = struct.unpack(">Q", (sk+("\x00"*8))[:8])[0]
			if i == 0:
				sk = vid + (sk>>32)
			sk += num
			key.append(sk)
		if len(key) < 5:
			key += [0]*(5-len(key))
		return tuple(key)

	def read_repo(self, path):
		lpar, k1, k2, k3, k4 = self.parse_repo_path(path)
		return self.lv1_get_repository_node_value(lpar, k1, k2, k3, k4)

	def get_area_size(self, addr):
		return self.lv1_query_logial_partition_address_region_info(addr)[1]

	def lv1_gpu_fifo_init(self, ctx, get, put, ref):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_FIFO_INIT, get, put, ref, 0)
	def lv1_gpu_display_sync(self, ctx, head, mode):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC, head, mode, 0, 0)
	def lv1_gpu_display_flip(self, ctx, head, ddr_offset):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP, head, ddr_offset, 0, 0)
	def lv1_gpu_fb_setup(self, ctx, xdr_lpar, xdr_size, ioif_offset):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_FB_SETUP, xdr_lpar, xdr_size, ioif_offset, 0)
	def lv1_gpu_fb_blit(self, ctx, ddr_offset, ioif_offset, sync_width, pitch):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_FB_BLIT, ddr_offset, ioif_offset, sync_width, pitch)
	def lv1_gpu_fb_close(self, ctx):
		self.lv1_gpu_context_attribute(ctx, L1GPU_CONTEXT_ATTRIBUTE_FB_CLOSE, 0, 0, 0, 0)

class CompileError(Exception):
	pass

class CCode(object):
	def __init__(self, proxy, source_code, addr=0x100000):
		self.p = proxy
		prefix = "bin/powerpc64-linux-"
		gcc_bin = os.path.join(os.environ['PS3DEV'], prefix + "gcc")
		netrpc_dir = os.path.dirname(__file__)
		self.source_code = '#include "c_head.h"\n' + source_code
		tmpdir = tempfile.mkdtemp()
		binfile = os.path.join(tmpdir, "blob.bin")
		gcc_args = [
			gcc_bin,
			"-Os", "-Wall",
			"-Wl,-T,%s"%os.path.join(netrpc_dir, "c_link.ld"),
			"-Wl,--section-start=.hdr=0x%x"%addr,
			"-I%s"%netrpc_dir,
			"-mbig-endian", "-mcpu=cell", "-m64",
			"-ffreestanding", "-nostartfiles", "-nostdlib",
			"-o", binfile,
			"-x", "c",
			"-"
		]

		proc = subprocess.Popen(gcc_args, stdin=subprocess.PIPE)
		proc.communicate(self.source_code)
		proc.wait()
		if proc.returncode != 0:
			try:
				os.remove(binfile)
			except:
				pass
			try:
				os.rmdir(tmpdir)
			except:
				pass
			raise CompileError("gcc returned %d exit status"%proc.returncode)
		
		bfd = open(binfile,"rb")
		self.bin = bfd.read()
		bfd.close()
		os.remove(binfile)
		os.rmdir(tmpdir)

		self.addr = addr
		self.desc_addr = struct.unpack(">Q", self.bin[:8])[0]
		self.uploaded = False

	def dump(self):
		print "Binary dump:"
		chexdump(self.bin, self.addr)
		print "Function descriptor at 0x%x"%self.desc_addr

	def upload(self):
		self.p.writemem(self.addr, self.bin)
		self.p.sync_before_exec(self.addr, len(self.bin))
		self.uploaded = True

	def __call__(self, *args):
		assert len(args) <= 8
		if not self.uploaded:
			self.upload()
		return self.p.call(self.desc_addr, *args)

if __name__ == "__main__":
	print "Connecting to PS3..."
	p = RPCClient("ps3")
	p.ping()
	print "Ping successful"
