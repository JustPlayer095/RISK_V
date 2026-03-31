#ifndef OSDP_MIN_H
#define OSDP_MIN_H

#include <stdint.h>

// коды команд/ответов OSDP
enum {
	osdp_POLL    = 0x60,
	osdp_ID      = 0x61, 
	osdp_CAP     = 0x62,
	osdp_ISTAT   = 0x65, 
	osdp_OSTAT   = 0x66,
	osdp_OUT     = 0x68,  // Output Control Command
	osdp_LED     = 0x69,
	osdp_COMSET  = 0x6E,
	// manufacturer specific
	osdp_MFG     = 0x80,
	// коды ответов
	osdp_ACK     = 0x40,
	osdp_NAK     = 0x41,
	osdp_ISTATR  = 0x49,
	osdp_OSTATR  = 0x4A,
	osdp_PDCAP   = 0x46,
	osdp_PDID    = 0x45, 
	osdp_COM     = 0x65,
};

void osdp_init(void);
void osdp_on_rx_byte(uint8_t byte);
// Вызывать раз в 1 мс (из таймера) для временного управления LED
void osdp_tick_1ms(void);

#endif


