#ifndef EEPROM_SPI_H
#define EEPROM_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


// Параметры микросхемы: командами WREN/WRITE/READ/RDSR.
#define EEPROM_SPI_CMD_WREN 0x06
#define EEPROM_SPI_CMD_WRITE 0x02
#define EEPROM_SPI_CMD_READ 0x03
#define EEPROM_SPI_CMD_RDSR 0x05
#define EEPROM_SPI_STATUS_BUSY 0x01

// Размер страницы (для поадресной записи можно оставить 16).
#define EEPROM_SPI_PAGE_SIZE 16

void eeprom_spi_init(void);
void eeprom_spi_cs_low(void);
void eeprom_spi_cs_high(void);
uint8_t eeprom_spi_xfer(uint8_t byte);
// Ждём сброс BUSY в статусе; возвращаем false по таймауту (итерации)
bool eeprom_spi_wait_ready(uint32_t timeout_cycles);
uint8_t eeprom_spi_read_status(void);
// Простой loopback-тест (MOSi замкнуть на MISO). Возвращает true при совпадении.
bool eeprom_spi_loopback_test(uint8_t rx_out[3]);
// Тест WREN+SR: поднимает WEL и возвращает новый статус
uint8_t eeprom_spi_wren_and_status(void);
void eeprom_spi_write_bytes(uint16_t addr, const uint8_t* data, size_t len);
void eeprom_spi_read_bytes(uint16_t addr, uint8_t* data, size_t len);

#endif // EEPROM_SPI_H

