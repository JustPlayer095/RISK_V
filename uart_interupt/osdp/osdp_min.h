#ifndef OSDP_MIN_H
#define OSDP_MIN_H

#include <stdint.h>

// коды команд/ответов OSDP
enum {
	osdp_POLL  = 0x60,
	osdp_ID    = 0x61,
	osdp_ACK   = 0x40,
	osdp_PDID  = 0x45
};

void osdp_init(uint8_t device_address);
void osdp_on_rx_byte(uint8_t byte);

#endif


