# Copyright (C) Bjorn Andersson <bjorn@iki.fi> 

#from ow.usb import OwUsb
#__all__ = ['usb']
from owusb import OwUsb

import sys
import types
import struct

READ_ROM = '\x33'
MATCH_ROM = '\x55'
SKIP_ROM = '\xcc'
SEARCH_ROM = '\xf0'
COND_SEARCH_ROM = '\xec'

CONVERT_T = '\x44'
READ_SCRATCHPAD = '\xbe'
COPY_SCRATCHPAD = '\x48'
RECALL_EEPROM = '\xb8'
READ_POWER_SUPPLY = '\xb4'

READ_MEMORY = '\xf0'
READ_MEMORY_AND_COUNTER = '\xa5'

# Mapping from familiy code to OwDevice subclass
family = {}

class OwBus(OwUsb):
	def get_devices(self, cmd=SEARCH_ROM):
		devices = []
		for a in self.searchiter(cmd):
			# create instance based on device familiy code
			c =  family.get(ord(a[0]), OwDevice)
			devices.append(c(self, a, selected=True))
		return devices
      
	def skip_rom(self):
		self.block_io(SKIP_ROM, reset=True)
      	
	def convert_t(self):
		"""
		Send a Skip ROM command followed by a Convert T command
		"""
		self.block_io(SKIP_ROM + CONVERT_T, reset=True)
 
class OwDevice(object):
	family = 0
	def __init__(self, bus, address, selected=False):
		self.bus = bus
		self._address = address
		if selected and hasattr(self, "initstate"):
			self.initstate()

	def io(self, *l, **kw):
		return self.bus.block_io(*l, **kw)
			
	def cmd(self, cmd, *l, **kw):
		"""Sends a match ROM command to target this device"""
		msg = MATCH_ROM + self._address + cmd
		return self.io(msg, reset=True, *l, **kw)

	def match(self):
		msg = MATCH_ROM + self._address
		self.io(msg, reset=True)

	def address(self):
		rev = lambda s:reduce(lambda a,b:b+a,s,'')	
		fam = "%02x" % ord(self._address[0])
		csum = "%02x" % ord(self._address[7])
		return "%s.%s.%s" % (csum, "".join(map(lambda x: "%02x" % ord(x), rev(self._address[1:7]))), fam)

	def __repr__(self):
		return "<%s %s>" % (self.__class__.__name__, self.address())

class OwSerialNumber(OwDevice):
	family = 0x01

class OwSerialId(OwDevice):
	family = 0x81

class OwThermometer(OwDevice):
	family = 0x28
	# DS12B20

	# attributes:
	# - last temp
	# - last temp timestamp
	# - config:
	#   - templow
	#   - temphigh
	#   - resolution
	# - powersupply
	def __init__(self, *l, **kw):
		self._temp = None
		self._templow = None
		self._temphigh = None
		self._resolution = 0xffff
		self._power = None
		OwDevice.__init__(self, *l, **kw)

	def initstate(self):
		s = self.io("\xbe", 9)
		self._decode_scratchpad(s)
		
	def b2temp(self, t):
		return (struct.unpack("h", t)[0] & (self._resolution | 0xfffc)) * 0.0625

	def b2atemp(self, t):
		return  struct.unpack("b", t)[0]

	def b2res(self, r):
		return ord(r) >> 5

	def atemp2b(self, t):
		return struct.pack("b", t)
	
	def res2b(self, r):
		return (r << 5) & 0x7f
		
	def convert_t(self):
		"""Sends a Convert T command"""
		self.cmd(CONVERT_T)
		
	def write_scratchpad(self, temphigh=None, templow=None, res=None): 
		if temphigh is None:
			temphigh = self._temphigh
		if templow is None:
			templow = self._templow
		if res is None:
			res = self._resolution
		cmd = struct.pack("BbbB", 0x4e, temphigh, templow, self.res2b(res))
		self.cmd(cmd)

	def _decode_scratchpad(self, s):
		self._resolution = self.b2res(s[4])
		self._templow = self.b2atemp(s[3])
		self._temphigh = self.b2atemp(s[2])
		self._temp = self.b2temp(s[0:2])
		return self._temp, self._temphigh, self._templow, self._resolution

	def read_scratchpad(self): 
		# FIX: check CRC or raise exception
		s = self.cmd(READ_SCRATCHPAD, 9)
		return self._decode_scratchpad(s)

	def temp(self, convert=True):
		if (convert):
			self.convert_t()
			while (self.bus.read_bit() == 0):
				pass
		return self.read_scratchpad()[0]
	
	def copy_scratchpad(self):
		self.cmd(COPY_SCRATCHPAD)

	def recall_eeprom(self):
		self.cmd(RECALL_EEPROM)

	def read_power_supply(self):
		return self.cmd(READ_POWER_SUPPLY)

class OwCounter(OwDevice):
	family = 0x1d

	def read_memory(self, address, len):
		cmd = READ_MEMORY + struct.pack("<H", address)
		return self.cmd(cmd, len)
	
	def read_memory_and_counter(self, address=0x01c0, len=42):
		cmd = READ_MEMORY_AND_COUNTER + struct.pack("<H", address)
		return self.cmd(cmd, len)

class OwAddressableSwitch(OwDevice):
	family = 0x05
	def __init__(self, *l, **kw):
		self._on = None
		OwDevice.__init__(self, *l, **kw)
		
	def initstate(self):
		self._on = not self.bus.read_bit()

	def ison(self):
		return self._on

	def toggle(self):
		self.match()
		self._on = not self._on
		
	def on(self):
		if not self._on:
			self.toggle()

	def off(self):
		if self._on:
			self.toggle()
		



for t in globals().values():
	if type(t) is types.TypeType and issubclass(t, OwDevice):
		family[t.family] = t
		
