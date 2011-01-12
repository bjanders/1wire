/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#define OWCMD_CONVERT_T		0x44
#define OWCMD_READ_SCRATCHPAD 	0xbe
#define OWCMD_WRITE_SCRATCHPAD	0x4e
#define OWCMD_COPY_SCRATCHPAD	0x48
#define OWCMD_RECALL_E2		0xb8
#define OWCMD_READ_PWRSUP	0xb4



/*
 * Function prototypes
 */

void printhex(uint8_t *data, int len);
void print_owusb_result(void);
void owusb_wait_until_idle(usb_dev_handle *h);


/*
 * Global variables
 */


uint8_t owdevs[MAX_OWDEVS][8];
int owdev_count = 0;



/*
 * Utility functions
 */


void
print_owdevs(void)
{
	int i;

	for (i = 0; i < owdev_count; i++) {
		printhex(owdevs[i], 8);
	}
}


void
print_temp(uint8_t *temp)
{
	int i; 
	float f;

	i = (temp[1] & 0x07) << 4 | (temp[0] >> 4);
	f = 0.0625 * (temp[0] & 0x0f);
	printf("Temp: %.4f\n", i + f);
}





/*
 * USB device function
 */


/*
 * Options:
 * PST - Reset until presence
 * F   - clear buffers on error
 * NTF - always generate result feedback
 * ICP - not last of a macro
 * SE  - speed change enable
 * IM  - execute immediately
 */
int
ow_reset(usb_dev_handle *h)
{
	int r;
	int cmd;

	cmd = PARAM_F | PARAM_IM | 0x42;

	r = usb_control_msg(h, 0x40, COMM_CMD, cmd, 0x0000, NULL, 0, USB_TIMEOUT);
	return r;
}

/*
 * NTF - always generate result feedback
 * ICP - not last of a macro
 * RST - 
 * IM  - execute immediately
 */
void
ow_read_temp(usb_dev_handle *h, uint8_t *rom)
{
	uint8_t buf[64];
	int r;
	int cmd;

	buf[0] = 0x55; /* MATCH ROM */
	buf[9] = 0xbe; 
	memcpy(&buf[1], rom, 8);
	owusb_write(h, buf, 10);

	cmd = 0x0080 | 0x0b | 0x0a00;
	r = usb_control_msg(h, 0x40, COMM_CMD, cmd, 0x0009, NULL, 0, USB_TIMEOUT);
	r = owusb_read(h, buf, 9);
	print_temp(buf);
}

/*
 * Options:
 * CIB - Prevent strong pullup if 1 bit read
 * SPU - Strong pullup after byte
 * NTF - always generate result feedback
 * ICP - not last of a macro
 * IM  - execute immediately
 */

void
ow_read_until_ready(usb_dev_handle *h)
{
	char data;
	int cmd;
	int r;
	int len;

	cmd = 0x20 | PARAM_IM | 0x08;
	while (1) {
		r = usb_control_msg(h, 0x40, COMM_CMD, cmd, 0x0000, NULL, 0, USB_TIMEOUT);
		len = usb_bulk_read(h, 3, &data, 1, USB_TIMEOUT);
		if (len != 1) exit(1);
		/*printf("len: %d, data: %d\n", len, data);*/
		if (data) {
			return;
		}
	}
}


/*
 * Options:
 * SPU - Strong pullup after byte
 * NTF - always generate result feedback
 * ICP - not last of a macro
 * IM  - execute immediately
 */

int
ow_send_byte(usb_dev_handle *h, uint8_t byte)
{
	int cmd;

	cmd = 0x52 | PARAM_ICP | PARAM_IM;
	return usb_control_msg(h, 0x40, COMM_CMD, cmd, byte, NULL, 0, USB_TIMEOUT);
}

void
ow_convert_temp(usb_dev_handle *h)
{
	int r;
	int cmd;
	char data;
	int len;

	/* Skip rom */
	ow_send_byte(h, 0xcc);
	/* Convert */
	ow_send_byte(h, 0x44);
	ow_read_until_ready(h);
}

int
read_owdevs(usb_dev_handle *h)
{
#define BUFSIZE 64
	int i;
	int len;
	uint8_t data[BUFSIZE];
	int total_len = 0;

	owdev_count = 0;

	owusb_interrupt_read(h);	
	while (!isidle() || datain()) {
		if (datain() >= 8) {
			len = usb_bulk_read(h, 3, (char *)data, BUFSIZE, USB_TIMEOUT);
			if (len < 0) {
				return -1;
			}
			memcpy((uint8_t *)owdevs + total_len, data, len);
			total_len += len;
		}
		owusb_interrupt_read(h);
	}
	owdev_count = total_len / 8;
	return 0;
}

/*
 * Options:
 * RTS - return discrepancy information
 * F   - clear buffers on error
 * NTF - always generate result feedback
 * ICP - not last of a macro
 * RST - insert reset before command
 * SM  - search for ROMs
 * IM  - execute immediately
 */
int 
ow_search(usb_dev_handle *h)
{
#define BUFSIZE 64
	unsigned char buf[BUFSIZE];
	int r;
	int cmd;

	/* Write zero ROM address to EP2 to get all devices */
	memset(buf, 0, 8);
	r = usb_bulk_write(h, 2, (char *)buf, 8, USB_TIMEOUT);
	if (r < 0) {
		return -1;
	}

	/* Search ROM command */
	cmd = PARAM_RST | PARAM_F | PARAM_SM | PARAM_IM | 0x00f4;
	r = usb_control_msg(h, 0x40, COMM_CMD, cmd, 0x00f0, NULL, 0, USB_TIMEOUT);
	if (r < 0) {
	       	return -1;
	}
	if (read_owdevs(h) < 0) {
		return -1;
	}
	return 0;
}



int 
main(int argc, char *argv[])
{
	int b;
	int d;
	int i;
	struct usb_bus *bus;
	struct usb_device *dev;
	struct usb_dev_handle *h;

	usb_init();
	b = usb_find_busses();
	printf("Busses %d\n", b);
	d = usb_find_devices();
	printf("Devices %d\n", d);

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {
			if (dev->descriptor.idVendor == VENDOR_MAXIM &&
			    dev->descriptor.idProduct == PRODUCT_2490) {
				printf("Found device %s\n", dev->filename);
				init_dev(dev_count++, dev);
			}
		}
	}
	printf("\n\n");
	h = usbhandles[0];
	owusb_reset(h);
	while (1) {
		owusb_wait_for_presence(h);
		printf("Presence detected\n");
	}

	ow_search(h);
	print_owdevs();
	while (1) {
		ow_reset(h);
		ow_convert_temp(h);
		for (i = 0; i < owdev_count; i++) {
			if (owdevs[i][0] == 0x28) {
				ow_read_temp(h, owdevs[i]);
			}
		}
		sleep(10);
	}
	print_state();
	return 0;
}
