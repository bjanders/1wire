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

/*
 * Three different vendor-specific command types exist to control and
 * communicate with the DS2490: Control, Communication, and Mode.
 *
 * Control, Communication and Mode commands, like USB core requests,
 * are communicated over the default control pipe at EP0.
 */


/*
 * Control commands are used to manage various device functions
 * including the processing of communication commands, buffer
 * clearing, and SW reset.
 */

#define CTL_RESET_DEVICE	0x0000
#define CTL_START_EXE		0x0001
#define CTL_RESUME_EXE		0x0002
#define CTL_HALT_EXE_IDLE	0x0003
#define CTL_HALT_EXE_DONE	0x0004
#define CTL_FLUSH_COMM_CMDS	0x0007
#define CTL_FLUSH_RCV_BUFFER	0x0008
#define CTL_FLUSH_XMT_BUFFER	0x0009
#define CTL_GET_COMM_CMDS	0x000A

/* 
 * Mode commands are used to establish the 1-Wire operational
 * characteristics of the DS2490 such as slew rate, low time, strong
 * pullup, etc.
 */

#define MOD_PULSE_EN		0x0000
#define MOD_SPEED_CHANGE_EN	0x0001
#define MOD_1WIRE_SPEED		0x0002
#define MOD_STRONG_PU_DURATION	0x0003
#define MOD_PULLDOWN_SLEWRATE	0x0004
#define MOD_PROG_PULSE_DURATION	0x0005
#define MOD_WRITE1_LOWTIME	0x0006
#define MOD_DSOW0_TREC		0x0007

/*
 * Communication commands are used for 1-Wire data and command I/O.
 */

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

/**************************************************************
 * Control commands
 *
 * Control commands are used to manage various device functions
 * including the processing of communication commands, buffer
 * clearing, and SW reset.
 **************************************************************/


/* owusb_ctrl_reset
 * 
 * Performs a hardware reset equivalent to the power-on reset. This
 * includes clearing all endpoint buffers and loading the Mode control
 * registers with their default values.
 */
 
int
owusb_ctl_reset(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESET_DEVICE, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_start_ext
 * 
 * Starts execution of Communication commands. This command is also
 * required to start the execution of Communication commands with an
 * IM (immediate execution control) bit set to logic 0.
 */ 

int
owusb_ctl_start_exe(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_START_EXE, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_resume_exe
 * 
 * Resume execution of a Communication command that was halted with
 * either of the owusb_ctl_halt_exe_idle() or
 * owusb_ctl_halt_exe_done() functions.
 */
 
int
owusb_ctl_resume_exe(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESUME_EXE, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_halt_exe_idle
 *
 * Halt the execution of the current Communication command after the
 * 1-Wire bus has returned to the idle state. Further Communication
 * command processing is stopped until owusb_ctl_resume_exe() is
 * called. This function, or the owusb_ctl_halt_exe_done() function,
 * is also used to terminate a strong pullup or programming pulse of
 * semi-infinite or infinite duration.
 */

int
owusb_ctl_halt_exe_idle(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_HALT_EXE_IDLE, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_halt_exe_done
 * 
 * Halt the execution of a Communication command after the current
 * command execution is complete. Further Communication command
 * processing is stopped until owusb_ctl_resume_exe() is called.  This
 * function, or the owusb_ctl_halt_exe_idle(), is also used to
 * terminate a strong pullup or programming pulse of semi-infinite or
 * infinite duration.
 */

int
owusb_ctl_halt_exe_done(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_HALT_EXE_DONE, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_flush_comm_cmds
 *
 * Clear all unexecuted Communication commands from the command
 * FIFO. The DS2490 must be in a halted state before this function can
 * be called.
 */

/* FIX: add check that it is in halted state? */
int
owusb_ctl_flush_comm_cmds(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_COMM_CMDS, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_fush_rcv_buffer
 *
 * Clear EP3 receive data FIFO (data from 1-Wire device). The DS2490
 * must be in a halted state before this function can be called.
 */

int
owusb_ctl_flush_rcv_buffer(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_RCV_BUFFER, 0x0000, NULL, 0, USB_TIMEOUT);
}


/* owusb_ctl_flush_xmt_buffer
 *
 * Clear EP2 transmit data FIFO (data to 1-Wire device). The DS2490
 * must be in a halted state before this function can be called.
 */

int
owusb_ctl_flush_xmt_buffer(owusb_device_t *d)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_FLUSH_XMT_BUFFER, 0x0000, NULL, 0, USB_TIMEOUT);
}

/* owusb_ctl_get_comm_cmds
 *
 * Retrieve unexecuted Communication commands and parameters from the
 * command FIFO. The DS2490 must be in a halted state before this
 * function can be called.  Unexecuted commands are returned over EP0
 * in the control transfer data phase. Host software is responsible
 * for determining the number of command/parameter bytes to be
 * returned and specifying the value in the wLength field of the
 * control transfer setup packet.  Commands/parameters are deleted
 * from the FIFO as they are transmitted to the host; the command
 * pointer used with the FIFO is updated as values are read. Any
 * commands/parameters that are not transferred remain in the FIFO and
 * will be processed when command execution resumes. If the wLength
 * value passed is larger than the number of command/parameter bytes,
 * the DS2490 will terminate the control transfer with a short data
 * packet.
 */

int
owusb_ctl_get_comm_cmds(owusb_device_t *d, uint8_t *cmds, int len)
{
	return usb_control_msg(d->handle, 0x40, CONTROL_CMD, CTL_RESUME_EXE, 0x0000, (char *)cmds, len, USB_TIMEOUT);
}

/**************************************************************
 * Mode commands
 * 
 * Mode commands are used to establish the 1-Wire operational
 * characteristics of the DS2490 such as slew rate, low time, strong
 * pullup, etc.
 **************************************************************/
 

/* owusb_mod_pulse_en
 * 
 * Enable a 1-Wire strong pullup pulse to 5V and/or +12V EPROM
 * programming pulse.
 *
 * The power-up default state for both strong pullup and
 * programming pulse is disabled.
 *
 * @param params
 * - PARAM_SPUE - Strong pullup enabled 
 * - PARAM_PRGE - Programming pulse enabled
 */

int
owusb_mod_pulse_en(owusb_device_t *d, int params)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PULSE_EN, params & 0x3, NULL, 0, USB_TIMEOUT);	
}


/* owusb_mod_speed_change_en
 *
 * Enable or disable a 1-Wire communication speed change. 
 * 
 * The power-up default state for speed change is disabled.
 *
 * @param enable 
 * - 0: disable
 * - 1: enable
 */

int
owusb_mod_speed_change_en(owusb_device_t *d, int enable)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_SPEED_CHANGE_EN, enable & 0x1, NULL, 0, USB_TIMEOUT);
}

