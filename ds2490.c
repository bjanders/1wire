/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include <usb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ds2490.h"

#define VENDOR_MAXIM 0x04FA
#define PRODUCT_2490 0x2490
#define MAX_USBDEVS 4
#define MAX_OWDEVS 128
#define USB_TIMEOUT 5000
#define DS2490_FIFOSIZE 128

#define USB_ALT_INTERFACE 1
#define EP3 3

#define CONTROL_CMD 0x00
#define COMM_CMD    0x01
#define MODE_CMD    0x02


#define REGULAR_RESET_US  1096
#define REGULAR_SLOT_US   86
#define OVERDRIVE_SLOT_US 10
#define FLEXIBLE_SLOT_US  70

#define REGULAR_BPS    1000000 / 68
#define OVERDRIVE_BPS  1000000 / 10
#define FLEXIBLE_BPS   1000000 / 79

#define CTL_RESET_DEVICE	0x0000
#define CTL_START_EXE		0x0001
#define CTL_RESUME_EXE		0x0002
#define CTL_HALT_EXE_IDLE	0x0003
#define CTL_HALT_EXE_DONE	0x0004
#define CTL_FLUSH_COMM_CMDS	0x0007
#define CTL_FLUSH_RCV_BUFFER	0x0008
#define CTL_FLUSH_XMT_BUFFER	0x0009
#define CTL_GET_COMM_CMDS	0x000A

#define MOD_PULSE_EN		0x0000
#define MOD_SPEED_CHANGE_EN	0x0001
#define MOD_1WIRE_SPEED		0x0002
#define MOD_STRONG_PU_DURATION	0x0003
#define MOD_PULLDOWN_SLEWRATE	0x0004
#define MOD_PROG_PULSE_DURATION	0x0005
#define MOD_WRITE1_LOWTIME	0x0006
#define MOD_DSOW0_TREC		0x0007

#define COM_SET_DURATION        0x12
#define COM_BIT_IO              0x20
#define COM_PULSE               0x30
#define COM_RESET               0x42
#define COM_BYTE_IO             0x52
#define COM_MATCH_ACCESS        0x64
#define COM_BLOCK_IO            0x74
#define COM_READ_STRAIGHT       0x80
#define COM_DO_AND_RELEASE      0x92
#define COM_SET_PATH            0xA2
#define COM_WRITE_SRAM_PAGE     0xB2
#define COM_WRITE_EPROM         0xC4
#define COM_READ_CRC_PROT_PAGE  0xD4
#define COM_READ_REDIRECT_PAGE  0xE4
#define COM_SEARCH_ACCESS       0xF4


#define PARAM_PRGE 0x01
#define PARAM_SPUE 0x02

#define PARAM_1WIRE_SPEED_REGULAR   0x0
#define PARAM_1WIRE_SPEED_FLEXIBLE  0x1
#define PARAM_1WIRE_SPEED_OVERDIRVE 0x2



owusb_device_t owusb_devs[MAX_USBDEVS];
int owusb_dev_count = 0;


const char *ow_speed[] = { 
	"Regular", 
	"Flexible", 
	"Overdrive" 
};

const char *ow_slew_rate[] = {
	"15V/us", 
	"2.20V/us",
	"1.65Vus",
	"1.37V/us",
	"1.10V/us",
	"0.83V/us",
	"0.70V/us",
	"0.55V/us"
};

/*
 * Control commands
 */

int
owusb_ctl_reset(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESET_DEVICE, 0x0000, NULL, 0, USB_TIMEOUT);
}

int
owusb_ctl_start_exe(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_START_EXE, 0x0000, NULL, 0, USB_TIMEOUT);
}

int
owusb_ctl_resume_exe(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESUME_EXE, 0x0000, NULL, 0, USB_TIMEOUT);
}

int
owusb_ctl_halt_exe_idle(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_HALT_EXE_IDLE, 0x0000, NULL, 0, USB_TIMEOUT);
}

int
owusb_ctl_halt_exe_done(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_HALT_EXE_DONE, 0x0000, NULL, 0, USB_TIMEOUT);
}

/* The DS2490 must be in a halted state before this command can be processed */
/* FIX: add check that it is in halted state? */
int
owusb_ctl_flush_comm_cmds(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_COMM_CMDS, 0x0000, NULL, 0, USB_TIMEOUT);
}

