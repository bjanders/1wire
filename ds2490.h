/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include <stdint.h>

/*
 * Some findings:
 * Bit 0x8000 is always zero
 * Bits 0x4000 and 0x0008 are command specific options
 * Bits 0x2000 to 0x0100 and 0x0001 have constant meaning
 * Bits 0x0080 to 0x0010 are the commands
 * The meaning of bits 0x0004 and 0x0002 are unknown
 *  (seems to correlate to the wIndex usage: 
 *   00: not used; 01: byte 1 used; 10: bytes 1 and 2 used)
 */

/* 1-wire reset */
#define PARAM_PST	0x4000 /* Reset until presence */
/* search access */
#define PARAM_RTS	0x4000 /* Return discrepancy info */
#define PARAM_SM  	0x0008 /* Search for ROMs, no access */
/* bit I/O */
#define PARAM_CIB       0x4000 /* Prevent strong pullup if SPU and readback is 1 */
#define PARAM_D         0x0008 /* Bit to write */
/* set duration, pulse */
#define PARAM_TYPE	0x0008 /* 1: Programming pulse, 0: strong pullup */ 
/* match access */
#define PARAM_SE	0x0008 /* Enable speed change */
/* read redirect page */
#define PARAM_CH        0x0008 /* Follows the chain if the page is redirected */
/* write SRAM page, write EPROM, read CRC prot page */
#define PARAM_DT        0x2000 /* Activate CRC generator */
#define PARAM_PS        0x4000 /* Reduce preamble from 3 to 2 bytes */
/* write EPROM */
#define PARAM_Z         0x0008 /* Check zero bit writes only */
/* do & release */
#define PARAM_R         0x0008 /* Perform write (0) or read (1) function */


#define PARAM_SPU       0x1000 /* Strong pullup after command */
#define PARAM_F		0x0800 /* Clear buffer on error */
#define PARAM_NTF	0x0400 /* Result feedback */
#define PARAM_ICP	0x0200 /* Not last one of macro */
#define PARAM_RST	0x0100 /* Reset before executing command */
#define PARAM_IM 	0x0001 /* Immediate execution */


#define INTERRUPT_DATA_LEN 32


#define RESULT_DETECT 0xa5 /* 1-Wire Device detected */ 
#define RESULT_XDETECT 0x0100
#define RESULT_EOS 0x80 /* Search access ended sooner than expected */
#define RESULT_RDP 0x40 /* Page redirect */
#define RESULT_CRC 0x20 /* CRC error */
#define RESULT_CMP 0x10 /* Compare failed */
#define RESULT_VPP 0x08 /* 12V not seen */
#define RESULT_APP 0x04 /* Alarming presence pulse */
#define RESULT_SH  0x02 /* Short cut */
#define RESULT_NRS 0x01 /* No response */

#define STATE_EP0F 0x80
#define STATE_IDLE 0x20
#define STATE_HALT 0x10
#define STATE_PMOD 0x08
#define STATE_12VP 0x04
#define STATE_PRGA 0x02
#define STATE_SPUA 0x01


typedef struct owusb_device {
	struct usb_device *device;
	struct usb_dev_handle *handle;
	int timeout;
	uint8_t interrupt_data[INTERRUPT_DATA_LEN];
	int interrupt_len;
	int interrupt_count;
	int setting; /* 0-3, see p. 11 */
	uint8_t discrepancy[8];
	int search_stop;
	uint8_t search_cmd;
	uint8_t last_bit;
	uint8_t last_byte;
} owusb_device_t;

extern owusb_device_t owusb_devs[];
extern int owusb_dev_count;

int owusb_ctl_reset(owusb_device_t *d);
int owusb_ctl_start_exe(owusb_device_t *d);
int owusb_ctl_resume_exe(owusb_device_t *d);
int owusb_ctl_halt_exe_idle(owusb_device_t *d);
int owusb_ctl_halt_exe_done(owusb_device_t *d);
int owusb_ctl_flush_comm_cmds(owusb_device_t *d);
int owusb_ctl_flush_rcv_buffer(owusb_device_t *d);
int owusb_ctl_flush_xmt_buffer(owusb_device_t *d);
int owusb_ctl_get_comm_cmds(owusb_device_t *d, uint8_t *cmds, int len);

int owusb_mod_pulse_en(owusb_device_t *d, int params);
int owusb_mod_speed_change_en(owusb_device_t *d, int enable);
int owusb_mod_speed(owusb_device_t *d, int speed);
int owusb_mod_strong_pu_duration(owusb_device_t *d, int duration);
int owusb_mod_pulldown_slewrate(owusb_device_t *d, int slewrate);
int owusb_mod_prog_pulse_duration(owusb_device_t *d, int duration);
int owusb_mod_write1_lowtime(owusb_device_t *d, int duration);
int owusb_mod_dsow0_trec(owusb_device_t *d, int duration);

int owusb_com_set_duration(owusb_device_t *d, int params, int type, int duration);
int owusb_com_pulse(owusb_device_t *d, int params, int type);
int owusb_com_reset(owusb_device_t *d, int params, int present, int speed);
int owusb_com_bit_io(owusb_device_t *d, int params, int bit);
int owusb_com_byte_io(owusb_device_t *d, int params, uint8_t byte);
int owusb_com_block_io(owusb_device_t *d, int params, int len);
int owusb_com_match_access(owusb_device_t *d, int params, int speed, uint8_t cmd);
int owusb_com_read_straight(owusb_device_t *d, int params, int writelen, int readlen);
int owusb_com_do_and_release(owusb_device_t *d, int params, int len);
int owusb_com_set_path(owusb_device_t *d, int params, int len);
int owusb_com_write_sram_page(owusb_device_t *d, int params, int len);
int owusb_com_write_eprom(owusb_device_t *d, int params, int len);
int owusb_com_read_crc_prot_page(owusb_device_t *d, int params, int page_count, int page_size);
int owusb_com_read_redirect_page(owusb_device_t *d, int params, int page_number, int page_size);
int owusb_com_search_access(owusb_device_t *d, int params, int discrepancy, int noaccess, int device_count, int cmd);

int owusb_init(void);
void owusb_fini(void);

int  owusb_search_all(owusb_device_t *dev, uint8_t *data, int len);
int  owusb_search(owusb_device_t *dev, uint8_t type, uint8_t *data, int len);
void owusb_interrupt_read(owusb_device_t *dev);
int  owusb_write(owusb_device_t *dev, const uint8_t *data, int len);
int  owusb_read(owusb_device_t *dev, uint8_t *data, int len);
void owusb_wait_until_idle(owusb_device_t *dev);
void owusb_wait_for_presence(owusb_device_t *dev);
int  owusb_datain(owusb_device_t *dev);
int  owusb_isidle(owusb_device_t *dev);
uint16_t owusb_result(owusb_device_t *dev);
void owusb_print_state(owusb_device_t *dev);
void owusb_print_result(owusb_device_t *dev);
int  owusb_cmd(owusb_device_t *dev, uint8_t* addr, uint8_t cmd, uint8_t *out, int outlen);
int  owusb_write_byte(owusb_device_t *dev, uint8_t byte);
int  owusb_read_bit(owusb_device_t *dev);
uint16_t owusb_reset(owusb_device_t *dev);
int owusb_block_io(owusb_device_t *dev, const uint8_t *writedata, int writedatalen,  uint8_t *readdata, int readdatalen,  int reset, int spu);
int owusb_presence_detect(owusb_device_t *dev);
int owusb_search_first(owusb_device_t *dev, uint8_t type, uint8_t *data);
int owusb_search_next(owusb_device_t *dev, uint8_t *data);