/* owusb_mod_speed
 *
 * Set the speed of 1-Wire communication
 *
 * The power-up default communication speed is regular.
 *
 * @param speed PARAM_SPEED_REGULAR, PARAM_SPEED_FLEXIBLE or
 * PARAM_SPEED_OVERDIRVE
 */
int
owusb_mod_speed(owusb_device_t *d, int speed)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_1WIRE_SPEED, speed & 0x3, NULL, 0, USB_TIMEOUT);
}

/* owusb_mod_strong_pu_duration
 *
 * Set the time duration of a 1-Wire strong pullup. The time is
 * controlled with an unsigned 8-bit binary number between 0x00 and
 * 0xfe which specifies the duration in multiples of 16ms. A value of
 * 0x01 specifies 16ms, 0x02 equals 32ms, etc. A value of 0x00
 * specifies infinite duration. Parameter value 0xff is reserved and
 * will cause the device to deliver a pullup duration of <1µs. To
 * terminate an infinite duration pullup call either
 * owusb_ctl_halt_exe_done() or owusb_halt_exe_idle().
 * 
 * The power-up default strong pullup duration register value is
 * 512ms.
 *
 */

int
owusb_mod_strong_pu_duration(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_STRONG_PU_DURATION, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * Select the pulldown slew rate for 1-Wire bus Flexible Speed
 * operation. The pulldown slew rate power-up default value for
 * Flexible speed is 0.83V/us.
 *
 * @param slewrate PARAM_SLEWRATE_15Vus, PARAM_SLEWRATE_2_20Vus,
 * PARAM_SLEWRATE_1_65Vus, PARAM_SLEWRATE_1_37Vus,
 * PARAM_SLEWRATE_1_10Vus, PARAM_SLEWRATE_0_83Vus,
 * PARAM_SLEWRATE_0_70Vus, PARAM_SLEWRATE_0_55Vus
 *
 */

int
owusb_mod_pulldown_slewrate(owusb_device_t *d, int slewrate)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PULLDOWN_SLEWRATE, slewrate & 0xf, NULL, 0, USB_TIMEOUT);
}

/*
 * Set the time duration of a 1-Wire Programming Pulse. The time is
 * controlled with a an unsigned 8-bit binary number between 0x00 and
 * 0xfe specifying the duration in multiples of 8µs. A value of 0x00
 * stands for infinite duration. Parameter value 0xff is reserved and
 * will cause the device to deliver a pulse duration of <1µs. To
 * terminate an infinite duration programming pulse call either
 * owusb_ctl_halt_exe_done() or owusb_halt_exe_idle(). The power-up
 * default strong pullup duration is 512µs.
 */

int
owusb_mod_prog_pulse_duration(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_PROG_PULSE_DURATION, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * Select the Write-1 low time for 1-Wire bus Flexible speed
 * operation; The nominal Write-1 Low Time for Regular speed is 8us,
 * at Overdrive speed it is 1us. The Write-1 Low Time power-up
 * default value for Flexible speed is 12us.
 */
int
owusb_mod_write1_lowtime(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_WRITE1_LOWTIME, duration & 0xf, NULL, 0, USB_TIMEOUT);	
}

/*
 * Select the Data Sample Offset (tDSO) / Write-0 recovery (tW0R) time
 * (DSO/W0R) for 1-Wire bus Flexible Speed operation.
 */