/* The DS2490 must be in a halted state before this command can be processed */
int
owusb_ctl_flush_rcv_buffer(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_RCV_BUFFER, 0x0000, NULL, 0, USB_TIMEOUT);
}

/* The DS2490 must be in a halted state before this command can be processed */
int
owusb_ctl_flush_xmt_buffer(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_XMT_BUFFER, 0x0000, NULL, 0, USB_TIMEOUT);
}

/* The DS2490 must be in a halted state before this command can be processed */
int
owusb_ctl_get_comm_cmds(owusb_device_t *d, uint8_t *cmds, int len)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESUME_EXE, 0x0000, (char *)cmds, len, USB_TIMEOUT);
}

/*
 * Mode commands
 */


/*
 * params:
 * - PARAM_SPUE - Strong pullup enabled
 * - PARAM_PRGE - Programming pulse enabled
 */

int
owusb_mod_pulse_en(owusb_device_t *d, int params)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PULSE_EN, params & 0x3, NULL, 0, USB_TIMEOUT);	
}


int
owusb_mod_speed_change_en(owusb_device_t *d, int enable)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_SPEED_CHANGE_EN, enable & 0x1, NULL, 0, USB_TIMEOUT);
}

int
owusb_mod_speed(owusb_device_t *d, int speed)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_1WIRE_SPEED, speed & 0x3, NULL, 0, USB_TIMEOUT);
}

int
owusb_mod_strong_pu_duration(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_STRONG_PU_DURATION, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

int
owusb_mod_pulldown_slewrate(owusb_device_t *d, int slewrate)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PULLDOWN_SLEWRATE, slewrate & 0xf, NULL, 0, USB_TIMEOUT);
}

int
owusb_mod_prog_pulse_duration(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PROG_PULSE_DURATION, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

int
owusb_mod_write1_lowtime(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_WRITE1_LOWTIME, duration & 0xf, NULL, 0, USB_TIMEOUT);	
}

int
owusb_mod_dsow0_trec(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_DSOW0_TREC, duration & 0xf, NULL, 0, USB_TIMEOUT);	
}


/*
 * Communication commands
 */

/*
 * params: NTF, ICP, IM (TYPE)
 * type: 1: programming pulse; 0: strong pullup
 */

int
owusb_com_set_duration(owusb_device_t *d, int params, int type, int duration)
{
	
	if (type) params |= PARAM_TYPE;
	params |= COM_SET_DURATION;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, params, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * params: F, NTF, ICP, TYPE, IM (TYPE)
 * type: 1: programming pulse; 0: strong pullup
 */

int
owusb_com_pulse(owusb_device_t *d, int params, int type)
{
	if (type) params |= PARAM_TYPE;
	params |= COM_PULSE;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, params, 0, NULL, 0, USB_TIMEOUT);
}

/*
 * params: PST, F, NTF, ICP, IM
 * present: Reset until present
 * speed:
 */

int
owusb_com_reset(owusb_device_t *d, int params, int present, int speed)
{
	if (present) params |= PARAM_PST;
	params |= COM_RESET;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, params, speed & 0x3, NULL, 0, USB_TIMEOUT);
}

/* 
 * SPU, NTF, ICP, IM (CIB, D)
 */

int
owusb_com_bit_io(owusb_device_t *d, int params, int bit)
{
	if (bit) params |= PARAM_D;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_BIT_IO | params, 0, NULL, 0, USB_TIMEOUT);
}

/*
 * SPU, NTF, ICP, IM
 */
int
owusb_com_byte_io(owusb_device_t *d, int params, uint8_t byte)
{
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_BYTE_IO | params, byte & 0xff, NULL, 0, USB_TIMEOUT);
}

/* 
 * SPU, NTF, ICP, RST, IM
 */
int
owusb_com_block_io(owusb_device_t *d, int params, int len)
{
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_BLOCK_IO | params, len, NULL, 0, USB_TIMEOUT);
}

/*
 * params: NTF, ICP, RST, SE, IM
 * speed: 0-2
 * cmd: 0x55 (match rom), 0x69 (overdrive match rom)
 * 
 */
int
owusb_com_match_access(owusb_device_t *d, int params, int speed, uint8_t cmd)
{
	int index = speed << 8 & cmd;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_MATCH_ACCESS | params, index, NULL, 0, USB_TIMEOUT);
}

/*
 * params: NTF, ICP, RST, IM
 */

