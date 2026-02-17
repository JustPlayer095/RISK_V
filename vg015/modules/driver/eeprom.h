#ifndef EEPROM_SPI_H
#define EEPROM_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// SPI defines
#define SPI_CMD_WREN 0x06
#define SPI_CMD_WRITE 0x02
#define SPI_CMD_READ 0x03
#define SPI_CMD_RDSR 0x05
#define SPI_STATUS_BUSY 0x01

// I2C defines
#define I2C_ADDR 0x50
#define I2C_FREQ 100000  //  Standard mode 100kHz

#define EEPROM_PAGE_SIZE 64

void eeprom_init(void);

void eeprom_write_bytes(uint16_t addr, const uint8_t* data, size_t len);
void eeprom_read_bytes(uint16_t addr, uint8_t* data, size_t len);

bool eeprom_is_busy(void);
bool eeprom_had_error(void);

#endif // EEPROM_SPI_H

