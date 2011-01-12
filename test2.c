/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include "ds2490.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


int
main(void)
{
	uint8_t owdevs[32][8];
	int devcount;
	owusb_device_t *dev;
	int len, i;
	uint8_t in[9];
	uint8_t out[256];


	if ((i = owusb_init()) != 0) {
		printf("Failed to initialize: %d\n", i);
		return -1;
	}
	dev = &owusb_devs[0];
	/*len = owusb_search_all(dev, (uint8_t *)owdevs, 32 * 8);*/
	len = owusb_search(dev, 0xf0, (uint8_t *)owdevs, 32 * 8);
	devcount = len / 8;
	for (i = 0; i < devcount; i++) {
		print_addr((uint8_t *)owdevs[i]);
	}
	printf("\n");
#if 0
	memset(out, 0, 16);
	out[0] = 0x05;
	owusb_write(dev, out, 8);
	owusb_com_search_access(dev,  PARAM_NTF|PARAM_IM | PARAM_RST | PARAM_F, 1, 1, 1, 0xf0);
	sleep(1);
	owusb_interrupt_read(dev);
	owusb_print_state(dev);
	owusb_read(dev, out, 16);
	print_hex(out, 16);
	return 0;
#endif	
	while (1) {
		owusb_com_reset(dev, PARAM_IM, 0, 0);
		owusb_write_byte(dev, 0xcc);
		owusb_write_byte(dev, 0x44);
		while (owusb_read_bit(dev) == 0);
		for (i = 0; i < devcount; i++) {
			if (owdevs[i][0] != 0x28) continue;
			out[0] = 0x55;
			memcpy(&out[1], owdevs[i], 8);
			out[9] = 0xbe;
			if (owusb_block_io(dev, out, 10, in, 9, 1, 0) != 0) {
				exit(1);
			}
			/*print_hex(in, 9);*/
			printf("%.4f\t", convert_temp(in));
		}
		printf("\n");
		sleep(60);
	}

	/*crc = calc_crc8(data, 7);
	  printf("CRC: %02x\n",  crc);*/

}
