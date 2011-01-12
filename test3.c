/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include "util.h"
#include "ds2490.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void
print_devs(uint8_t *devs, int len)
{
	int devcount;
	int i;

	devcount = len / 8;
	for (i = 0; i < devcount; i++) {
		print_addr(&devs[i * 8]);
	}
}

int
main(void)
{
	uint8_t owdevs[32][8];
	owusb_device_t *dev;
	int len, i;
	uint8_t in[9];
	uint8_t out[256];


	if ((i = owusb_init()) != 0) {
		printf("Failed to initialize: %d\n", i);
		return -1;
	}
	dev = &owusb_devs[0];
	for (i = owusb_search_first(dev, 0xf0, (uint8_t *)owdevs); i; 
	     i = owusb_search_next(dev, (uint8_t *)owdevs)) {
		print_addr((uint8_t *)owdevs);
	}
	return 0;
}