int
owusb_com_read_straight(owusb_device_t *d, int params, int writelen, int readlen)
{
	int p = 0;

	if (params & PARAM_NTF) p |= 0x8;
	if (params & PARAM_ICP) p |= 0x4;
	if (params & PARAM_RST) p |= 0x2;
	if (params & PARAM_IM)  p |= 0x1;

	p |= writelen << 8;

	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_READ_STRAIGHT | p, readlen, NULL, 0, USB_TIMEOUT);
}

/*
 * params: SPU, F, NTF, ICP, R, IM
 */
int
owusb_com_do_and_release(owusb_device_t *d, int params, int len)
{
	int cmd = 0x6000 | COM_DO_AND_RELEASE | params;

	return usb_control_msg(d->handle, 0x40, COMM_CMD, cmd, len & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * params: F, NTF, ICP, RST, IM
 */
int
owusb_com_set_path(owusb_device_t *d, int params, int len)
{
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_SET_PATH | params, len & 0xff, NULL, 0, USB_TIMEOUT);
}

/* 
 * params: PS, DT, F, NTF, ICP, IM
 */
int
owusb_com_write_sram_page(owusb_device_t *d, int params, int len)
{
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_WRITE_SRAM_PAGE | params, len & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * params: DT, F, NTF, ICP, Z, IM
 */
int
owusb_com_write_eprom(owusb_device_t *d, int params, int len)
{
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_WRITE_EPROM | params, len, NULL, 0, USB_TIMEOUT);
}

/* 
 * params: PS, DT, F, NTF, ICP, IM
 */
int
owusb_com_read_crc_prot_page(owusb_device_t *d, int params, int page_count, int page_size)
{
	int index = page_count << 8 | page_size;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_READ_CRC_PROT_PAGE | params, index, NULL, 0, USB_TIMEOUT);
}

/*
 * params: F, NTF, ICP, CH, IM
 */
int
owusb_com_read_redirect_page(owusb_device_t *d, int params, int page_number, int page_size)
{
	int value = COM_READ_REDIRECT_PAGE | 0x2100 | params;
	int index = page_number << 8 | page_size;

	return usb_control_msg(d->handle, 0x40, COMM_CMD, value, index, NULL, 0, USB_TIMEOUT);
}

/*
 * params:  F, NTF, ICP, RST, IM (RTS, SM)
 * cmd: 0xf0 (search ROM), 0xec (conditional search ROM)
 */

int
owusb_com_search_access(owusb_device_t *d, int params, int discrepancy, int noaccess, int device_count, int cmd)
{
	int index = device_count << 8 | cmd;
	
	if (discrepancy) params |= PARAM_RTS;
	if (noaccess) params |= PARAM_SM;

	return usb_control_msg(d->handle, 0x40, COMM_CMD, COM_SEARCH_ACCESS | params, index, NULL, 0, USB_TIMEOUT);
}

static int
owusb_init_dev(int i, struct usb_device *dev)
{
	struct usb_dev_handle *h;
	
	h = usb_open(dev);
	if (h <= 0) {
		return -1;
	}

	if (usb_set_configuration(h, 1) < 0) {
		return -2;
	}

	if (usb_claim_interface(h, 0) < 0) {
		return -3;
	}

	if (usb_set_altinterface(h, USB_ALT_INTERFACE) < 0) {
		return -4;
	}

	owusb_devs[i].device = dev;
	owusb_devs[i].handle = h;
	owusb_devs[i].timeout = USB_TIMEOUT;
	owusb_devs[i].interrupt_len = 0;
	owusb_devs[i].setting = USB_ALT_INTERFACE;

	owusb_ctl_reset(&owusb_devs[i]);
	return 0;
}


/*
 * High level functions
 */

int
owusb_init(void)
{
	int b, d, e;
	struct usb_bus *bus;
	struct usb_device *dev;

	usb_init();
	b = usb_find_busses();
	d = usb_find_devices();

	owusb_dev_count = 0;
	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == VENDOR_MAXIM &&
			    dev->descriptor.idProduct == PRODUCT_2490) {
				if ((e = owusb_init_dev(owusb_dev_count++, dev)) != 0) {
					return e;
				}
			}
		}
	}
	return 0;
}

void
owusb_fini(void)
{

}


void
owusb_interrupt_read(owusb_device_t *dev)
{
	dev->interrupt_len = usb_interrupt_read(dev->handle, 1, 
						(char *)dev->interrupt_data, INTERRUPT_DATA_LEN, 
						dev->timeout);
	dev->interrupt_count++;
}


