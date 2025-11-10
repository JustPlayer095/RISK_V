#include <string.h>
#include <stdint.h>
#include "osdp_min.h"
#include "ccitt_crc16.h"
#include "../device/Include/K1921VG015.h"
#include "../device/Include/retarget.h"

#define OSDP_SOM 0x53

static uint8_t g_addr = 0x01;

typedef enum {
	st_wait_som = 0,
	st_wait_addr,
	st_wait_len_l,
	st_wait_len_h,
	st_receive_bytes
} rx_state_t;

static rx_state_t rx_state = st_wait_som;
static uint16_t   rx_expected_len = 0;
static uint16_t   rx_pos = 0;
static uint8_t    rx_buf[256];

static void osdp_send_blocking(const uint8_t *data, uint16_t len)
{
	for (uint16_t i = 0; i < len; i++) {
		while (RETARGET_UART->FR_bit.TXFF) { }
		RETARGET_UART->DR_bit.DATA = data[i];
	}
	while (!RETARGET_UART->FR_bit.TXFE) { }
}

static void osdp_build_and_send_ack(uint8_t seq)
{
	uint8_t tx[16];
	uint16_t dlen = 8; // header(5) + reply(1) + crc(2)
	uint16_t crc;
	uint16_t i = 0;
	// SOM
	tx[i++] = OSDP_SOM;
	// ADDR (MSB=1 для ответа)
	tx[i++] = (uint8_t)(g_addr | 0x80);
	// LEN LSB/MSB
	tx[i++] = (uint8_t)(dlen & 0xFF);
	tx[i++] = (uint8_t)((dlen >> 8) & 0xFF);
	// CTRL: тот же seq, CRC-бит установлен
	tx[i++] = (uint8_t)((seq & 0x03) | 0x04);
	// REPLY
	tx[i++] = osdp_ACK;
	// CRC16
	crc = ccitt_crc16_calc(OSDP_INIT_CRC16, tx, (uint16_t)(i));
	tx[i++] = (uint8_t)(crc & 0xFF);
	tx[i++] = (uint8_t)((crc >> 8) & 0xFF);
	osdp_send_blocking(tx, (uint16_t)i);
}

static void osdp_build_and_send_pdid(uint8_t seq)
{
	uint8_t tx[64];
	uint16_t i = 0;
	uint8_t pdid[14] = {
		'R','V','G', // условный вендор
		1,           // модель
		1,           // версия
		0x12,0x34,0x56,0x78, // серийный
		1,0,0       // FW: 1.0.0
	};
	uint16_t dlen = (uint16_t)(8 + sizeof(pdid));
	uint16_t crc;
	// Header
	tx[i++] = OSDP_SOM;
	tx[i++] = (uint8_t)(g_addr | 0x80);
	tx[i++] = (uint8_t)(dlen & 0xFF);
	tx[i++] = (uint8_t)((dlen >> 8) & 0xFF);
	tx[i++] = (uint8_t)((seq & 0x03) | 0x04);
	// Reply code
	tx[i++] = osdp_PDID;
	// Data
	memcpy(&tx[i], pdid, sizeof(pdid));
	i += (uint16_t)sizeof(pdid);
	// CRC
	crc = ccitt_crc16_calc(OSDP_INIT_CRC16, tx, i);
	tx[i++] = (uint8_t)(crc & 0xFF);
	tx[i++] = (uint8_t)((crc >> 8) & 0xFF);
	osdp_send_blocking(tx, i);
}

void osdp_init(uint8_t device_address)
{
	g_addr = (uint8_t)(device_address & 0x7F);
	rx_state = st_wait_som;
	rx_expected_len = 0;
	rx_pos = 0;
}

void osdp_on_rx_byte(uint8_t byte)
{
	switch (rx_state) {
	case st_wait_som:
		if (byte == OSDP_SOM) {
			rx_pos = 0;
			rx_buf[rx_pos++] = byte;
			rx_state = st_wait_addr;
		}
		break;
	case st_wait_addr:
		rx_buf[rx_pos++] = byte;
		rx_state = st_wait_len_l;
		break;
	case st_wait_len_l:
		rx_buf[rx_pos++] = byte;
		rx_expected_len = byte; // LSB
		rx_state = st_wait_len_h;
		break;
	case st_wait_len_h:
		rx_buf[rx_pos++] = byte;
		rx_expected_len |= ((uint16_t)byte << 8);
		if (rx_expected_len < 8 || rx_expected_len > sizeof(rx_buf)) {
			// некорректная длина — сброс
			rx_state = st_wait_som;
			break;
		}
		rx_state = st_receive_bytes;
		break;
	case st_receive_bytes:
		rx_buf[rx_pos++] = byte;
		if (rx_pos >= rx_expected_len) {
			// Проверка CRC
			if (osdp_crc_is_ok(rx_buf, rx_expected_len)) {
				uint8_t addr = (uint8_t)(rx_buf[1] & 0x7F);
				if (addr == g_addr || addr == 0x7F || addr == 0x00) {
					uint8_t ctrl = rx_buf[4];
					uint8_t seq = (uint8_t)(ctrl & 0x03);
					uint8_t cmd = rx_buf[5];
					// На широковещательный (0x7F) отвечать нельзя
					int should_reply = (addr != 0x7F);
					switch (cmd) {
					case osdp_POLL:
						if (should_reply) osdp_build_and_send_ack(seq);
						break;
					case osdp_ID:
						if (should_reply) osdp_build_and_send_pdid(seq);
						break;
					default:
						// Для прочих команд пока только ACK без данных
						if (should_reply) osdp_build_and_send_ack(seq);
						break;
					}
				}
			}
			// Готовы к следующему пакету
			rx_state = st_wait_som;
		}
		break;
	default:
		rx_state = st_wait_som;
		break;
	}
}