int
owusb_mod_dsow0_trec(owusb_device_t *d, int duration)
{
	return usb_control_msg(d->handle, 0x40, MODE_CMD, MOD_DSOW0_TREC, duration & 0xf, NULL, 0, USB_TIMEOUT);	
}


/*******************************************************************
 * Communication commands
 *
 * Communication commands are used for 1-Wire data and command I/O.
 *******************************************************************/

/*
 * Change the State Register pulse duration value for either the +12V
 * programming pulse or strong pullup.
 *
 * @param params NTF, ICP, IM (TYPE)
 * @param type 1: programming pulse; 0: strong pullup
 */

int
owusb_com_set_duration(owusb_device_t *d, int params, int type, int duration)
{
	
	if (type) params |= PARAM_TYPE;
	params |= COM_SET_DURATION;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, params, duration & 0xff, NULL, 0, USB_TIMEOUT);
}

/*
 * Temporarily pull the 1-Wire bus to +12V in order to program an
 * EPROM device or to generate a strong pullup to 5V in order to
 * provide extra power for an attached iButton device, e.g.,
 * temperature sensor or crypto iButton.
 *
 * @param params F, NTF, ICP, TYPE, IM (TYPE)
 * @param type 1: programming pulse; 0: strong pullup
 */

int
owusb_com_pulse(owusb_device_t *d, int params, int type)
{
	if (type) params |= PARAM_TYPE;
	params |= COM_PULSE;
	return usb_control_msg(d->handle, 0x40, COMM_CMD, params, 0, NULL, 0, USB_TIMEOUT);
}

/*
 * Generate a reset pulse on the 1-Wire bus and to optionally change
 * the 1-Wire speed.
 * 
 * @param params PST, F, NTF, ICP, IM
 * @param present Reset until present
 * @param speed PARAM_SPEED_REGULAR, PARAM_SPEED_FLEXIBLE,
 * PARAM_SPEED_OVERDIRVE
 * 
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

/*
 * Initialize DS2490 device
 * 
 * i -- DS2490 count
 * dev -- USB device
 * 
 * Returns: 0 on success, < 0 on failure
 */

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


/*
 * Initalize the owusb library. This function must be called before any
 * other function.
 *
 * Returns: 0 on success, < 0 on failure
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

/*
 * Finalize the owusb library.
 */

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
	return dev->interrupt_data[STATE_DATA_IN_BUFFER_STATUS];
}

int
owusb_isidle(owusb_device_t *dev)
{
	return dev->interrupt_data[STATE_STATUS_FLAGS] & 0x20;
}

uint16_t
owusb_result(owusb_device_t *dev)
{
	int i;
	uint16_t result = 0;

	for (i = 16; i < dev->interrupt_len; i++) {
		if (dev->interrupt_data[STATE_COMBYTE2] == RESULT_DETECT) {
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
	printf("Enable Flags: %02x\n", data[STATE_ENABLE_FLAGS]);
	printf("1-Wire speed: %s\n", ow_speed[data[STATE_1WIRE_SPEED]]);
	printf("Strong Pullup Duration: %dms\n", data[STATE_SPU_DURATION] * 16);
	printf("Programming Pulse: %dus\n", data[STATE_PROG_PULSE_DURATION] * 8);
	printf("Pulldown Slew Rate: %s\n", ow_slew_rate[data[STATE_PULLDOWN_SLEW_RATE_CTRL]]);
	printf("Write-1 Low Time: %dus\n", data[STATE_WRITE1_LOW_TIME] + 8);
	printf("Data Sample Offset: %dus\n", data[STATE_DSO] + 3);
	/* byte 7 reserved */
	printf("Status: %02x\n", data[STATE_STATUS_FLAGS]);
	printf("Com command: %02x%02x\n", data[STATE_COMBYTE2], data[STATE_COMBYTE1]);
	printf("Comstat: %d bytes\n", data[STATE_COMBUFFER_STATUS]);
	printf("Dataout: %d bytes\n", data[STATE_DATA_OUT_BUFFER_STATUS]);
	printf("Datain: %d bytes\n", data[STATE_DATA_IN_BUFFER_STATUS]);
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
	int r;

	memset(zeros, 0, 8);
	r = owusb_write(dev, zeros, 8);
	if (r < 0) {
	  return r;
	}
	/* no discrepancy, no access, no device limit */
	r = owusb_com_search_access(dev, PARAM_F | PARAM_RST | PARAM_IM, 0, 1, 0, type);
	if (r < 0) {
	  return r;
	}
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

/* Return 1 if more devices, 0 if none */

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
	r = owusb_write(dev, dev->discrepancy, 8);
	if (r < 0) {
	  return 0;
	}
	  
	/* discrepancy, access, 1 device */
	r = owusb_com_search_access(dev, PARAM_F | PARAM_RST | PARAM_IM, 1, 1, 1, dev->search_cmd);
	if (r < 0) {
	  return 0;
	}
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
			if (set) {
				dev->discrepancy[i] = disc[i] & disc[i + 8];
			} else  {
				dev->discrepancy[i] = 0;
			}
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