int
owusb_write(owusb_device_t *dev, const uint8_t *data, int len)
{
	return usb_bulk_write(dev->handle, 2, (char *)data, len, dev->timeout);
}

int
owusb_read(owusb_device_t *dev, uint8_t *data, int len)
{
	return usb_bulk_read(dev->handle, 3, (char *)data, len, dev->timeout);
}

/* Wait for a command to complete */
void
owusb_wait_until_idle(owusb_device_t *dev)
{
	owusb_interrupt_read(dev);
	while (!owusb_isidle(dev)){
		owusb_interrupt_read(dev);	
	};
}

void
owusb_wait_for_presence(owusb_device_t *dev)
{
	owusb_interrupt_read(dev);
	while (owusb_result(dev) != 0xa5) {
		owusb_interrupt_read(dev);
	}
}


int
owusb_datain(owusb_device_t *dev)
{
	return dev->interrupt_data[13];
}

int
owusb_isidle(owusb_device_t *dev)
{
	return dev->interrupt_data[8] & 0x20;
}

uint16_t
owusb_result(owusb_device_t *dev)
{
	int i;
	uint16_t result = 0;

	for (i = 16; i < dev->interrupt_len; i++) {
		if (dev->interrupt_data[0x10] == RESULT_DETECT) {
			result |= RESULT_XDETECT;
		} else {
			result |= dev->interrupt_data[i];
		}
	}
	return result;
}

int
owusb_presence_detect(owusb_device_t *dev)
{
	uint16_t r;

	owusb_interrupt_read(dev);
	r = owusb_result(dev);
	return r & RESULT_XDETECT;
}

void 
owusb_print_state(owusb_device_t *dev)
{
	uint8_t *data = dev->interrupt_data;

	printf("============================\n");
	printf("Enable Flags: %02x\n", data[0]);
	printf("1-Wire speed: %s\n", ow_speed[data[1]]);
	printf("Strong Pullup Duration: %dms\n", data[2] * 16);
	printf("Programming Pulse: %dus\n", data[3] * 8);
	printf("Pulldown Slew Rate: %s\n", ow_slew_rate[data[4]]);
	printf("Write-1 Low Time: %dus\n", data[5] + 8);
	printf("Data Sample Offset: %dus\n", data[6] + 3);
	/* byte 7 reserved */
	printf("Status: %02x\n", data[8]);
	printf("Com command: %02x%02x\n", data[10], data[9]);
	printf("Comstat: %d bytes\n", data[11]);
	printf("Dataout: %d bytes\n", data[12]);
	printf("Datain: %d bytes\n", data[13]);
	/* bytes 14 and 15 reserved */
	owusb_print_result(dev);
	printf("============================\n");
}

void
owusb_print_result(owusb_device_t *dev)
{
	int i;
	for (i = 0x10; i < dev->interrupt_len; i++) {
		printf("Result: %02x\n", dev->interrupt_data[i]);
	}
}

int
owusb_search(owusb_device_t *dev, uint8_t type, uint8_t *data, int len)
{
	uint8_t zeros[8];

	memset(zeros, 0, 8);
	owusb_write(dev, zeros, 8);
	/* no discrepancy, no access, no device limit */
	owusb_com_search_access(dev, PARAM_F | PARAM_RST | PARAM_IM, 0, 1, 0, type);
	/* Sleep for the reset and eventual first ROM to finish */
	usleep(REGULAR_RESET_US);
	/* 3 bits for each ROM bit */
	usleep(3 * 64 * FLEXIBLE_SLOT_US);
	owusb_interrupt_read(dev);
	while (!owusb_isidle(dev)) {
		/* If are not idle, then there is probably more ROMs to read */
		usleep(3 * 64 * FLEXIBLE_SLOT_US);
		owusb_interrupt_read(dev);
	}
	return owusb_read(dev, data, len);
}

int
owusb_search_all(owusb_device_t *dev, uint8_t *data, int len)
{
	return owusb_search(dev, 0xf0, data, len);
}

static int
highest_bit(uint8_t b) 
{
	int r = 0;
	while (b >>= 1) {
		r++;
	}
	return r;
}

uint8_t
compare(uint8_t disc, uint8_t addr)
{
	uint8_t mask = 0x80;
	uint8_t mask2 = 0xff;
	
	while (mask) {
		/*printf("%02x %02x\n", mask & disc, mask & addr);*/
		if (mask & disc && !(mask & addr)) {
			return (disc & addr & mask2) | mask;
		}
		mask >>= 1;
		mask2 >>= 1;
	}
	return 0;
}

