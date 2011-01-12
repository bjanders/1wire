/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include <stdint.h>

void print_hex(uint8_t *data, int len);
void print_addr(uint8_t *addr);
uint8_t calc_crc8(uint8_t *data, int len);
float convert_temp(uint8_t *temp);