int
owusb_search_next(owusb_device_t *dev, uint8_t *data) 
{
	uint8_t disc[16];
	int r;
	int i;
	uint8_t b;
	int set = 0;

	if (dev->search_stop) {
		return 0;
	}
	owusb_write(dev, dev->discrepancy, 8);
	/* discrepancy, access, 1 device */
	owusb_com_search_access(dev, PARAM_F | PARAM_RST | PARAM_IM, 1, 1, 1, dev->search_cmd);
	usleep(REGULAR_RESET_US + 3 * 64 * FLEXIBLE_SLOT_US + 100);
	/*owusb_interrupt_read(dev);*/
	r = owusb_read(dev, disc, 16);
	if (r < 8) {
		dev->search_stop = 1;
		return 0;
	}
	memcpy(data, disc, 8);
	if (r == 16) {
		for (i = 7; i >= 0; i--) {
			/*printf("disc: %02x\n", disc[i]);*/
			if (disc[i] && !set) {
				b = compare(disc[i + 8], disc[i]);
				if (b) {
					set = 1;
					dev->discrepancy[i] = b;
					continue;
				}
			}
			if (set)
				dev->discrepancy[i] = disc[i] & disc[i + 8];
			else 
				dev->discrepancy[i] = 0;
		}
	} else {
		dev->search_stop = 1;
	}
	return 1;
}

int
owusb_search_first(owusb_device_t *dev, uint8_t type, uint8_t *data) 
{
	dev->search_cmd = type;
	dev->search_stop = 0;
	memset(dev->discrepancy, 0, 8);
	dev->last_bit = 0;
	dev->last_byte = 0;
	return owusb_search_next(dev, data);
}


int
owusb_write_byte(owusb_device_t *dev, uint8_t byte)
{
	return owusb_com_byte_io(dev, PARAM_ICP | PARAM_IM, byte);
}

int
owusb_read_bit(owusb_device_t *dev)
{
	uint8_t bit;
	owusb_com_bit_io(dev, PARAM_IM, 1);
	owusb_read(dev, &bit, 1);
	return bit;
}

uint16_t
owusb_reset(owusb_device_t *dev)
{
	int r;
	r = owusb_com_reset(dev, PARAM_F | PARAM_IM | PARAM_NTF, 0, 0);
	return owusb_result(dev);
}

int
owusb_cmd(owusb_device_t *dev, uint8_t* addr, uint8_t cmd, uint8_t *out, int outlen)
{
	uint8_t cmdbuf[10];
	
	cmdbuf[0] = 0x55; /* Match ROM */
	memcpy(&cmdbuf[1], addr, 8);
	cmdbuf[9] = cmd;
	owusb_write(dev, cmdbuf, 10);
	owusb_com_read_straight(dev, PARAM_RST | PARAM_IM, 10, outlen);
	return owusb_read(dev, out, outlen);
}

int
owusb_block_io(owusb_device_t *dev, 
	       const uint8_t *writedata, int writedatalen, 
	       uint8_t *readdata, int readdatalen,
	       int reset, int spu) 
{
	uint8_t tmpbuf[DS2490_FIFOSIZE];
	int flags = PARAM_IM;
	int datalen = writedatalen + readdatalen;
	int sleeplen = 0;
	
	/* FIX: check writedatalen > tmpbuf */
	
	if (reset) { 
		flags |= PARAM_RST;
		sleeplen = 1096;
	}
	if (spu) flags |= PARAM_SPU;
	
	if (writedatalen) { 
		owusb_write(dev, writedata, writedatalen);
	}
	if (readdatalen) {
		memset(readdata, 0xff, readdatalen);
		owusb_write(dev, readdata, readdatalen);
	}

	owusb_com_block_io(dev, flags, datalen);
	sleeplen += datalen * 8 * FLEXIBLE_SLOT_US;
	usleep(sleeplen);
	owusb_read(dev, tmpbuf, DS2490_FIFOSIZE);
	if (writedatalen) {
		/* Verify that the same bits we wrote were seen on the wire */
		if (memcmp(tmpbuf, writedata, writedatalen) != 0) {
			return -1;
		}
	}
	if (readdatalen) {
		memcpy(readdata, &tmpbuf[writedatalen], readdatalen);
	}
	return 0;
}
